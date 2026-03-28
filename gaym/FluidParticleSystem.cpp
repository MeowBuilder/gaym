#include "stdafx.h"
#include "FluidParticleSystem.h"
#include "DescriptorHeap.h"
#include <cmath>
#include <cstring>
#include <algorithm>

// Static member definitions
ComPtr<ID3D12RootSignature> FluidParticleSystem::s_pRootSignature;
ComPtr<ID3D12PipelineState> FluidParticleSystem::s_pPSO;

// Internal pass CB layout (matches cbFluidPass in inline shader)
struct FluidPassCB
{
    XMFLOAT4X4 viewProj;
    XMFLOAT3   cameraRight; float padR;
    XMFLOAT3   cameraUp;    float padU;
};
static_assert(sizeof(FluidPassCB) == 96, "FluidPassCB size mismatch");

static constexpr float PI_F = 3.14159265358979323846f;

// Inline HLSL shader code for fluid particles
static const char* g_FluidShaderCode = R"(
    cbuffer cbFluidPass : register(b0)
    {
        matrix gViewProj;
        float3 gCameraRight; float gPadR;
        float3 gCameraUp;    float gPadU;
    };

    struct FluidParticleData
    {
        float3 position;
        float  size;
        float4 color;
    };
    StructuredBuffer<FluidParticleData> gParticles : register(t0);

    struct FluidVSOut
    {
        float4 pos   : SV_POSITION;
        float4 color : COLOR0;
        float2 uv    : TEXCOORD0;
    };

    // TRIANGLE_STRIP: 4 vertices per quad
    // 0=BL, 1=BR, 2=TL, 3=TR
    static const float2 kOffsets[4] = {
        { -0.5f, -0.5f },
        {  0.5f, -0.5f },
        { -0.5f,  0.5f },
        {  0.5f,  0.5f }
    };
    static const float2 kUVs[4] = {
        { 0.0f, 1.0f },
        { 1.0f, 1.0f },
        { 0.0f, 0.0f },
        { 1.0f, 0.0f }
    };

    FluidVSOut VS_Fluid(uint vertId : SV_VertexID, uint instId : SV_InstanceID)
    {
        FluidParticleData p = gParticles[instId];

        float2 offset = kOffsets[vertId];
        float2 uv     = kUVs[vertId];

        float3 worldPos = p.position
            + gCameraRight * offset.x * p.size
            + gCameraUp    * offset.y * p.size;

        FluidVSOut output;
        output.pos   = mul(float4(worldPos, 1.0f), gViewProj);
        output.color = p.color;
        output.uv    = uv;
        return output;
    }

    float4 PS_Fluid(FluidVSOut input) : SV_TARGET
    {
        // UV to [-1, 1]
        float2 centered = input.uv * 2.0f - 1.0f;
        float  d        = length(centered);

        // Soft disc alpha
        float alpha = 1.0f - smoothstep(0.5f, 1.0f, d);
        clip(alpha - 0.02f);

        // Inner glow boost
        float glow = 1.0f - smoothstep(0.0f, 0.4f, d);
        float3 col = input.color.rgb * (1.0f + glow * 0.8f);

        return float4(col, input.color.a * alpha);
    }
)";

// ============================================================================
// PRNG
// ============================================================================
float FluidParticleSystem::Rand01()
{
    m_Seed ^= m_Seed << 13;
    m_Seed ^= m_Seed >> 17;
    m_Seed ^= m_Seed << 5;
    return (float)(m_Seed & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

float FluidParticleSystem::RandRange(float lo, float hi)
{
    return lo + Rand01() * (hi - lo);
}

XMFLOAT3 FluidParticleSystem::RandInSphere(float radius)
{
    XMFLOAT3 v;
    float r;
    do {
        v = { RandRange(-1, 1), RandRange(-1, 1), RandRange(-1, 1) };
        r = v.x * v.x + v.y * v.y + v.z * v.z;
    } while (r > 1.0f || r < 0.0001f);
    float s = radius * Rand01() / sqrtf(r);
    return { v.x * s, v.y * s, v.z * s };
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
FluidParticleSystem::FluidParticleSystem()
{
    memset(m_HashTable.data(), 0, sizeof(SpatialHashCell) * HASH_TABLE_SIZE);
}

FluidParticleSystem::~FluidParticleSystem()
{
    if (m_pParticleBuffer && m_pMappedParticles)
    {
        m_pParticleBuffer->Unmap(0, nullptr);
        m_pMappedParticles = nullptr;
    }
    if (m_pPassCB && m_pMappedPassCB)
    {
        m_pPassCB->Unmap(0, nullptr);
        m_pMappedPassCB = nullptr;
    }
}

// ============================================================================
// Init
// ============================================================================
void FluidParticleSystem::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* /*pCommandList*/,
                               CDescriptorHeap* pDescriptorHeap, UINT nSrvDescriptorIndex)
{
    m_pDescriptorHeap     = pDescriptorHeap;
    m_nSrvDescriptorIndex = nSrvDescriptorIndex;

    // 1. Particle upload buffer (StructuredBuffer source)
    UINT64 bufferSize = sizeof(FluidParticleRenderData) * MAX_PARTICLES;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width              = bufferSize;
    bufDesc.Height             = 1;
    bufDesc.DepthOrArraySize   = 1;
    bufDesc.MipLevels          = 1;
    bufDesc.Format             = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count   = 1;
    bufDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&m_pParticleBuffer));
    if (FAILED(hr)) { OutputDebugStringA("[FluidPS] Failed to create particle buffer\n"); return; }

    m_pParticleBuffer->Map(0, nullptr, (void**)&m_pMappedParticles);

    // 2. SRV for StructuredBuffer
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                     = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement        = 0;
    srvDesc.Buffer.NumElements         = MAX_PARTICLES;
    srvDesc.Buffer.StructureByteStride = sizeof(FluidParticleRenderData);
    srvDesc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;

    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = pDescriptorHeap->GetCPUHandle(nSrvDescriptorIndex);
    pDevice->CreateShaderResourceView(m_pParticleBuffer.Get(), &srvDesc, srvCpuHandle);

    // 3. Small pass CB (ViewProj + CameraRight + CameraUp) - 256 bytes aligned
    UINT passCBSize = (sizeof(FluidPassCB) + 255) & ~255;
    D3D12_RESOURCE_DESC cbDesc = bufDesc;
    cbDesc.Width = passCBSize;

    hr = pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&m_pPassCB));
    if (FAILED(hr)) { OutputDebugStringA("[FluidPS] Failed to create pass CB\n"); return; }
    m_pPassCB->Map(0, nullptr, &m_pMappedPassCB);

    // 4-6. Build shared pipeline (root signature, shaders, PSO) - once for all instances
    BuildSharedPipeline(pDevice);

    OutputDebugStringA("[FluidParticleSystem] Initialized\n");
}

// ============================================================================
// BuildSharedPipeline (static) - 모든 인스턴스가 공유하는 파이프라인
// ============================================================================
void FluidParticleSystem::BuildSharedPipeline(ID3D12Device* pDevice)
{
    if (s_pRootSignature && s_pPSO) return;  // 이미 빌드됨

    HRESULT hr;

    // 4. Build Root Signature
    D3D12_ROOT_PARAMETER rootParams[2] = {};

    rootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;  // b0
    rootParams[0].Descriptor.RegisterSpace  = 0;
    rootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors                    = 1;
    srvRange.BaseShaderRegister                = 0;  // t0
    srvRange.RegisterSpace                     = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParams[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges   = &srvRange;
    rootParams[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters     = 2;
    rsDesc.pParameters       = rootParams;
    rsDesc.NumStaticSamplers = 0;
    rsDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> sigBlob, errBlob;
    hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        return;
    }
    hr = pDevice->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
                                       IID_PPV_ARGS(&s_pRootSignature));
    if (FAILED(hr)) { OutputDebugStringA("[FluidPS] Failed to create root signature\n"); return; }

    // 5. Compile inline shaders
    ComPtr<ID3DBlob> vsBlob, psBlob;
    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    hr = D3DCompile(g_FluidShaderCode, strlen(g_FluidShaderCode), "FluidParticleShader",
                     nullptr, nullptr, "VS_Fluid", "vs_5_1", compileFlags, 0, &vsBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        OutputDebugStringA("[FluidPS] VS compile failed\n");
        return;
    }

    errBlob.Reset();
    hr = D3DCompile(g_FluidShaderCode, strlen(g_FluidShaderCode), "FluidParticleShader",
                     nullptr, nullptr, "PS_Fluid", "ps_5_1", compileFlags, 0, &psBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        OutputDebugStringA("[FluidPS] PS compile failed\n");
        return;
    }

    // 6. Build PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = s_pRootSignature.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    psoDesc.InputLayout = { nullptr, 0 };

    psoDesc.BlendState.AlphaToCoverageEnable  = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    auto& rt = psoDesc.BlendState.RenderTarget[0];
    rt.BlendEnable           = TRUE;
    rt.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOp               = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha         = D3D12_BLEND_ONE;
    rt.DestBlendAlpha        = D3D12_BLEND_ZERO;
    rt.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthClipEnable       = TRUE;

    psoDesc.DepthStencilState.DepthEnable    = TRUE;
    psoDesc.DepthStencilState.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    psoDesc.SampleMask             = UINT_MAX;
    psoDesc.PrimitiveTopologyType  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets       = 1;
    psoDesc.RTVFormats[0]          = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat              = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count       = 1;

    hr = pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&s_pPSO));
    if (FAILED(hr)) { OutputDebugStringA("[FluidPS] Failed to create PSO\n"); return; }

    OutputDebugStringA("[FluidParticleSystem] Shared pipeline built\n");
}

// ============================================================================
// Spawn
// ============================================================================
void FluidParticleSystem::Spawn(const XMFLOAT3& center, const FluidParticleConfig& config)
{
    // 슬롯 재사용 시 이전 시퀀스 상태 완전 초기화
    m_MotionMode          = ParticleMotionMode::ControlPoint;
    m_ConfinementBox      = ConfinementBoxDesc{};   // active=false
    m_BeamDesc            = BeamDesc{};
    m_GravityDesc         = GravityDesc{};
    m_GlobalGravityStrength = 0.f;
    m_ControlPoints.clear();

    m_Config = config;
    m_Colors = FluidElementColors::Get(config.element);

    int count = (std::min)(config.particleCount, MAX_PARTICLES);
    m_Particles.clear();
    m_Particles.resize(count);

    for (auto& p : m_Particles)
    {
        XMFLOAT3 offset = RandInSphere(config.spawnRadius);
        p.position = { center.x + offset.x, center.y + offset.y, center.z + offset.z };
        // Small random kick so particles start visibly moving from frame 1
        p.velocity = { RandRange(-1.5f, 1.5f), RandRange(-0.5f, 0.5f), RandRange(-1.5f, 1.5f) };
        p.force    = { 0, 0, 0 };
        p.density  = config.restDensity;
        p.pressure = 0.0f;
        p.mass     = 1.0f;
        p.active   = true;
    }

    // Set default control point at center if none set
    if (m_ControlPoints.empty())
    {
        FluidControlPoint cp;
        cp.position           = center;
        cp.attractionStrength = 15.0f;
        cp.sphereRadius       = config.spawnRadius * 1.2f;
        m_ControlPoints.push_back(cp);
    }

    OutputDebugStringA("[FluidPS] Spawned particles\n");
}

void FluidParticleSystem::Clear()
{
    m_Particles.clear();
    m_ControlPoints.clear();
    m_nActiveCount = 0;
}

void FluidParticleSystem::SetControlPoints(const std::vector<FluidControlPoint>& cps)
{
    m_ControlPoints = cps;
}

void FluidParticleSystem::AddControlPoint(const FluidControlPoint& cp)
{
    m_ControlPoints.push_back(cp);
}

void FluidParticleSystem::ClearControlPoints()
{
    m_ControlPoints.clear();
}

// ============================================================================
// SPH Kernel Functions
// ============================================================================
float FluidParticleSystem::Poly6(float r2, float h2) const
{
    if (r2 >= h2) return 0.0f;
    float h = sqrtf(h2);
    float h9 = h2 * h2 * h2 * h2 * h;  // h^9
    float diff = h2 - r2;
    return (315.0f / (64.0f * PI_F * h9)) * diff * diff * diff;
}

XMFLOAT3 FluidParticleSystem::SpikyGrad(const XMFLOAT3& r, float rLen, float h) const
{
    if (rLen >= h || rLen < 0.0001f) return { 0, 0, 0 };
    float h6 = h * h * h * h * h * h;  // h^6
    float coeff = -(45.0f / (PI_F * h6)) * (h - rLen) * (h - rLen) / rLen;
    return { r.x * coeff, r.y * coeff, r.z * coeff };
}

float FluidParticleSystem::ViscLaplacian(float r, float h) const
{
    if (r >= h) return 0.0f;
    float h6 = h * h * h * h * h * h;
    return (45.0f / (PI_F * h6)) * (h - r);
}

// ============================================================================
// Spatial Hash
// ============================================================================
int FluidParticleSystem::HashCell(int cx, int cy, int cz) const
{
    unsigned int h = (unsigned int)(cx * 73856093) ^ (unsigned int)(cy * 19349663) ^ (unsigned int)(cz * 83492791);
    return (int)(h & (HASH_TABLE_SIZE - 1));
}

void FluidParticleSystem::CellFromPos(const XMFLOAT3& p, int& cx, int& cy, int& cz) const
{
    float invH = 1.0f / m_Config.smoothingRadius;
    cx = (int)floorf(p.x * invH);
    cy = (int)floorf(p.y * invH);
    cz = (int)floorf(p.z * invH);
}

void FluidParticleSystem::BuildSpatialHash()
{
    for (int i = 0; i < HASH_TABLE_SIZE; ++i)
        m_HashTable[i].count = 0;

    int n = (int)m_Particles.size();
    for (int i = 0; i < n; ++i)
    {
        if (!m_Particles[i].active) continue;
        int cx, cy, cz;
        CellFromPos(m_Particles[i].position, cx, cy, cz);
        int h = HashCell(cx, cy, cz);
        auto& cell = m_HashTable[h];
        if (cell.count < MAX_PER_CELL)
        {
            cell.particles[cell.count++] = i;
        }
    }
}

int FluidParticleSystem::GetNeighbors(int i, int* out, int maxOut) const
{
    int count = 0;
    int cx, cy, cz;
    CellFromPos(m_Particles[i].position, cx, cy, cz);

    float h2 = m_Config.smoothingRadius * m_Config.smoothingRadius;

    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dz = -1; dz <= 1; ++dz)
            {
                int h = HashCell(cx + dx, cy + dy, cz + dz);
                const auto& cell = m_HashTable[h];
                for (int k = 0; k < cell.count; ++k)
                {
                    int j = cell.particles[k];
                    if (j == i) continue;

                    float rx = m_Particles[i].position.x - m_Particles[j].position.x;
                    float ry = m_Particles[i].position.y - m_Particles[j].position.y;
                    float rz = m_Particles[i].position.z - m_Particles[j].position.z;
                    float r2 = rx * rx + ry * ry + rz * rz;

                    if (r2 < h2)
                    {
                        if (count < maxOut)
                            out[count++] = j;
                    }
                }
            }
        }
    }
    return count;
}

// ============================================================================
// SPH Density / Pressure
// ============================================================================
void FluidParticleSystem::ComputeDensityPressure()
{
    float h2 = m_Config.smoothingRadius * m_Config.smoothingRadius;
    int neighbors[MAX_NEIGHBOR_SEARCH];
    int n = (int)m_Particles.size();

    for (int i = 0; i < n; ++i)
    {
        if (!m_Particles[i].active) continue;

        // Self contribution
        float density = m_Particles[i].mass * Poly6(0.0f, h2);

        int nCount = GetNeighbors(i, neighbors, MAX_NEIGHBOR_SEARCH);
        for (int k = 0; k < nCount; ++k)
        {
            int j = neighbors[k];
            float rx = m_Particles[i].position.x - m_Particles[j].position.x;
            float ry = m_Particles[i].position.y - m_Particles[j].position.y;
            float rz = m_Particles[i].position.z - m_Particles[j].position.z;
            float r2 = rx * rx + ry * ry + rz * rz;

            density += m_Particles[j].mass * Poly6(r2, h2);
        }

        m_Particles[i].density = (std::max)(density, 0.001f);
        m_Particles[i].pressure = m_Config.stiffness * (m_Particles[i].density - m_Config.restDensity);
    }
}

// ============================================================================
// SPH Forces
// ============================================================================
void FluidParticleSystem::ComputeForces()
{
    float h = m_Config.smoothingRadius;
    int neighbors[MAX_NEIGHBOR_SEARCH];
    int n = (int)m_Particles.size();

    for (int i = 0; i < n; ++i)
    {
        if (!m_Particles[i].active) continue;

        float fx = 0.0f, fy = 0.0f, fz = 0.0f;

        int nCount = GetNeighbors(i, neighbors, MAX_NEIGHBOR_SEARCH);
        for (int k = 0; k < nCount; ++k)
        {
            int j = neighbors[k];
            float rx = m_Particles[i].position.x - m_Particles[j].position.x;
            float ry = m_Particles[i].position.y - m_Particles[j].position.y;
            float rz = m_Particles[i].position.z - m_Particles[j].position.z;
            float rLen = sqrtf(rx * rx + ry * ry + rz * rz);

            if (rLen < 0.0001f) continue;

            // Pressure force
            float pressAvg = (m_Particles[i].pressure + m_Particles[j].pressure) * 0.5f;
            XMFLOAT3 rVec = { rx, ry, rz };
            XMFLOAT3 gradP = SpikyGrad(rVec, rLen, h);

            float pScale = -m_Particles[j].mass * pressAvg / (std::max)(m_Particles[j].density, 0.001f);
            fx += gradP.x * pScale;
            fy += gradP.y * pScale;
            fz += gradP.z * pScale;

            // Viscosity force
            float viscLap = ViscLaplacian(rLen, h);
            float vScale = m_Config.viscosity * m_Particles[j].mass / (std::max)(m_Particles[j].density, 0.001f) * viscLap;

            fx += (m_Particles[j].velocity.x - m_Particles[i].velocity.x) * vScale;
            fy += (m_Particles[j].velocity.y - m_Particles[i].velocity.y) * vScale;
            fz += (m_Particles[j].velocity.z - m_Particles[i].velocity.z) * vScale;
        }

        // Gravity 모드: CP 인력 대신 중력 적용
        if (m_MotionMode == ParticleMotionMode::Gravity) {
            fx += m_GravityDesc.gravity.x * m_Particles[i].mass;
            fy += m_GravityDesc.gravity.y * m_Particles[i].mass;
            fz += m_GravityDesc.gravity.z * m_Particles[i].mass;
        }
        else {
            // ControlPoint / OrbitalCP 모드: 기존 CP 인력 + swirl
            for (const auto& cp : m_ControlPoints)
            {
                float dx = cp.position.x - m_Particles[i].position.x;
                float dy = cp.position.y - m_Particles[i].position.y;
                float dz = cp.position.z - m_Particles[i].position.z;
                float dist = sqrtf(dx * dx + dy * dy + dz * dz);

                if (dist < 0.001f) continue;

                float invDist = 1.0f / dist;
                float ndx = dx * invDist;
                float ndy = dy * invDist;
                float ndz = dz * invDist;

                // Soft attraction (attenuated by distance)
                float attraction = cp.attractionStrength / (1.0f + dist * 0.5f);
                fx += ndx * attraction;
                fy += ndy * attraction;
                fz += ndz * attraction;

                // Gentle swirl: tangential force = cross(Y_up, toward_cp)
                // Creates horizontal rotation around the control point
                float swirlStrength = cp.attractionStrength * 0.35f;
                // tangent = cross((0,1,0), (ndx,ndy,ndz)) = (ndz, 0, -ndx)
                fx += ndz * swirlStrength;
                fz += -ndx * swirlStrength;

                // Boundary: strong inward force if outside sphere radius
                if (dist > cp.sphereRadius)
                {
                    float overshoot = dist - cp.sphereRadius;
                    float boundaryForce = m_Config.boundaryStiffness * overshoot;
                    fx += ndx * boundaryForce;
                    fy += ndy * boundaryForce;
                    fz += ndz * boundaryForce;
                }
            }
        }

        // 전역 중력 (모든 모드에서 선택적 적용)
        if (m_GlobalGravityStrength > 0.f) {
            fy -= m_GlobalGravityStrength * m_Particles[i].mass;
        }

        // ConfinementBox 경계력 (모든 모드에서 선택적 적용)
        if (m_ConfinementBox.active) {
            XMVECTOR toParticle = XMVectorSubtract(
                XMLoadFloat3(&m_Particles[i].position),
                XMLoadFloat3(&m_ConfinementBox.center));
            float localX = XMVectorGetX(XMVector3Dot(toParticle, XMLoadFloat3(&m_ConfinementBox.axisX)));
            float localY = XMVectorGetX(XMVector3Dot(toParticle, XMLoadFloat3(&m_ConfinementBox.axisY)));
            float localZ = XMVectorGetX(XMVector3Dot(toParticle, XMLoadFloat3(&m_ConfinementBox.axisZ)));

            XMVECTOR forceVec = XMVectorZero();
            float bStiff = m_Config.boundaryStiffness;

            auto applyAxisForce = [&](float localCoord, float halfExt, const XMFLOAT3& axis) {
                if (localCoord > halfExt) {
                    float excess = localCoord - halfExt;
                    XMVECTOR axisVec = XMLoadFloat3(&axis);
                    forceVec = XMVectorSubtract(forceVec, XMVectorScale(axisVec, bStiff * excess));
                } else if (localCoord < -halfExt) {
                    float excess = -halfExt - localCoord;
                    XMVECTOR axisVec = XMLoadFloat3(&axis);
                    forceVec = XMVectorAdd(forceVec, XMVectorScale(axisVec, bStiff * excess));
                }
            };

            applyAxisForce(localX, m_ConfinementBox.halfExtents.x, m_ConfinementBox.axisX);
            applyAxisForce(localY, m_ConfinementBox.halfExtents.y, m_ConfinementBox.axisY);
            applyAxisForce(localZ, m_ConfinementBox.halfExtents.z, m_ConfinementBox.axisZ);

            XMFLOAT3 boxForce;
            XMStoreFloat3(&boxForce, forceVec);
            fx += boxForce.x;
            fy += boxForce.y;
            fz += boxForce.z;
        }

        m_Particles[i].force = { fx, fy, fz };
    }
}

// ============================================================================
// Integration
// ============================================================================
void FluidParticleSystem::Integrate(float dt)
{
    const float DAMPING = 0.995f;
    const float MAX_SPEED = 12.0f;
    int n = (int)m_Particles.size();

    for (int i = 0; i < n; ++i)
    {
        if (!m_Particles[i].active) continue;

        float invDensity = 1.0f / (std::max)(m_Particles[i].density, 0.001f);

        // acceleration = force / density
        m_Particles[i].velocity.x += (m_Particles[i].force.x * invDensity) * dt;
        m_Particles[i].velocity.y += (m_Particles[i].force.y * invDensity) * dt;
        m_Particles[i].velocity.z += (m_Particles[i].force.z * invDensity) * dt;

        // Damping
        m_Particles[i].velocity.x *= DAMPING;
        m_Particles[i].velocity.y *= DAMPING;
        m_Particles[i].velocity.z *= DAMPING;

        // Speed clamping
        float speed2 = m_Particles[i].velocity.x * m_Particles[i].velocity.x
                      + m_Particles[i].velocity.y * m_Particles[i].velocity.y
                      + m_Particles[i].velocity.z * m_Particles[i].velocity.z;
        if (speed2 > MAX_SPEED * MAX_SPEED)
        {
            float scale = MAX_SPEED / sqrtf(speed2);
            m_Particles[i].velocity.x *= scale;
            m_Particles[i].velocity.y *= scale;
            m_Particles[i].velocity.z *= scale;
        }

        // Position update
        m_Particles[i].position.x += m_Particles[i].velocity.x * dt;
        m_Particles[i].position.y += m_Particles[i].velocity.y * dt;
        m_Particles[i].position.z += m_Particles[i].velocity.z * dt;
    }
}

void FluidParticleSystem::OffsetParticles(const XMFLOAT3& delta)
{
    for (auto& p : m_Particles)
    {
        if (!p.active) continue;
        p.position.x += delta.x;
        p.position.y += delta.y;
        p.position.z += delta.z;
    }
}

// ============================================================================
// Update
// ============================================================================
void FluidParticleSystem::Update(float deltaTime)
{
    if (m_Particles.empty()) return;

    // Clamp dt for stability with large timesteps
    float dt = (std::min)(deltaTime, 0.016f);

    // Beam 모드: 빔-로컬 좌표 방식 (방향 변경 시 모든 파티클 즉시 스냅)
    if (m_MotionMode == ParticleMotionMode::Beam) {
        XMVECTOR start     = XMLoadFloat3(&m_BeamDesc.startPos);
        XMVECTOR end       = XMLoadFloat3(&m_BeamDesc.endPos);
        XMVECTOR dir       = XMVector3Normalize(XMVectorSubtract(end, start));
        float    totalDist = XMVectorGetX(XMVector3Length(XMVectorSubtract(end, start)));
        XMStoreFloat3(&m_BeamDesc.prevDir, dir);

        // 수직 기저 벡터 (빔 축에 수직인 평면)
        XMVECTOR perpX = XMVector3Normalize(XMVectorSet(-XMVectorGetZ(dir), 0.f, XMVectorGetX(dir), 0.f));
        XMVECTOR perpY = XMVector3Cross(dir, perpX);

        for (auto& p : m_Particles) {
            if (!p.active) continue;

            // 빔 축 방향으로 전진
            p.beamT += p.beamSpeed * dt;

            // 끝 도달 또는 범위 이탈 → 시작으로 리셋 (새 퍼짐 오프셋 할당)
            if (p.beamT >= totalDist || p.beamT < 0.f) {
                p.beamT     = 0.f;
                p.beamRx    = (Rand01() - 0.5f) * 2.f * m_BeamDesc.spreadRadius;
                p.beamRy    = (Rand01() - 0.5f) * 2.f * m_BeamDesc.spreadRadius;
                p.beamSpeed = m_BeamDesc.speedMin + Rand01() * (m_BeamDesc.speedMax - m_BeamDesc.speedMin);
            }

            // 빔-로컬 → 월드 좌표 변환: 방향이 바뀌면 즉시 반영, 늘어남 없음
            XMVECTOR pos = XMVectorAdd(
                XMVectorAdd(start, XMVectorScale(dir, p.beamT)),
                XMVectorAdd(XMVectorScale(perpX, p.beamRx), XMVectorScale(perpY, p.beamRy))
            );
            XMStoreFloat3(&p.position, pos);
        }
        return; // SPH 단계 스킵
    }

    // 일반 SPH 시뮬레이션 (ControlPoint, Gravity, OrbitalCP 모드)
    BuildSpatialHash();
    ComputeDensityPressure();
    ComputeForces();
    Integrate(dt);
}

// ============================================================================
// Upload Render Data
// ============================================================================
void FluidParticleSystem::UploadRenderData()
{
    if (!m_pMappedParticles) return;

    int renderIdx = 0;
    int n = (int)m_Particles.size();

    for (int i = 0; i < n && renderIdx < MAX_PARTICLES; ++i)
    {
        if (!m_Particles[i].active) continue;

        // Density-based color interpolation
        float t = m_Particles[i].density / (std::max)(m_Config.restDensity, 0.001f);
        t = (std::min)(t, 2.0f) * 0.5f;  // normalize to [0, 1]
        t = (std::max)(0.0f, (std::min)(1.0f, t));

        // Lerp from edgeColor to coreColor
        XMFLOAT4 color;
        color.x = m_Colors.edgeColor.x + (m_Colors.coreColor.x - m_Colors.edgeColor.x) * t;
        color.y = m_Colors.edgeColor.y + (m_Colors.coreColor.y - m_Colors.edgeColor.y) * t;
        color.z = m_Colors.edgeColor.z + (m_Colors.coreColor.z - m_Colors.edgeColor.z) * t;
        color.w = m_Colors.edgeColor.w + (m_Colors.coreColor.w - m_Colors.edgeColor.w) * t;

        m_pMappedParticles[renderIdx].position = m_Particles[i].position;
        m_pMappedParticles[renderIdx].size     = m_Config.particleSize;
        m_pMappedParticles[renderIdx].color    = color;

        renderIdx++;
    }

    m_nActiveCount = renderIdx;
}

// ============================================================================
// Render
// ============================================================================
void FluidParticleSystem::Render(ID3D12GraphicsCommandList* pCommandList,
                                 const XMFLOAT4X4& viewProj, const XMFLOAT3& cameraRight, const XMFLOAT3& cameraUp)
{
    if (m_Particles.empty() || !s_pPSO || !s_pRootSignature) return;

    // Update pass CB
    if (m_pMappedPassCB)
    {
        FluidPassCB* pCB = reinterpret_cast<FluidPassCB*>(m_pMappedPassCB);
        pCB->viewProj    = viewProj;
        pCB->cameraRight = cameraRight;
        pCB->padR        = 0.0f;
        pCB->cameraUp    = cameraUp;
        pCB->padU        = 0.0f;
    }

    // Upload render data
    UploadRenderData();
    if (m_nActiveCount == 0) return;

    // Set descriptor heap
    ID3D12DescriptorHeap* ppHeaps[] = { m_pDescriptorHeap->GetHeap() };
    pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Set pipeline state
    pCommandList->SetPipelineState(s_pPSO.Get());
    pCommandList->SetGraphicsRootSignature(s_pRootSignature.Get());

    // Bind root parameters
    pCommandList->SetGraphicsRootConstantBufferView(0, m_pPassCB->GetGPUVirtualAddress());

    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = m_pDescriptorHeap->GetGPUHandle(m_nSrvDescriptorIndex);
    pCommandList->SetGraphicsRootDescriptorTable(1, srvGpuHandle);

    // Set primitive topology
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Instanced draw (4 vertices per quad, one instance per active particle)
    pCommandList->DrawInstanced(4, (UINT)m_nActiveCount, 0, 0);
}

// ============================================================================
// 운동 모드 관련 메서드
// ============================================================================
void FluidParticleSystem::SetMotionMode(ParticleMotionMode mode)
{
    m_MotionMode = mode;
}

void FluidParticleSystem::SetConfinementBox(const ConfinementBoxDesc& box)
{
    m_ConfinementBox = box;
}

void FluidParticleSystem::SetBeamDesc(const BeamDesc& beam)
{
    m_BeamDesc = beam;
}

void FluidParticleSystem::SetGravityDesc(const GravityDesc& grav)
{
    m_GravityDesc = grav;
}

void FluidParticleSystem::SetGlobalGravity(float strength)
{
    m_GlobalGravityStrength = strength;
}

void FluidParticleSystem::InitBeamParticles()
{
    // Beam 모드용 파티클 초기화: startPos 주변에서 endPos 방향으로 분산 배치
    XMVECTOR start = XMLoadFloat3(&m_BeamDesc.startPos);
    XMVECTOR end   = XMLoadFloat3(&m_BeamDesc.endPos);
    XMVECTOR dir   = XMVector3Normalize(XMVectorSubtract(end, start));

    // prevDir을 현재 방향으로 초기화 (첫 프레임에 불필요한 회전 방지)
    XMStoreFloat3(&m_BeamDesc.prevDir, dir);
    float totalDist = XMVectorGetX(XMVector3Length(XMVectorSubtract(end, start)));

    XMVECTOR perpX = XMVector3Normalize(XMVectorSet(-XMVectorGetZ(dir), 0, XMVectorGetX(dir), 0));
    XMVECTOR perpY = XMVector3Cross(dir, perpX);

    for (auto& p : m_Particles) {
        if (!p.active) continue;
        // 빔-로컬 좌표 초기화: 빔 전체 길이에 균등 분포
        p.beamT     = Rand01() * totalDist;
        p.beamRx    = (Rand01() - 0.5f) * 2.f * m_BeamDesc.spreadRadius;
        p.beamRy    = (Rand01() - 0.5f) * 2.f * m_BeamDesc.spreadRadius;
        p.beamSpeed = m_BeamDesc.speedMin + Rand01() * (m_BeamDesc.speedMax - m_BeamDesc.speedMin);

        // 초기 월드 좌표 설정
        XMVECTOR pos = XMVectorAdd(
            XMVectorAdd(start, XMVectorScale(dir, p.beamT)),
            XMVectorAdd(XMVectorScale(perpX, p.beamRx), XMVectorScale(perpY, p.beamRy))
        );
        XMStoreFloat3(&p.position, pos);
        p.velocity = { 0, 0, 0 };
    }
}

void FluidParticleSystem::ApplyDirectionalForce(const XMFLOAT3& direction, float impulse)
{
    for (auto& p : m_Particles) {
        if (!p.active) continue;
        p.velocity.x += direction.x * impulse;
        p.velocity.y += direction.y * impulse;
        p.velocity.z += direction.z * impulse;
    }
}

void FluidParticleSystem::ApplyRadialBurst(XMFLOAT3 center, float minSpeed, float maxSpeed)
{
    for (auto& p : m_Particles) {
        if (!p.active) continue;
        XMVECTOR pDir = XMVector3Normalize(
            XMVectorSubtract(XMLoadFloat3(&p.position), XMLoadFloat3(&center))
        );
        // 방향이 영벡터인 경우 랜덤 방향
        if (XMVectorGetX(XMVector3Length(pDir)) < 0.001f) {
            pDir = XMVector3Normalize(XMVectorSet(
                Rand01() - 0.5f, Rand01() - 0.5f, Rand01() - 0.5f, 0));
        }
        float speed = minSpeed + Rand01() * (maxSpeed - minSpeed);
        XMFLOAT3 v; XMStoreFloat3(&v, XMVectorScale(pDir, speed));
        p.velocity.x += v.x;
        p.velocity.y += v.y;
        p.velocity.z += v.z;
    }
}

void FluidParticleSystem::ZeroAxisVelocity(const XMFLOAT3& worldAxis)
{
    XMVECTOR axis = XMVector3Normalize(XMLoadFloat3(&worldAxis));
    for (auto& p : m_Particles)
    {
        if (!p.active) continue;
        XMVECTOR vel    = XMLoadFloat3(&p.velocity);
        float    proj   = XMVectorGetX(XMVector3Dot(vel, axis));
        // 축 방향 성분 제거: vel -= proj * axis
        vel = XMVectorSubtract(vel, XMVectorScale(axis, proj));
        XMStoreFloat3(&p.velocity, vel);
    }
}

void FluidParticleSystem::ApplyAxisSpreadForce(const XMFLOAT3& axisDir,
                                                const XMFLOAT3& originPoint,
                                                float           impulse)
{
    XMVECTOR axis   = XMVector3Normalize(XMLoadFloat3(&axisDir));
    XMVECTOR origin = XMLoadFloat3(&originPoint);
    for (auto& p : m_Particles)
    {
        if (!p.active) continue;
        XMVECTOR toP  = XMVectorSubtract(XMLoadFloat3(&p.position), origin);
        float    side = XMVectorGetX(XMVector3Dot(toP, axis));
        // 중심보다 오른쪽이면 +axis, 왼쪽이면 -axis
        float    sign = (side >= 0.f) ? 1.f : -1.f;
        p.velocity.x += XMVectorGetX(axis) * sign * impulse;
        p.velocity.y += XMVectorGetY(axis) * sign * impulse;
        p.velocity.z += XMVectorGetZ(axis) * sign * impulse;
    }
}

void FluidParticleSystem::ApplyRandomSidewaysImpulse(const XMFLOAT3& worldAxis, float maxImpulse)
{
    XMVECTOR axis = XMVector3Normalize(XMLoadFloat3(&worldAxis));
    for (auto& p : m_Particles)
    {
        if (!p.active) continue;
        // -maxImpulse ~ +maxImpulse 균등 랜덤
        float impulse = (Rand01() * 2.f - 1.f) * maxImpulse;
        p.velocity.x += XMVectorGetX(axis) * impulse;
        p.velocity.y += XMVectorGetY(axis) * impulse;
        p.velocity.z += XMVectorGetZ(axis) * impulse;
    }
}
