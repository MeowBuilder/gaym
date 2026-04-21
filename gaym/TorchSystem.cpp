#include "stdafx.h"
#include "TorchSystem.h"
#include "Scene.h"
#include "Shader.h"
#include "GameObject.h"
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "MeshLoader.h"
#include <cmath>

// Static PSO/RootSig (shared across all TorchSystem instances)
static Microsoft::WRL::ComPtr<ID3D12RootSignature> s_pBillboardRootSig;
static Microsoft::WRL::ComPtr<ID3D12PipelineState> s_pBillboardPSO;

// Helper: Recursively add render components to shader
static void AddRenderComponentsRecursive(GameObject* pObj, Shader* pShader)
{
    if (!pObj || !pShader) return;

    if (pObj->GetMesh())
    {
        auto* pRC = pObj->GetComponent<RenderComponent>();
        if (!pRC)
        {
            pRC = pObj->AddComponent<RenderComponent>();
            pRC->SetMesh(pObj->GetMesh());
        }
        pRC->SetCastsShadow(true);
        pShader->AddRenderComponent(pRC);
    }

    // Recurse to children and siblings
    if (pObj->m_pChild) AddRenderComponentsRecursive(pObj->m_pChild, pShader);
    if (pObj->m_pSibling) AddRenderComponentsRecursive(pObj->m_pSibling, pShader);
}

// Billboard constant buffer layout
struct BillboardPassCB
{
    XMFLOAT4X4 viewProj;
    XMFLOAT3   cameraRight; float padR;
    XMFLOAT3   cameraUp;    float padU;
    float      time;        float pad1; float pad2; float pad3;
};
static_assert(sizeof(BillboardPassCB) == 112, "BillboardPassCB size mismatch");

// Per-instance data for flame billboards
struct FlameInstanceData
{
    XMFLOAT3 position;
    float    size;
    XMFLOAT4 color;
};

// Inline HLSL shader for flame billboards
static const char* g_FlameBillboardShaderCode = R"(
    cbuffer cbBillboardPass : register(b0)
    {
        matrix gViewProj;
        float3 gCameraRight; float gPadR;
        float3 gCameraUp;    float gPadU;
        float  gTime;        float gPad1; float gPad2; float gPad3;
    };

    Texture2D gFlameTexture : register(t0);
    SamplerState gSampler : register(s0);

    struct FlameInstanceData
    {
        float3 position;
        float  size;
        float4 color;
    };
    StructuredBuffer<FlameInstanceData> gFlames : register(t1);

    struct VSOut
    {
        float4 pos   : SV_POSITION;
        float4 color : COLOR0;
        float2 uv    : TEXCOORD0;
    };

    static const float2 kOffsets[4] = {
        { -0.5f,  0.0f },
        {  0.5f,  0.0f },
        { -0.5f,  1.0f },
        {  0.5f,  1.0f }
    };
    static const float2 kUVs[4] = {
        { 0.0f, 1.0f },
        { 1.0f, 1.0f },
        { 0.0f, 0.0f },
        { 1.0f, 0.0f }
    };

    VSOut VS_Flame(uint vertId : SV_VertexID, uint instId : SV_InstanceID)
    {
        // Cross-billboard: 2 instances per flame (instId/2 = flame, instId%2 = quad)
        uint flameIdx = instId / 2;
        uint quadIdx = instId % 2;

        FlameInstanceData f = gFlames[flameIdx];

        float2 offset = kOffsets[vertId];
        float2 uv     = kUVs[vertId];

        // Flame narrows toward top (teardrop shape)
        float taper = 1.0f - offset.y * 0.4f;
        offset.x *= taper;

        // Use world up vector for stable vertical flame
        float3 worldUp = float3(0.0f, 1.0f, 0.0f);

        // Two perpendicular directions for cross-billboard
        float3 right1 = gCameraRight;
        float3 right2 = cross(worldUp, gCameraRight);
        right2 = normalize(right2);

        float3 horizontalDir = (quadIdx == 0) ? right1 : right2;

        float3 worldPos = f.position
            + horizontalDir * offset.x * f.size
            + worldUp * offset.y * f.size * 2.0f;

        VSOut output;
        output.pos   = mul(float4(worldPos, 1.0f), gViewProj);
        output.color = f.color;
        output.uv    = uv;
        return output;
    }

    float4 PS_Flame(VSOut input) : SV_TARGET
    {
        float2 uv = input.uv;
        float2 centered = uv * 2.0f - 1.0f;

        // Subtle edge distortion (very small, slow)
        float edgeWave = sin(gTime * 1.5f + uv.y * 4.0f) * 0.02f;
        centered.x += edgeWave * (1.0f - uv.y);  // Only affect bottom portion slightly

        float dist = length(centered * float2(0.8f, 1.0f));

        // Core intensity - bright center
        float core = 1.0f - smoothstep(0.0f, 0.5f, dist);
        core = pow(core, 1.2f);

        // Outer falloff
        float falloff = 1.0f - smoothstep(0.2f, 0.9f, dist);

        // Flickering brightness (subtle)
        float flicker = 0.95f + sin(gTime * 8.0f) * 0.05f;

        // Color gradient: bright yellow core -> orange -> red edge
        float3 coreColor  = float3(1.0f, 0.9f, 0.5f);
        float3 midColor   = float3(1.0f, 0.5f, 0.1f);
        float3 outerColor = float3(0.7f, 0.15f, 0.0f);

        float3 flameColor = lerp(outerColor, midColor, core);
        flameColor = lerp(flameColor, coreColor, core * core);

        // Brighter at bottom, fades toward top
        float heightFade = 1.0f - uv.y * 0.4f;

        float3 finalColor = flameColor * heightFade * flicker * input.color.a * 1.5f;

        // Alpha
        float alpha = falloff * input.color.a;
        alpha *= heightFade;
        alpha = saturate(alpha * 1.5f);

        clip(alpha - 0.02f);

        return float4(finalColor, alpha);
    }
)";

TorchSystem::TorchSystem()
{
}

TorchSystem::~TorchSystem()
{
    if (m_pBillboardCB && m_pBillboardCBMapped)
    {
        m_pBillboardCB->Unmap(0, nullptr);
        m_pBillboardCBMapped = nullptr;
    }
}

void TorchSystem::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
                       Scene* pScene, Shader* pShader,
                       CDescriptorHeap* pDescriptorHeap, UINT nDescriptorStart)
{
    m_pScene = pScene;
    m_pShader = pShader;
    m_pDescriptorHeap = pDescriptorHeap;
    m_nDescriptorStart = nDescriptorStart;

    // Load torch mesh (if available)
    // Note: Torch mesh will be loaded per-instance in AddTorch

    // Create billboard rendering resources
    CreateBillboardResources(pDevice, pCommandList);

    // Load flame texture
    LoadFlameTexture(pDevice, pCommandList);

    m_bInitialized = true;
    OutputDebugStringA("[TorchSystem] Initialized\n");
}

void TorchSystem::CreateBillboardResources(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    HRESULT hr;

    // Only create once (shared static)
    if (!s_pBillboardRootSig)
    {
        // Root signature: CBV(b0), SRV table(t0-t1), Sampler(s0)
        D3D12_DESCRIPTOR_RANGE srvRange = {};
        srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors = 2;  // t0=texture, t1=instance buffer
        srvRange.BaseShaderRegister = 0;
        srvRange.RegisterSpace = 0;
        srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER rootParams[3] = {};

        // [0] CBV for pass constants
        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[0].Descriptor.ShaderRegister = 0;
        rootParams[0].Descriptor.RegisterSpace = 0;
        rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // [1] Descriptor table for SRVs
        rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[1].DescriptorTable.pDescriptorRanges = &srvRange;
        rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // Static sampler
        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 1;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
        rsDesc.NumParameters = 2;  // CBV + SRV table
        rsDesc.pParameters = rootParams;
        rsDesc.NumStaticSamplers = 1;
        rsDesc.pStaticSamplers = &sampler;
        rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        Microsoft::WRL::ComPtr<ID3DBlob> sigBlob, errBlob;
        hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            return;
        }

        hr = pDevice->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
                                          IID_PPV_ARGS(&s_pBillboardRootSig));
        if (FAILED(hr)) { OutputDebugStringA("[TorchSystem] Failed to create root signature\n"); return; }
    }

    // Create PSO
    if (!s_pBillboardPSO)
    {
        Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

        hr = D3DCompile(g_FlameBillboardShaderCode, strlen(g_FlameBillboardShaderCode),
                        "FlameBillboard", nullptr, nullptr, "VS_Flame", "vs_5_1",
                        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vsBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            return;
        }

        hr = D3DCompile(g_FlameBillboardShaderCode, strlen(g_FlameBillboardShaderCode),
                        "FlameBillboard", nullptr, nullptr, "PS_Flame", "ps_5_1",
                        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &psBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            return;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = s_pBillboardRootSig.Get();
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

        // Additive blending for flames
        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;  // Additive
        psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        psoDesc.SampleMask = UINT_MAX;
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;

        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;  // Don't write depth
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        psoDesc.SampleDesc.Count = 1;

        hr = pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&s_pBillboardPSO));
        if (FAILED(hr)) { OutputDebugStringA("[TorchSystem] Failed to create PSO\n"); return; }
    }

    // Create constant buffer
    UINT cbSize = (sizeof(BillboardPassCB) + 255) & ~255;
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC cbDesc = {};
    cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Width = cbSize;
    cbDesc.Height = 1;
    cbDesc.DepthOrArraySize = 1;
    cbDesc.MipLevels = 1;
    cbDesc.Format = DXGI_FORMAT_UNKNOWN;
    cbDesc.SampleDesc.Count = 1;
    cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                          &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                          nullptr, IID_PPV_ARGS(&m_pBillboardCB));
    if (FAILED(hr)) { OutputDebugStringA("[TorchSystem] Failed to create CB\n"); return; }

    m_pBillboardCB->Map(0, nullptr, (void**)&m_pBillboardCBMapped);

    // Create instance buffer for flame data (up to MAX_TORCH_LIGHTS flames)
    UINT64 instanceBufferSize = sizeof(FlameInstanceData) * MAX_TORCH_LIGHTS;

    D3D12_RESOURCE_DESC ibDesc = cbDesc;
    ibDesc.Width = instanceBufferSize;

    hr = pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                          &ibDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                          nullptr, IID_PPV_ARGS(&m_pBillboardVB));
    if (FAILED(hr)) { OutputDebugStringA("[TorchSystem] Failed to create instance buffer\n"); return; }

    // Create SRV for instance buffer at descriptor slot (nDescriptorStart + 1)
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = MAX_TORCH_LIGHTS;
    srvDesc.Buffer.StructureByteStride = sizeof(FlameInstanceData);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    D3D12_CPU_DESCRIPTOR_HANDLE instanceSrvCpu = m_pDescriptorHeap->GetCPUHandle(m_nDescriptorStart + 1);
    pDevice->CreateShaderResourceView(m_pBillboardVB.Get(), &srvDesc, instanceSrvCpu);

    OutputDebugStringA("[TorchSystem] Billboard resources created\n");
}

void TorchSystem::LoadFlameTexture(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    HRESULT hr;

    // Create a procedural flame texture (soft orange glow)
    OutputDebugStringA("[TorchSystem] Creating procedural flame texture\n");
    {

        // Create a simple 64x64 orange gradient texture
        const UINT texWidth = 64;
        const UINT texHeight = 64;
        const UINT bpp = 4;
        std::vector<UINT8> texData(texWidth * texHeight * bpp);

        for (UINT y = 0; y < texHeight; ++y)
        {
            for (UINT x = 0; x < texWidth; ++x)
            {
                UINT idx = (y * texWidth + x) * bpp;
                float u = (float)x / (texWidth - 1) - 0.5f;
                float v = (float)y / (texHeight - 1) - 0.5f;
                float dist = sqrtf(u * u + v * v) * 2.0f;
                float alpha = 1.0f - dist;
                alpha = alpha > 0.0f ? alpha : 0.0f;
                alpha = alpha * alpha;  // Softer falloff

                // Orange-yellow color
                texData[idx + 0] = (UINT8)(255 * alpha);          // R
                texData[idx + 1] = (UINT8)(180 * alpha);          // G
                texData[idx + 2] = (UINT8)(50 * alpha);           // B
                texData[idx + 3] = (UINT8)(255 * alpha);          // A
            }
        }

        // Create texture resource
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = texWidth;
        texDesc.Height = texHeight;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        hr = pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                              &texDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                              nullptr, IID_PPV_ARGS(&m_pFlameTexture));
        if (FAILED(hr)) return;

        // Create upload buffer
        UINT64 uploadSize = 0;
        pDevice->GetCopyableFootprints(&texDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadSize);

        D3D12_HEAP_PROPERTIES uploadHeapProps = {};
        uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC uploadDesc = {};
        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Width = uploadSize;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = pDevice->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE,
                                              &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                              nullptr, IID_PPV_ARGS(&m_pFlameTextureUpload));
        if (FAILED(hr)) return;

        // Copy data to upload buffer
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        UINT numRows;
        UINT64 rowSize;
        pDevice->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows, &rowSize, nullptr);

        UINT8* pMapped;
        m_pFlameTextureUpload->Map(0, nullptr, (void**)&pMapped);
        for (UINT row = 0; row < numRows; ++row)
        {
            memcpy(pMapped + footprint.Offset + row * footprint.Footprint.RowPitch,
                   texData.data() + row * texWidth * bpp,
                   texWidth * bpp);
        }
        m_pFlameTextureUpload->Unmap(0, nullptr);

        // Copy to texture
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = m_pFlameTexture.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = m_pFlameTextureUpload.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = footprint;

        pCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        // Transition to shader resource
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = m_pFlameTexture.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        pCommandList->ResourceBarrier(1, &barrier);
    }

    // Create SRV for texture at descriptor slot nDescriptorStart
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE texSrvCpu = m_pDescriptorHeap->GetCPUHandle(m_nDescriptorStart);
    pDevice->CreateShaderResourceView(m_pFlameTexture.Get(), &srvDesc, texSrvCpu);

    m_FlameTextureSrvGpu = m_pDescriptorHeap->GetGPUHandle(m_nDescriptorStart);
}

void TorchSystem::AddTorch(const XMFLOAT3& position, ID3D12Device* pDevice,
                           ID3D12GraphicsCommandList* pCommandList,
                           float flameScale, bool spawnMesh, float heightOffset)
{
    if (m_vTorches.size() >= MAX_TORCH_LIGHTS)
    {
        OutputDebugStringA("[TorchSystem] Max torch limit reached\n");
        return;
    }

    TorchData torch;
    torch.m_xmf3Position = position;
    torch.m_fBaseIntensity = m_fMaxIntensity;
    torch.m_fCurrentIntensity = m_fMaxIntensity;
    torch.m_fFlickerTimer = 0.0f;
    torch.m_fFlickerOffset = (float)(rand() % 1000) / 1000.0f * 6.28f;  // Random phase
    torch.m_pMeshObject = nullptr;
    torch.m_fFlameScale  = flameScale;
    torch.m_fHeightOffset = heightOffset;

    // Load torch mesh if requested (Brazier 같은 맵 배치 오브젝트는 spawnMesh=false 로 중복 방지)
    if (spawnMesh && m_pScene)
    {
        GameObject* pTorchMesh = MeshLoader::LoadGeometryFromFile(
            m_pScene, pDevice, pCommandList, nullptr,
            "Assets/MapData/meshes/tourch/TorchHighPoly.bin");

        if (pTorchMesh)
        {
            pTorchMesh->GetTransform()->SetPosition(position.x, position.y, position.z);
            pTorchMesh->GetTransform()->SetScale(8.0f, 8.0f, 8.0f);  // Larger scale for visibility

            // Add render components for mesh hierarchy
            if (m_pShader)
            {
                AddRenderComponentsRecursive(pTorchMesh, m_pShader);
            }

            torch.m_pMeshObject = pTorchMesh;
            OutputDebugStringA("[TorchSystem] Torch mesh loaded successfully\n");
        }
        else
        {
            OutputDebugStringA("[TorchSystem] WARNING: Failed to load torch mesh\n");
        }
    }

    m_vTorches.push_back(torch);

    char buf[128];
    sprintf_s(buf, "[TorchSystem] Torch added at (%.1f, %.1f, %.1f), total: %zu\n",
              position.x, position.y, position.z, m_vTorches.size());
    OutputDebugStringA(buf);
}

void TorchSystem::Update(float deltaTime)
{
    static float totalTime = 0.0f;
    totalTime += deltaTime;

    for (auto& torch : m_vTorches)
    {
        torch.m_fFlickerTimer += deltaTime;

        // Multi-frequency noise for natural flicker
        float noise1 = sinf(torch.m_fFlickerTimer * m_fFlickerSpeed + torch.m_fFlickerOffset);
        float noise2 = sinf(torch.m_fFlickerTimer * m_fFlickerSpeed * 2.3f + torch.m_fFlickerOffset * 1.7f) * 0.5f;
        float noise3 = sinf(torch.m_fFlickerTimer * m_fFlickerSpeed * 0.7f + torch.m_fFlickerOffset * 0.5f) * 0.3f;

        float combinedNoise = (noise1 + noise2 + noise3) / 1.8f;  // Normalize
        combinedNoise = combinedNoise * 0.5f + 0.5f;  // Map to [0, 1]

        torch.m_fCurrentIntensity = m_fMinIntensity + combinedNoise * (m_fMaxIntensity - m_fMinIntensity);
    }
}

void TorchSystem::FillLightData(PassConstants* pPassConstants)
{
    if (!pPassConstants) return;

    int numLights = (int)m_vTorches.size();
    if (numLights > MAX_TORCH_LIGHTS) numLights = MAX_TORCH_LIGHTS;

    pPassConstants->m_nActiveTorchLights = numLights;

    for (int i = 0; i < numLights; ++i)
    {
        const auto& torch = m_vTorches[i];

        // Light position is above the torch base (at flame height). Brazier 같은 큰 받침은 heightOffset 크게.
        pPassConstants->m_TorchLights[i].m_xmf3Position = XMFLOAT3(
            torch.m_xmf3Position.x,
            torch.m_xmf3Position.y + torch.m_fHeightOffset,
            torch.m_xmf3Position.z);

        pPassConstants->m_TorchLights[i].m_fRange    = m_fLightRange * torch.m_fFlameScale;  // flame 크면 빛도 멀리
        pPassConstants->m_TorchLights[i].m_xmf3Color = m_xmf3LightColor;
        pPassConstants->m_TorchLights[i].m_fIntensity = torch.m_fCurrentIntensity;
    }

    // Clear unused slots
    for (int i = numLights; i < MAX_TORCH_LIGHTS; ++i)
    {
        pPassConstants->m_TorchLights[i].m_fIntensity = 0.0f;
    }
}

void TorchSystem::Render(ID3D12GraphicsCommandList* pCommandList,
                         const XMFLOAT4X4& viewProj,
                         const XMFLOAT3& camRight, const XMFLOAT3& camUp)
{
    if (!m_bInitialized || m_vTorches.empty()) return;
    if (!s_pBillboardPSO || !s_pBillboardRootSig) return;
    if (!m_pBillboardCB || !m_pBillboardVB) return;

    // Update constant buffer
    BillboardPassCB* pCB = (BillboardPassCB*)m_pBillboardCBMapped;
    pCB->viewProj = viewProj;
    pCB->cameraRight = camRight;
    pCB->cameraUp = camUp;

    static float time = 0.0f;
    time += 0.016f;  // Approximate 60fps
    pCB->time = time;

    // Update instance buffer with flame positions
    FlameInstanceData* pInstances = nullptr;
    m_pBillboardVB->Map(0, nullptr, (void**)&pInstances);

    for (size_t i = 0; i < m_vTorches.size() && i < MAX_TORCH_LIGHTS; ++i)
    {
        const auto& torch = m_vTorches[i];

        pInstances[i].position = XMFLOAT3(
            torch.m_xmf3Position.x,
            torch.m_xmf3Position.y + torch.m_fHeightOffset,
            torch.m_xmf3Position.z);

        pInstances[i].size = m_fFlameWidth * torch.m_fFlameScale;

        // Intensity controls flame brightness
        float intensity = torch.m_fCurrentIntensity;
        pInstances[i].color = XMFLOAT4(1.0f, 1.0f, 1.0f, intensity);
    }

    m_pBillboardVB->Unmap(0, nullptr);

    // Set pipeline state
    pCommandList->SetPipelineState(s_pBillboardPSO.Get());
    pCommandList->SetGraphicsRootSignature(s_pBillboardRootSig.Get());

    // Set root parameters
    pCommandList->SetGraphicsRootConstantBufferView(0, m_pBillboardCB->GetGPUVirtualAddress());
    pCommandList->SetGraphicsRootDescriptorTable(1, m_FlameTextureSrvGpu);

    // Draw cross-billboard (2 instances per flame for X-shape)
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    pCommandList->DrawInstanced(4, (UINT)m_vTorches.size() * 2, 0, 0);
}

void TorchSystem::Clear()
{
    m_vTorches.clear();
    OutputDebugStringA("[TorchSystem] Cleared all torches\n");
}
