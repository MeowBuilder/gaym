#include "stdafx.h"
#include "ScreenSpaceFluid.h"
#include "d3dx12.h"

// ============================================================================
// Inline HLSL 셰이더 코드
// ============================================================================

// ---------- Pass 1: Sphere Depth + Thickness ----------
static const char* g_SphereDepthShader = R"(
    cbuffer SphereDepthCB : register(b0)
    {
        matrix gViewProj;       // 전치된 ViewProj
        matrix gView;           // 전치된 View
        float3 gCameraRight;
        float  gProjA;          // proj._33 = far/(far-near)
        float3 gCameraUp;
        float  gProjB;          // proj._43 = -near*far/(far-near)
        float  gSmoothingRadius; // SPH 스무딩 반경 (구체 최소 크기 보장용)
        float3 _gPad2;
    };

    struct FluidParticleData
    {
        float3 position;
        float  size;
        float4 color;
    };
    StructuredBuffer<FluidParticleData> gParticles : register(t0);

    struct VSOut
    {
        float4 pos       : SV_POSITION;
        float3 centerVS  : TEXCOORD0;
        float2 uv        : TEXCOORD1;    // [0, 1]
        float  radius    : TEXCOORD2;
    };

    // billboard offset (TRIANGLE_STRIP)
    static const float2 kOffsets[4] = {
        { -0.5f, -0.5f },
        {  0.5f, -0.5f },
        { -0.5f,  0.5f },
        {  0.5f,  0.5f }
    };

    // SSF 전용 구체 스케일 (기본 fallback 배율)
    // SebLague 방식: ssfSize = max(size * scale, smoothingRadius * 1.3)
    // → 구체 반경이 항상 smoothingRadius * 0.65 이상이 되어 인접 파티클이 확실히 겹침
    static const float kSphereScale = 4.5f;

    VSOut SphereDepth_VS(uint vertId : SV_VertexID, uint instId : SV_InstanceID)
    {
        FluidParticleData p = gParticles[instId];

        float2 offset  = kOffsets[vertId];
        // SebLague: 구체 크기는 smoothingRadius에 비례 (반경 = h * 0.65 보장)
        float  ssfSize = max(p.size * kSphereScale, gSmoothingRadius * 1.3f);

        float3 worldPos = p.position
            + gCameraRight * offset.x * ssfSize
            + gCameraUp    * offset.y * ssfSize;

        float4 centerVS4 = mul(float4(p.position, 1.0f), gView);

        VSOut o;
        o.pos      = mul(float4(worldPos, 1.0f), gViewProj);
        o.centerVS = centerVS4.xyz;
        o.uv       = offset + 0.5f;    // [0, 1] 텍스처 좌표
        o.radius   = ssfSize * 0.5f;
        return o;
    }

    // Pass 1a: 깊이만 출력 (SV_DEPTH + FluidDepthRT)
    struct DepthOnlyPSOut
    {
        float depth   : SV_TARGET0;   // R32_FLOAT: 뷰 공간 깊이
        float svDepth : SV_DEPTH;
    };

    DepthOnlyPSOut DepthOnly_PS(VSOut input)
    {
        float2 offset = (input.uv - float2(0.5f, 0.5f)) * 2.0f;
        if (length(offset) >= 1.0f) discard;

        float r  = input.radius;
        float ox = offset.x * r;
        float oy = offset.y * r;
        float inside   = r * r - ox * ox - oy * oy;
        float surfaceZ = input.centerVS.z - sqrt(inside);
        float ndcZ     = (gProjA * surfaceZ + gProjB) / surfaceZ;

        DepthOnlyPSOut o;
        o.depth   = surfaceZ;
        o.svDepth = ndcZ;
        return o;
    }

    // Pass 1b: 두께(chord)만 출력 — 깊이 테스트 없이 모든 파티클이 기여
    float Thickness_PS(VSOut input) : SV_TARGET0
    {
        float2 offset = (input.uv - float2(0.5f, 0.5f)) * 2.0f;
        if (length(offset) >= 1.0f) discard;

        float r      = input.radius;
        float ox     = offset.x * r;
        float oy     = offset.y * r;
        float inside = r * r - ox * ox - oy * oy;
        // chord = 2*sqrt(inside), 반경 정규화: 중심=2, 가장자리→0
        return 2.0f * sqrt(inside) / max(r, 0.001f);
    }
)";

// ---------- Pass 2: Bilateral Depth Smooth ----------
static const char* g_SmoothShader = R"(
    cbuffer SmoothCB : register(b0)
    {
        float2 gTexelSize;
        float2 gBlurDir;
        float  gSigmaDepth;
        float3 gPad;
    };

    Texture2D<float> gDepthTex : register(t0);
    SamplerState     gSampler  : register(s0);

    struct VSOut
    {
        float4 pos : SV_POSITION;
        float2 uv  : TEXCOORD0;
    };

    // 풀스크린 삼각형 (vertex ID 기반)
    VSOut Fullscreen_VS(uint vid : SV_VertexID)
    {
        VSOut o;
        // vid=0: (-1, 1), (0,0)
        // vid=1: ( 3, 1), (2,0)
        // vid=2: (-1,-3), (0,2)
        o.uv  = float2((vid << 1) & 2, vid & 2);
        o.pos = float4(o.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
        return o;
    }

    // 가우시안 가중치 (sigma=4.0), 13-tap (+/- 6)
    static const float kGaussWeights[13] = {
        0.011254f, 0.026995f, 0.052665f, 0.088542f, 0.128420f, 0.160693f, 0.172860f,
        0.160693f, 0.128420f, 0.088542f, 0.052665f, 0.026995f, 0.011254f
    };

    // 각 탭당 픽셀 간격 (더 넓은 커버리지)
    static const float kBlurStep = 4.0f;

    float Smooth_PS(VSOut input) : SV_TARGET
    {
        float centerDepth = gDepthTex.Sample(gSampler, input.uv);

        float totalWeight = 0.0f;
        float totalDepth  = 0.0f;

        float sigmaD2 = 2.0f * gSigmaDepth * gSigmaDepth;

        // 빈 픽셀은 blur 대상 제외 — 인접 유체를 샘플링하면 파티클 외곽에 선 형태 아티팩트 발생
        if (centerDepth < 0.001f) return 0.0f;

        [unroll]
        for (int i = -6; i <= 6; ++i)
        {
            float2 sampleUV    = input.uv + gBlurDir * gTexelSize * ((float)i * kBlurStep);
            float  sampleDepth = gDepthTex.Sample(gSampler, sampleUV);

            if (sampleDepth < 0.001f) continue;

            float depthDiff  = sampleDepth - centerDepth;
            float bilateralW = exp(-(depthDiff * depthDiff) / sigmaD2);
            float w = kGaussWeights[i + 6] * bilateralW;

            totalWeight += w;
            totalDepth  += sampleDepth * w;
        }

        return (totalWeight > 0.0001f) ? (totalDepth / totalWeight) : 0.0f;
    }
)";

// ---------- Pass 3: Sebastian Lague 스타일 Composite ----------
static const char* g_CompositeShader = R"(
    cbuffer CompositeCB : register(b0)
    {
        float  gInvProjX;           // 1/proj._11
        float  gInvProjY;           // 1/proj._22
        float  gScreenWidth;
        float  gScreenHeight;
        float3 gLightDirVS;         // 뷰 공간 조명 방향 (정규화)
        float  gRefractionStr;      // 굴절 강도 (기본 0.04)
        float3 gAbsorption;         // Beer-Lambert 흡수 계수 (채널별)
        float  gProjA;              // proj._33 (view Z → NDC depth 변환용)
        float4 gSpecularColor;      // rgb=색상, a=강도
        float4 gFluidColorOuter;    // rgb=외곽(얇은 부분) 색상, a=발광 강도
        float4 gFluidColorInner;    // rgb=코어(두꺼운 부분) 색상, a=미사용
        float  gProjB;              // proj._43
        float3 gPad1;
    };

    Texture2D<float>  gSmoothedDepth : register(t0);  // 스무딩된 깊이
    Texture2D<float>  gThickness     : register(t1);  // 유체 두께
    Texture2D<float4> gSceneColor    : register(t2);  // 굴절 배경 (유체 전 장면)
    SamplerState      gPointSamp     : register(s0);  // 포인트 클램프
    SamplerState      gLinearSamp    : register(s1);  // 리니어 클램프 (굴절용)

    struct VSOut
    {
        float4 pos : SV_POSITION;
        float2 uv  : TEXCOORD0;
    };

    struct CompositeOut
    {
        float4 color   : SV_TARGET;
        float  svDepth : SV_Depth;
    };

    // 풀스크린 삼각형 (Smooth pass와 동일)
    VSOut Composite_Fullscreen_VS(uint vid : SV_VertexID)
    {
        VSOut o;
        o.uv  = float2((vid << 1) & 2, vid & 2);
        o.pos = float4(o.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
        return o;
    }

    CompositeOut Composite_PS(VSOut input)
    {
        float2 uv = input.uv;
        float depth = gSmoothedDepth.Sample(gPointSamp, uv);
        if (depth < 0.001f) discard;

        // ---- 원형 경계 페이드 ----
        float dzdx = ddx(depth);
        float dzdy = ddy(depth);
        float gradMag = length(float2(dzdx, dzdy));
        // 민감도 완화 (기존 2.5 -> 0.8): 내부 불연속점에서 과도한 discard 방지
        float edgeAlpha = saturate(1.0f - (gradMag / max(depth, 0.01f)) * 0.8f);
        if (edgeAlpha < 0.01f) discard;

        // 뷰 공간 위치 재구성
        float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
        float3 posVS = float3(ndc.x * depth * gInvProjX,
                              ndc.y * depth * gInvProjY,
                              depth);

        // ddx/ddy로 노말 재구성
        // dPdx = 화면 오른쪽 방향, dPdy = 화면 아래 방향(view +Y = 위이므로 부호 반전됨)
        // cross(dPdx, dPdy) → 카메라 방향(-Z)을 향하는 올바른 outward normal
        float3 dPdx = ddx(posVS);
        float3 dPdy = ddy(posVS);
        float3 normal = normalize(cross(dPdx, dPdy));

        // 뷰 벡터
        float3 V = normalize(-posVS);
        float NdotV = max(dot(normal, V), 0.0f);

        // ---- Schlick Fresnel (물 IOR n=1.33, F0=0.02) ----
        float F0 = 0.02f;
        float fresnel = F0 + (1.0f - F0) * pow(1.0f - NdotV, 5.0f);

        // ---- Thickness ----
        float thickness = gThickness.Sample(gPointSamp, uv);

        // ---- 의사 굴절: 노말로 배경 UV 왜곡 ----
        float2 refractUV = uv + normal.xy * gRefractionStr * min(thickness, 3.0f);
        refractUV = saturate(refractUV);
        float3 sceneColor = gSceneColor.Sample(gLinearSamp, refractUV).rgb;

        // ---- 두께 × NdotV 기반 색상 그라디언트 ----
        // kSphereScale=4.5 + 파티클 겹침 → thickness는 실제로 20~40 범위
        float thickT  = saturate(thickness / 25.0f);
        float rawT    = saturate(thickT * (0.3f + 0.7f * NdotV));
        // [0.2, 0.8]로 리매핑: 얇은 곳도 inner 20%, 두꺼운 곳도 outer 20% 항상 섞임
        float colorT  = lerp(0.2f, 0.8f, rawT);
        float3 fluidColor = lerp(gFluidColorOuter.rgb, gFluidColorInner.rgb, colorT);

        // ---- 발광: floor 제거, 두꺼울수록 선형 증가 ----
        float emitStrength = saturate(thickness * 0.04f) * gFluidColorOuter.a;
        float3 emission = fluidColor * emitStrength;

        // ---- 두꺼운 중심부 foam → inner 색으로 밝게 ----
        float foamT = saturate((thickness - 15.0f) * 0.06f);
        float3 foamHighlight = gFluidColorInner.rgb * foamT * 0.4f;

        // ---- 스페큘러 반사 (Blinn-Phong) ----
        float3 L = normalize(-gLightDirVS);
        float3 H = normalize(L + V);
        float specular = pow(max(dot(normal, H), 0.0f), 128.0f);
        float3 specColor = gSpecularColor.rgb * specular * gSpecularColor.a;

        float3 finalColor = sceneColor
                          + specColor * (fresnel + 0.3f)
                          + emission + foamHighlight;

        // 씬 DSV와 깊이 비교용: view-space depth → NDC depth
        float ndcZ = (gProjA * depth + gProjB) / depth;

        CompositeOut o;
        o.color   = float4(finalColor, edgeAlpha);
        o.svDepth = ndcZ;
        return o;
    }
)";

// ============================================================================
// CB 구조체 (CPU 측, 256바이트 정렬용)
// ============================================================================
struct SmoothCB
{
    XMFLOAT2 texelSize;
    XMFLOAT2 blurDir;
    float    sigmaDepth;
    float    pad[3];
};
static_assert(sizeof(SmoothCB) == 32, "SmoothCB 크기 확인");

struct CompositeCB
{
    float    invProjX;
    float    invProjY;
    float    screenWidth;
    float    screenHeight;
    XMFLOAT3 lightDirVS;
    float    refractionStr;
    XMFLOAT3 absorption;        // Beer-Lambert 채널별 흡수 계수
    float    projA;             // proj._33 (view Z → NDC depth)
    XMFLOAT4 specularColor;     // rgb=색상, a=강도
    XMFLOAT4 fluidColorOuter;   // rgb=외곽(얇은 부분) 색상, a=발광 강도
    XMFLOAT4 fluidColorInner;   // rgb=코어(두꺼운 부분) 색상
    float    projB;             // proj._43
    float    pad1[3];
};
static_assert(sizeof(CompositeCB) == 112, "CompositeCB 크기 확인");

// ============================================================================
// 소멸자
// ============================================================================
ScreenSpaceFluid::~ScreenSpaceFluid()
{
    if (m_pCBUpload && m_pMappedCB)
    {
        m_pCBUpload->Unmap(0, nullptr);
        m_pMappedCB = nullptr;
    }
}

// ============================================================================
// Init
// ============================================================================
void ScreenSpaceFluid::Init(ID3D12Device* pDevice, UINT width, UINT height)
{
    if (m_bInitialized) return;

    m_Width  = width;
    m_Height = height;

    // 디스크립터 increment 크기
    m_RTVIncrSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_SRVIncrSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    CreateTextures(pDevice, width, height);
    CreatePipelines(pDevice);

    // Constant buffer (3 x 256 bytes = 768 bytes)
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC cbDesc = {};
    cbDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Width              = 256 * 3;
    cbDesc.Height             = 1;
    cbDesc.DepthOrArraySize   = 1;
    cbDesc.MipLevels          = 1;
    cbDesc.Format             = DXGI_FORMAT_UNKNOWN;
    cbDesc.SampleDesc.Count   = 1;
    cbDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    cbDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&m_pCBUpload));
    if (FAILED(hr)) { OutputDebugStringA("[SSF] CB 생성 실패\n"); return; }

    m_pCBUpload->Map(0, nullptr, (void**)&m_pMappedCB);

    m_bInitialized = true;
    OutputDebugStringA("[SSF] ScreenSpaceFluid 초기화 완료 (Sebastian Lague 스타일)\n");
}

// ============================================================================
// OnResize
// ============================================================================
void ScreenSpaceFluid::OnResize(ID3D12Device* pDevice, UINT width, UINT height)
{
    if (width == m_Width && height == m_Height) return;
    if (width == 0 || height == 0) return;

    m_Width  = width;
    m_Height = height;

    // 텍스처 + 디스크립터 재생성
    m_pFluidDepthRT.Reset();
    m_pSmoothedRT.Reset();
    m_pTempRT.Reset();
    m_pThicknessRT.Reset();
    m_pSceneColorRT.Reset();
    m_pFluidDSV.Reset();

    CreateTextures(pDevice, width, height);
    OutputDebugStringA("[SSF] OnResize 완료\n");
}

// ============================================================================
// CreateTextures
// ============================================================================
void ScreenSpaceFluid::CreateTextures(ID3D12Device* pDevice, UINT width, UINT height)
{
    // ---- RTV 디스크립터 힙 (4개): FluidDepth(0), Smoothed(1), Temp(2), Thickness(3) ----
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = 4;
        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pRTVHeap));
    }

    // ---- DSV 디스크립터 힙 (1개) ----
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = 1;
        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pDSVHeap));
    }

    // ---- SRV/UAV 디스크립터 힙 (7개, SHADER_VISIBLE) ----
    // 레이아웃: FluidDepth(0), Temp(1), Smoothed(2), Thickness(3), SceneColor(4), TempUAV(5), SmoothedUAV(6)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = 7;
        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pSRVHeap));
    }

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    // ---- R32_FLOAT 텍스처 3개 (FluidDepth, Smoothed, Temp) ----
    // uavIndex >= 0 이면 추가 UAV 디스크립터도 생성
    auto CreateR32FloatRT = [&](ComPtr<ID3D12Resource>& outTex, int rtvIndex, int srvIndex, const wchar_t* name, int uavIndex = -1)
    {
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width              = width;
        texDesc.Height             = height;
        texDesc.DepthOrArraySize   = 1;
        texDesc.MipLevels          = 1;
        texDesc.Format             = DXGI_FORMAT_R32_FLOAT;
        texDesc.SampleDesc.Count   = 1;
        texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        D3D12_CLEAR_VALUE clearVal = {};
        clearVal.Format   = DXGI_FORMAT_R32_FLOAT;
        clearVal.Color[0] = 0.0f;
        clearVal.Color[1] = 0.0f;
        clearVal.Color[2] = 0.0f;
        clearVal.Color[3] = 0.0f;

        HRESULT hr = pDevice->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE,
            &texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET,
            &clearVal, IID_PPV_ARGS(&outTex));
        if (FAILED(hr)) { OutputDebugStringA("[SSF] R32_FLOAT RT 생성 실패\n"); return; }
        outTex->SetName(name);

        // RTV
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += rtvIndex * m_RTVIncrSize;
        pDevice->CreateRenderTargetView(outTex.Get(), nullptr, rtvHandle);

        // SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                        = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels           = 1;
        srvDesc.Texture2D.MostDetailedMip     = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_pSRVHeap->GetCPUDescriptorHandleForHeapStart();
        srvHandle.ptr += srvIndex * m_SRVIncrSize;
        pDevice->CreateShaderResourceView(outTex.Get(), &srvDesc, srvHandle);

        // UAV 디스크립터 (CS blur용)
        if (uavIndex >= 0)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format               = DXGI_FORMAT_R32_FLOAT;
            uavDesc.ViewDimension        = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice   = 0;
            uavDesc.Texture2D.PlaneSlice = 0;

            D3D12_CPU_DESCRIPTOR_HANDLE uavHandle = m_pSRVHeap->GetCPUDescriptorHandleForHeapStart();
            uavHandle.ptr += uavIndex * m_SRVIncrSize;
            pDevice->CreateUnorderedAccessView(outTex.Get(), nullptr, &uavDesc, uavHandle);
        }
    };

    // SRV 힙 레이아웃: FluidDepth(0), Temp(1), Smoothed(2), Thickness(3), SceneColor(4), TempUAV(5), SmoothedUAV(6)
    // RTV 힙 레이아웃: FluidDepth(0), Smoothed(1), Temp(2), Thickness(3)
    CreateR32FloatRT(m_pFluidDepthRT, 0, 0, L"SSF_FluidDepthRT");           // RTV=0, SRV=0, UAV 없음
    CreateR32FloatRT(m_pTempRT,       2, 1, L"SSF_TempRT",       5);        // RTV=2, SRV=1, UAV=5
    CreateR32FloatRT(m_pSmoothedRT,   1, 2, L"SSF_SmoothedRT",   6);        // RTV=1, SRV=2, UAV=6

    // ---- R16_FLOAT ThicknessRT (additive blend) ----
    {
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width              = width;
        texDesc.Height             = height;
        texDesc.DepthOrArraySize   = 1;
        texDesc.MipLevels          = 1;
        texDesc.Format             = DXGI_FORMAT_R16_FLOAT;
        texDesc.SampleDesc.Count   = 1;
        texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearVal = {};
        clearVal.Format   = DXGI_FORMAT_R16_FLOAT;
        clearVal.Color[0] = 0.0f;
        clearVal.Color[1] = 0.0f;
        clearVal.Color[2] = 0.0f;
        clearVal.Color[3] = 0.0f;

        HRESULT hr = pDevice->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE,
            &texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET,
            &clearVal, IID_PPV_ARGS(&m_pThicknessRT));
        if (FAILED(hr)) { OutputDebugStringA("[SSF] ThicknessRT 생성 실패\n"); return; }
        m_pThicknessRT->SetName(L"SSF_ThicknessRT");

        // RTV (index 3)
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += 3 * m_RTVIncrSize;
        pDevice->CreateRenderTargetView(m_pThicknessRT.Get(), nullptr, rtvHandle);

        // SRV (index 3)
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                        = DXGI_FORMAT_R16_FLOAT;
        srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels           = 1;
        srvDesc.Texture2D.MostDetailedMip     = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_pSRVHeap->GetCPUDescriptorHandleForHeapStart();
        srvHandle.ptr += 3 * m_SRVIncrSize;
        pDevice->CreateShaderResourceView(m_pThicknessRT.Get(), &srvDesc, srvHandle);
    }

    // ---- R8G8B8A8_UNORM SceneColorRT (굴절 배경, CopyResource로 채움) ----
    {
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width              = width;
        texDesc.Height             = height;
        texDesc.DepthOrArraySize   = 1;
        texDesc.MipLevels          = 1;
        texDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count   = 1;
        texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;  // CopyResource 대상이므로 RT 플래그 불필요

        HRESULT hr = pDevice->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE,
            &texDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            nullptr, IID_PPV_ARGS(&m_pSceneColorRT));
        if (FAILED(hr)) { OutputDebugStringA("[SSF] SceneColorRT 생성 실패\n"); return; }
        m_pSceneColorRT->SetName(L"SSF_SceneColorRT");
        m_eSceneColorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        // SRV (index 4)
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                        = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels           = 1;
        srvDesc.Texture2D.MostDetailedMip     = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_pSRVHeap->GetCPUDescriptorHandleForHeapStart();
        srvHandle.ptr += 4 * m_SRVIncrSize;
        pDevice->CreateShaderResourceView(m_pSceneColorRT.Get(), &srvDesc, srvHandle);
    }

    // ---- D32_FLOAT Depth Stencil ----
    {
        D3D12_RESOURCE_DESC dsDesc = {};
        dsDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        dsDesc.Width              = width;
        dsDesc.Height             = height;
        dsDesc.DepthOrArraySize   = 1;
        dsDesc.MipLevels          = 1;
        dsDesc.Format             = DXGI_FORMAT_D32_FLOAT;
        dsDesc.SampleDesc.Count   = 1;
        dsDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        dsDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE dsClear = {};
        dsClear.Format               = DXGI_FORMAT_D32_FLOAT;
        dsClear.DepthStencil.Depth   = 1.0f;
        dsClear.DepthStencil.Stencil = 0;

        HRESULT hr = pDevice->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE,
            &dsDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &dsClear, IID_PPV_ARGS(&m_pFluidDSV));
        if (FAILED(hr)) { OutputDebugStringA("[SSF] D32_FLOAT DSV 생성 실패\n"); return; }
        m_pFluidDSV->SetName(L"SSF_FluidDSV");

        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pDSVHeap->GetCPUDescriptorHandleForHeapStart();
        pDevice->CreateDepthStencilView(m_pFluidDSV.Get(), nullptr, dsvHandle);
    }
}

// ============================================================================
// CreatePipelines
// ============================================================================
void ScreenSpaceFluid::CreatePipelines(ID3D12Device* pDevice)
{
    HRESULT hr;
    ComPtr<ID3DBlob> errBlob;

    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // ================================================================
    // Pass 1: Sphere Depth + Thickness PSO
    // ================================================================
    {
        // Root signature: param[0]=CBV b0 (ALL), param[1]=descriptor table t0 SRV (VS)
        D3D12_ROOT_PARAMETER rootParams[2] = {};

        rootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[0].Descriptor.ShaderRegister = 0;  // b0
        rootParams[0].Descriptor.RegisterSpace  = 0;
        rootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

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

        ComPtr<ID3DBlob> sigBlob;
        hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            OutputDebugStringA("[SSF] Depth root sig 직렬화 실패\n");
            return;
        }
        hr = pDevice->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
                                           IID_PPV_ARGS(&m_pDepthRootSig));
        if (FAILED(hr)) { OutputDebugStringA("[SSF] Depth root sig 생성 실패\n"); return; }

        // 셰이더 컴파일 (VS는 두 PSO에서 공유)
        ComPtr<ID3DBlob> vsBlob, psBlob;
        errBlob.Reset();
        hr = D3DCompile(g_SphereDepthShader, strlen(g_SphereDepthShader), "SphereDepthShader",
                         nullptr, nullptr, "SphereDepth_VS", "vs_5_1", compileFlags, 0, &vsBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            OutputDebugStringA("[SSF] SphereDepth VS 컴파일 실패\n");
            return;
        }
        errBlob.Reset();
        hr = D3DCompile(g_SphereDepthShader, strlen(g_SphereDepthShader), "SphereDepthShader",
                         nullptr, nullptr, "DepthOnly_PS", "ps_5_1", compileFlags, 0, &psBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            OutputDebugStringA("[SSF] DepthOnly PS 컴파일 실패\n");
            return;
        }

        // Pass 1a PSO: FluidDepthRT 단독 (블렌드 없음, 깊이 테스트 있음)
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_pDepthRootSig.Get();
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        psoDesc.InputLayout = { nullptr, 0 };

        psoDesc.BlendState.AlphaToCoverageEnable  = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        psoDesc.BlendState.RenderTarget[0].BlendEnable           = FALSE;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        psoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
        psoDesc.RasterizerState.DepthClipEnable       = TRUE;

        psoDesc.DepthStencilState.DepthEnable    = TRUE;
        psoDesc.DepthStencilState.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

        psoDesc.SampleMask            = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets      = 1;
        psoDesc.RTVFormats[0]         = DXGI_FORMAT_R32_FLOAT;   // FluidDepth
        psoDesc.DSVFormat             = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count      = 1;

        hr = pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pDepthPSO));
        if (FAILED(hr)) { OutputDebugStringA("[SSF] Depth PSO 생성 실패\n"); return; }

        // Pass 1b PSO: ThicknessRT 단독 (가산 블렌딩, 깊이 테스트 없음)
        // — 모든 파티클이 두께에 기여: 카메라 각도에 무관하게 대칭적인 두께 보장
        {
            ComPtr<ID3DBlob> thickPS;
            errBlob.Reset();
            hr = D3DCompile(g_SphereDepthShader, strlen(g_SphereDepthShader), "SphereDepthShader",
                             nullptr, nullptr, "Thickness_PS", "ps_5_1", compileFlags, 0, &thickPS, &errBlob);
            if (FAILED(hr))
            {
                if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
                OutputDebugStringA("[SSF] Thickness PS 컴파일 실패\n");
                return;
            }

            D3D12_GRAPHICS_PIPELINE_STATE_DESC tDesc = {};
            tDesc.pRootSignature = m_pDepthRootSig.Get();
            tDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
            tDesc.PS = { thickPS->GetBufferPointer(), thickPS->GetBufferSize() };
            tDesc.InputLayout = { nullptr, 0 };

            tDesc.BlendState.AlphaToCoverageEnable  = FALSE;
            tDesc.BlendState.IndependentBlendEnable = FALSE;
            // ThicknessRT: 가산 블렌딩 (모든 구체의 chord 누적)
            auto& rt1 = tDesc.BlendState.RenderTarget[0];
            rt1.BlendEnable           = TRUE;
            rt1.SrcBlend              = D3D12_BLEND_ONE;
            rt1.DestBlend             = D3D12_BLEND_ONE;
            rt1.BlendOp               = D3D12_BLEND_OP_ADD;
            rt1.SrcBlendAlpha         = D3D12_BLEND_ONE;
            rt1.DestBlendAlpha        = D3D12_BLEND_ONE;
            rt1.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
            rt1.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

            tDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
            tDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
            tDesc.RasterizerState.FrontCounterClockwise = FALSE;
            tDesc.RasterizerState.DepthClipEnable       = FALSE;  // 깊이 클립 없음

            tDesc.DepthStencilState.DepthEnable = FALSE;  // 깊이 테스트 없음!

            tDesc.SampleMask            = UINT_MAX;
            tDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            tDesc.NumRenderTargets      = 1;
            tDesc.RTVFormats[0]         = DXGI_FORMAT_R16_FLOAT;   // ThicknessRT
            tDesc.DSVFormat             = DXGI_FORMAT_UNKNOWN;
            tDesc.SampleDesc.Count      = 1;

            hr = pDevice->CreateGraphicsPipelineState(&tDesc, IID_PPV_ARGS(&m_pThicknessPSO));
            if (FAILED(hr)) { OutputDebugStringA("[SSF] Thickness PSO 생성 실패\n"); return; }
        }
    }

    // ================================================================
    // Pass 2: Bilateral Smooth PSO
    // ================================================================
    {
        // Root signature: param[0]=CBV b0 (ALL), param[1]=descriptor table t0 SRV (PS), static sampler s0
        D3D12_ROOT_PARAMETER rootParams[2] = {};

        rootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[0].Descriptor.ShaderRegister = 0;
        rootParams[0].Descriptor.RegisterSpace  = 0;
        rootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_DESCRIPTOR_RANGE srvRange = {};
        srvRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors                    = 1;
        srvRange.BaseShaderRegister                = 0;
        srvRange.RegisterSpace                     = 0;
        srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        rootParams[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[1].DescriptorTable.pDescriptorRanges   = &srvRange;
        rootParams[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

        // Static sampler (point clamp)
        D3D12_STATIC_SAMPLER_DESC staticSampler = {};
        staticSampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
        staticSampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSampler.ShaderRegister   = 0;
        staticSampler.RegisterSpace    = 0;
        staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        staticSampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
        staticSampler.MaxLOD           = D3D12_FLOAT32_MAX;

        D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
        rsDesc.NumParameters     = 2;
        rsDesc.pParameters       = rootParams;
        rsDesc.NumStaticSamplers = 1;
        rsDesc.pStaticSamplers   = &staticSampler;
        rsDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> sigBlob;
        errBlob.Reset();
        hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            return;
        }
        hr = pDevice->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
                                           IID_PPV_ARGS(&m_pSmoothRootSig));
        if (FAILED(hr)) { OutputDebugStringA("[SSF] Smooth root sig 생성 실패\n"); return; }

        ComPtr<ID3DBlob> vsBlob, psBlob;
        errBlob.Reset();
        hr = D3DCompile(g_SmoothShader, strlen(g_SmoothShader), "SmoothShader",
                         nullptr, nullptr, "Fullscreen_VS", "vs_5_1", compileFlags, 0, &vsBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            OutputDebugStringA("[SSF] Smooth VS 컴파일 실패\n");
            return;
        }
        errBlob.Reset();
        hr = D3DCompile(g_SmoothShader, strlen(g_SmoothShader), "SmoothShader",
                         nullptr, nullptr, "Smooth_PS", "ps_5_1", compileFlags, 0, &psBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            OutputDebugStringA("[SSF] Smooth PS 컴파일 실패\n");
            return;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_pSmoothRootSig.Get();
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        psoDesc.InputLayout = { nullptr, 0 };

        psoDesc.BlendState.AlphaToCoverageEnable  = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        psoDesc.BlendState.RenderTarget[0].BlendEnable    = FALSE;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        psoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
        psoDesc.RasterizerState.DepthClipEnable       = FALSE;

        psoDesc.DepthStencilState.DepthEnable = FALSE;

        psoDesc.SampleMask            = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets      = 1;
        psoDesc.RTVFormats[0]         = DXGI_FORMAT_R32_FLOAT;
        psoDesc.DSVFormat             = DXGI_FORMAT_UNKNOWN;
        psoDesc.SampleDesc.Count      = 1;

        hr = pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pSmoothPSO));
        if (FAILED(hr)) { OutputDebugStringA("[SSF] Smooth PSO 생성 실패\n"); return; }
    }

    // ================================================================
    // Pass 2 (CS): Bilateral Blur Compute Shader
    // ================================================================
    {
        // Blur CS 셰이더 코드
        static const char* blurCSCode = R"(
            cbuffer BlurCB : register(b0) {
                int2  gTexSize;
                float gSigmaDepth;
                int   gIsHorizontal;
            };

            Texture2D<float>   gDepthIn  : register(t0);
            RWTexture2D<float> gDepthOut : register(u0);

            #define BLUR_RADIUS 6
            #define BLUR_STEP   4
            #define CACHE_SIZE  (128 + 2*BLUR_RADIUS*BLUR_STEP)

            static const float kGauss[13] = {
                0.011254f, 0.026995f, 0.052665f, 0.088542f, 0.128420f, 0.160693f, 0.172860f,
                0.160693f, 0.128420f, 0.088542f, 0.052665f, 0.026995f, 0.011254f
            };

            groupshared float gCache[CACHE_SIZE];

            [numthreads(128, 1, 1)]
            void CS_Blur(uint3 groupID : SV_GroupID, uint localIdx : SV_GroupIndex)
            {
                int2 baseCoord;
                int2 sampleDir;
                if (gIsHorizontal) {
                    baseCoord = int2(groupID.x * 128, groupID.y);
                    sampleDir = int2(1, 0);
                } else {
                    baseCoord = int2(groupID.x, groupID.y * 128);
                    sampleDir = int2(0, 1);
                }

                int halo = BLUR_RADIUS * BLUR_STEP;
                int totalLoad = CACHE_SIZE;
                for (int k = (int)localIdx; k < totalLoad; k += 128) {
                    int offset = k - halo;
                    int2 coord = baseCoord + sampleDir * offset;
                    coord = clamp(coord, int2(0,0), gTexSize - 1);
                    gCache[k] = gDepthIn[coord];
                }
                GroupMemoryBarrierWithGroupSync();

                int2 outCoord = baseCoord + sampleDir * (int)localIdx;
                if (outCoord.x >= gTexSize.x || outCoord.y >= gTexSize.y) return;

                float centerDepth = gCache[localIdx + halo];

                // 빈 픽셀은 이웃 유체를 샘플링하지 않음 — 외곽 선 아티팩트 방지
                if (centerDepth < 0.001f) {
                    gDepthOut[outCoord] = 0.0f;
                    return;
                }

                float totalW = 0.0f, totalD = 0.0f;
                float sigmaD2 = 2.0f * gSigmaDepth * gSigmaDepth;

                [unroll]
                for (int i = -BLUR_RADIUS; i <= BLUR_RADIUS; ++i) {
                    int cacheIdx = (int)localIdx + halo + i * BLUR_STEP;
                    float s = gCache[cacheIdx];
                    if (s < 0.001f) continue;

                    float d = s - centerDepth;
                    float bilW = exp(-(d*d) / sigmaD2);
                    float w = kGauss[i + BLUR_RADIUS] * bilW;
                    totalW += w;
                    totalD += s * w;
                }
                gDepthOut[outCoord] = (totalW > 0.0001f) ? (totalD / totalW) : 0.0f;
            }
        )";

        // CS Blur 루트 시그니처: [0] CBV b0, [1] table SRV t0, [2] table UAV u0
        D3D12_ROOT_PARAMETER csRootParams[3] = {};

        csRootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
        csRootParams[0].Descriptor.ShaderRegister = 0;
        csRootParams[0].Descriptor.RegisterSpace  = 0;
        csRootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_DESCRIPTOR_RANGE srvRng = {};
        srvRng.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRng.NumDescriptors                    = 1;
        srvRng.BaseShaderRegister                = 0;
        srvRng.RegisterSpace                     = 0;
        srvRng.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        csRootParams[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        csRootParams[1].DescriptorTable.NumDescriptorRanges = 1;
        csRootParams[1].DescriptorTable.pDescriptorRanges   = &srvRng;
        csRootParams[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_DESCRIPTOR_RANGE uavRng = {};
        uavRng.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRng.NumDescriptors                    = 1;
        uavRng.BaseShaderRegister                = 0;
        uavRng.RegisterSpace                     = 0;
        uavRng.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        csRootParams[2].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        csRootParams[2].DescriptorTable.NumDescriptorRanges = 1;
        csRootParams[2].DescriptorTable.pDescriptorRanges   = &uavRng;
        csRootParams[2].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC csRsDesc = {};
        csRsDesc.NumParameters     = 3;
        csRsDesc.pParameters       = csRootParams;
        csRsDesc.NumStaticSamplers = 0;
        csRsDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> csSigBlob;
        errBlob.Reset();
        hr = D3D12SerializeRootSignature(&csRsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &csSigBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            OutputDebugStringA("[SSF] Blur CS root sig 직렬화 실패\n");
            return;
        }
        hr = pDevice->CreateRootSignature(0, csSigBlob->GetBufferPointer(), csSigBlob->GetBufferSize(),
                                           IID_PPV_ARGS(&m_pBlurCSRootSig));
        if (FAILED(hr)) { OutputDebugStringA("[SSF] Blur CS root sig 생성 실패\n"); return; }

        // CS 컴파일
        ComPtr<ID3DBlob> csBlob;
        errBlob.Reset();
        hr = D3DCompile(blurCSCode, strlen(blurCSCode), "BlurCSShader",
                         nullptr, nullptr, "CS_Blur", "cs_5_1", compileFlags, 0, &csBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            OutputDebugStringA("[SSF] Blur CS 컴파일 실패\n");
            return;
        }

        // H blur PSO
        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC cpsoDesc = {};
            cpsoDesc.pRootSignature = m_pBlurCSRootSig.Get();
            cpsoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
            hr = pDevice->CreateComputePipelineState(&cpsoDesc, IID_PPV_ARGS(&m_pBlurHPSO));
            if (FAILED(hr)) { OutputDebugStringA("[SSF] Blur H PSO 생성 실패\n"); return; }
        }
        // V blur PSO (동일한 셰이더, CB의 gIsHorizontal로 분기)
        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC cpsoDesc = {};
            cpsoDesc.pRootSignature = m_pBlurCSRootSig.Get();
            cpsoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
            hr = pDevice->CreateComputePipelineState(&cpsoDesc, IID_PPV_ARGS(&m_pBlurVPSO));
            if (FAILED(hr)) { OutputDebugStringA("[SSF] Blur V PSO 생성 실패\n"); return; }
        }

        OutputDebugStringA("[SSF] CS Blur 파이프라인 생성 완료\n");
    }

    // ================================================================
    // Pass 3: Composite PSO (Sebastian Lague 스타일)
    // ================================================================
    {
        // Root signature: param[0]=CBV b0 (ALL), param[1]=descriptor table 3 SRVs t0-t2 (PS)
        // Static sampler s0=point clamp, s1=linear clamp
        D3D12_ROOT_PARAMETER rootParams[2] = {};

        rootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[0].Descriptor.ShaderRegister = 0;
        rootParams[0].Descriptor.RegisterSpace  = 0;
        rootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

        // 3개 연속 SRV: t0=SmoothedDepth, t1=Thickness, t2=SceneColor
        D3D12_DESCRIPTOR_RANGE srvRange = {};
        srvRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors                    = 3;
        srvRange.BaseShaderRegister                = 0;  // t0
        srvRange.RegisterSpace                     = 0;
        srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        rootParams[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[1].DescriptorTable.pDescriptorRanges   = &srvRange;
        rootParams[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

        // 2개 static sampler: s0=point clamp, s1=linear clamp
        D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};

        // s0: 포인트 클램프 (깊이/두께 샘플링)
        staticSamplers[0].Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
        staticSamplers[0].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[0].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[0].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[0].ShaderRegister   = 0;
        staticSamplers[0].RegisterSpace    = 0;
        staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        staticSamplers[0].ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
        staticSamplers[0].MaxLOD           = D3D12_FLOAT32_MAX;

        // s1: 리니어 클램프 (굴절 배경 샘플링)
        staticSamplers[1].Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSamplers[1].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[1].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[1].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[1].ShaderRegister   = 1;
        staticSamplers[1].RegisterSpace    = 0;
        staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        staticSamplers[1].ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
        staticSamplers[1].MaxLOD           = D3D12_FLOAT32_MAX;

        D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
        rsDesc.NumParameters     = 2;
        rsDesc.pParameters       = rootParams;
        rsDesc.NumStaticSamplers = 2;
        rsDesc.pStaticSamplers   = staticSamplers;
        rsDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> sigBlob;
        errBlob.Reset();
        hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            return;
        }
        hr = pDevice->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
                                           IID_PPV_ARGS(&m_pCompositeRootSig));
        if (FAILED(hr)) { OutputDebugStringA("[SSF] Composite root sig 생성 실패\n"); return; }

        ComPtr<ID3DBlob> vsBlob, psBlob;
        errBlob.Reset();
        hr = D3DCompile(g_CompositeShader, strlen(g_CompositeShader), "CompositeShader",
                         nullptr, nullptr, "Composite_Fullscreen_VS", "vs_5_1", compileFlags, 0, &vsBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            OutputDebugStringA("[SSF] Composite VS 컴파일 실패\n");
            return;
        }
        errBlob.Reset();
        hr = D3DCompile(g_CompositeShader, strlen(g_CompositeShader), "CompositeShader",
                         nullptr, nullptr, "Composite_PS", "ps_5_1", compileFlags, 0, &psBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            OutputDebugStringA("[SSF] Composite PS 컴파일 실패\n");
            return;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_pCompositeRootSig.Get();
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        psoDesc.InputLayout = { nullptr, 0 };

        // Alpha blend (메인 RT에 합성)
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
        psoDesc.RasterizerState.DepthClipEnable       = FALSE;

        // 씬 DSV와 깊이 비교 (캐릭터/오브젝트 뒤 유체 가림), 깊이 쓰기는 하지 않음
        psoDesc.DepthStencilState.DepthEnable      = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc        = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DepthStencilState.StencilEnable    = FALSE;

        psoDesc.SampleMask            = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets      = 1;
        psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat             = DXGI_FORMAT_D24_UNORM_S8_UINT;
        psoDesc.SampleDesc.Count      = 1;

        hr = pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pCompositePSO));
        if (FAILED(hr)) { OutputDebugStringA("[SSF] Composite PSO 생성 실패\n"); return; }
    }

    OutputDebugStringA("[SSF] 파이프라인 생성 완료 (Sebastian Lague 스타일)\n");
}

// ============================================================================
// CaptureSceneColor - 메인 RT를 SceneColorRT로 복사 (굴절 배경)
// ============================================================================
void ScreenSpaceFluid::CaptureSceneColor(
    ID3D12GraphicsCommandList* pCmdList,
    ID3D12Resource* pMainRTBuffer)
{
    if (!pMainRTBuffer || !m_pSceneColorRT) return;

    // 메인 RT: RENDER_TARGET -> COPY_SOURCE
    // SceneColorRT: 현재 상태 -> COPY_DEST
    D3D12_RESOURCE_BARRIER barriers[2] = {};

    barriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[0].Transition.pResource   = pMainRTBuffer;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    barriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[1].Transition.pResource   = m_pSceneColorRT.Get();
    barriers[1].Transition.StateBefore = m_eSceneColorState;
    barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    pCmdList->ResourceBarrier(2, barriers);

    // 리소스 복사
    pCmdList->CopyResource(m_pSceneColorRT.Get(), pMainRTBuffer);

    // 상태 복원: 메인 RT -> RENDER_TARGET, SceneColorRT -> PIXEL_SHADER_RESOURCE
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    pCmdList->ResourceBarrier(2, barriers);

    m_eSceneColorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

// ============================================================================
// BeginDepthPass
// ============================================================================
void ScreenSpaceFluid::BeginDepthPass(ID3D12GraphicsCommandList* pCmdList)
{
    // FluidDepthRT 클리어 (RTV index 0)
    D3D12_CPU_DESCRIPTOR_HANDLE depthRTV = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    pCmdList->ClearRenderTargetView(depthRTV, clearColor, 0, nullptr);

    // ThicknessRT 클리어 (RTV index 3)
    D3D12_CPU_DESCRIPTOR_HANDLE thicknessRTV = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();
    thicknessRTV.ptr += 3 * m_RTVIncrSize;
    pCmdList->ClearRenderTargetView(thicknessRTV, clearColor, 0, nullptr);

    // FluidDSV 클리어
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pDSVHeap->GetCPUDescriptorHandleForHeapStart();
    pCmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // 뷰포트/시저렉트 설정
    D3D12_VIEWPORT vp = { 0, 0, (FLOAT)m_Width, (FLOAT)m_Height, 0.0f, 1.0f };
    pCmdList->RSSetViewports(1, &vp);
    D3D12_RECT sr = { 0, 0, (LONG)m_Width, (LONG)m_Height };
    pCmdList->RSSetScissorRects(1, &sr);

    // RT 바인딩: FluidDepthRT 단독 + DSV (깊이 테스트 있음)
    // ThicknessRT는 BeginThicknessPass에서 별도 패스로 바인딩
    pCmdList->OMSetRenderTargets(1, &depthRTV, FALSE, &dsvHandle);
}

// ============================================================================
// BeginThicknessPass
// ============================================================================
void ScreenSpaceFluid::BeginThicknessPass(ID3D12GraphicsCommandList* pCmdList)
{
    // ThicknessRT는 BeginDepthPass에서 이미 클리어됨 (여전히 RENDER_TARGET 상태)
    // DSV 없이 ThicknessRT만 바인딩 — 깊이 테스트 없이 모든 파티클이 두께에 기여
    D3D12_CPU_DESCRIPTOR_HANDLE thicknessRTV = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();
    thicknessRTV.ptr += 3 * m_RTVIncrSize;
    pCmdList->OMSetRenderTargets(1, &thicknessRTV, FALSE, nullptr);
}

// ============================================================================
// EndDepthPass
// ============================================================================
void ScreenSpaceFluid::EndDepthPass(ID3D12GraphicsCommandList* pCmdList)
{
    // FluidDepthRT: RENDER_TARGET -> PIXEL_SHADER_RESOURCE
    // ThicknessRT: RENDER_TARGET -> PIXEL_SHADER_RESOURCE
    D3D12_RESOURCE_BARRIER barriers[2] = {};

    barriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[0].Transition.pResource   = m_pFluidDepthRT.Get();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    barriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[1].Transition.pResource   = m_pThicknessRT.Get();
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    pCmdList->ResourceBarrier(2, barriers);
}

// ============================================================================
// SmoothAndComposite
// ============================================================================
void ScreenSpaceFluid::SmoothAndComposite(
    ID3D12GraphicsCommandList* pCmdList,
    D3D12_CPU_DESCRIPTOR_HANDLE mainRTV,
    D3D12_CPU_DESCRIPTOR_HANDLE mainDSV,
    const XMFLOAT4X4& proj,
    const XMFLOAT3& lightDirVS,
    const XMFLOAT4& fluidColorOuter,
    const XMFLOAT4& fluidColorInner)
{
    float texelW = 1.0f / (float)m_Width;
    float texelH = 1.0f / (float)m_Height;

    // ---- SRV 힙 전환 (SSF 전용 SRV 힙 바인딩) ----
    ID3D12DescriptorHeap* ppHeaps[] = { m_pSRVHeap.Get() };
    pCmdList->SetDescriptorHeaps(1, ppHeaps);

    // ---- CB 업데이트 ----
    // BlurCB_H (offset 0): CS blur 수평
    struct BlurCB {
        int   texSizeX; int texSizeY;
        float sigmaDepth;
        int   isHorizontal;
    };
    {
        BlurCB cb = {};
        cb.texSizeX     = (int)m_Width;
        cb.texSizeY     = (int)m_Height;
        cb.sigmaDepth   = 3.5f;
        cb.isHorizontal = 1;
        memcpy(m_pMappedCB + 0, &cb, sizeof(cb));
    }
    // BlurCB_V (offset 256): CS blur 수직
    {
        BlurCB cb = {};
        cb.texSizeX     = (int)m_Width;
        cb.texSizeY     = (int)m_Height;
        cb.sigmaDepth   = 3.5f;
        cb.isHorizontal = 0;
        memcpy(m_pMappedCB + 256, &cb, sizeof(cb));
    }
    // CompositeCB (offset 512)
    {
        CompositeCB cb = {};
        cb.invProjX       = 1.0f / proj._11;
        cb.invProjY       = 1.0f / proj._22;
        cb.screenWidth    = (float)m_Width;
        cb.screenHeight   = (float)m_Height;
        cb.lightDirVS     = lightDirVS;
        cb.refractionStr  = 0.04f;

        // 흡수 계수: outer(코로나) 색상 기반 — 보색 채널을 약하게 흡수
        float maxComp = (std::max)({ fluidColorOuter.x, fluidColorOuter.y, fluidColorOuter.z, 0.01f });
        cb.absorption = {
            (1.0f - fluidColorOuter.x / maxComp) * 0.8f + 0.05f,
            (1.0f - fluidColorOuter.y / maxComp) * 0.8f + 0.05f,
            (1.0f - fluidColorOuter.z / maxComp) * 0.8f + 0.05f
        };

        cb.projA           = proj._33;
        cb.specularColor   = { 1.0f, 0.95f, 0.8f, 0.9f };   // 태양빛 색 스페큘러
        cb.fluidColorOuter = fluidColorOuter;
        cb.fluidColorInner = fluidColorInner;
        cb.projB           = proj._43;
        memcpy(m_pMappedCB + 512, &cb, sizeof(cb));
    }

    D3D12_GPU_VIRTUAL_ADDRESS cbBase = m_pCBUpload->GetGPUVirtualAddress();

    // 뷰포트/시저렉트
    D3D12_VIEWPORT vp = { 0, 0, (FLOAT)m_Width, (FLOAT)m_Height, 0.0f, 1.0f };
    pCmdList->RSSetViewports(1, &vp);
    D3D12_RECT sr = { 0, 0, (LONG)m_Width, (LONG)m_Height };
    pCmdList->RSSetScissorRects(1, &sr);

    // ================================================================
    // Pass 2a (CS): 수평 blur (FluidDepthRT -> TempRT)
    // ================================================================
    if (m_pBlurCSRootSig && m_pBlurHPSO)
    {
        // FluidDepthRT: PSR -> NON_PIXEL_SR (CS에서 SRV로 읽기)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource   = m_pFluidDepthRT.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            pCmdList->ResourceBarrier(1, &barrier);
        }
        // TempRT: RENDER_TARGET -> UAV
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource   = m_pTempRT.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            pCmdList->ResourceBarrier(1, &barrier);
        }

        pCmdList->SetComputeRootSignature(m_pBlurCSRootSig.Get());
        pCmdList->SetPipelineState(m_pBlurHPSO.Get());

        pCmdList->SetComputeRootConstantBufferView(0, cbBase + 0);  // BlurCB_H

        // SRV: FluidDepthRT (index 0)
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = m_pSRVHeap->GetGPUDescriptorHandleForHeapStart();
        pCmdList->SetComputeRootDescriptorTable(1, srvHandle);

        // UAV: TempRT (index 5)
        D3D12_GPU_DESCRIPTOR_HANDLE uavHandle = m_pSRVHeap->GetGPUDescriptorHandleForHeapStart();
        uavHandle.ptr += 5 * m_SRVIncrSize;
        pCmdList->SetComputeRootDescriptorTable(2, uavHandle);

        // Dispatch: 수평 = ceil(W/128) groups x H groups
        UINT groupsX = (m_Width + 127) / 128;
        pCmdList->Dispatch(groupsX, m_Height, 1);

        // TempRT: UAV -> NON_PIXEL_SHADER_RESOURCE
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource   = m_pTempRT.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            pCmdList->ResourceBarrier(1, &barrier);
        }
    }

    // ================================================================
    // Pass 2b (CS): 수직 blur (TempRT -> SmoothedRT)
    // ================================================================
    if (m_pBlurCSRootSig && m_pBlurVPSO)
    {
        // SmoothedRT: RENDER_TARGET -> UAV
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource   = m_pSmoothedRT.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            pCmdList->ResourceBarrier(1, &barrier);
        }

        pCmdList->SetComputeRootSignature(m_pBlurCSRootSig.Get());
        pCmdList->SetPipelineState(m_pBlurVPSO.Get());

        pCmdList->SetComputeRootConstantBufferView(0, cbBase + 256);  // BlurCB_V

        // SRV: TempRT (index 1)
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = m_pSRVHeap->GetGPUDescriptorHandleForHeapStart();
        srvHandle.ptr += 1 * m_SRVIncrSize;
        pCmdList->SetComputeRootDescriptorTable(1, srvHandle);

        // UAV: SmoothedRT (index 6)
        D3D12_GPU_DESCRIPTOR_HANDLE uavHandle = m_pSRVHeap->GetGPUDescriptorHandleForHeapStart();
        uavHandle.ptr += 6 * m_SRVIncrSize;
        pCmdList->SetComputeRootDescriptorTable(2, uavHandle);

        // Dispatch: 수직 = W groups x ceil(H/128) groups
        UINT groupsY = (m_Height + 127) / 128;
        pCmdList->Dispatch(m_Width, groupsY, 1);

        // SmoothedRT: UAV -> PIXEL_SHADER_RESOURCE (Composite pass에서 PS SRV로 읽기)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource   = m_pSmoothedRT.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            pCmdList->ResourceBarrier(1, &barrier);
        }
        // TempRT: NON_PIXEL_SR -> PIXEL_SR (다음 프레임 복원 대비)
        // -> 사실 이 시점에서 Composite가 TempRT를 안 읽으므로, 나중에 RENDER_TARGET로 복원 필요
    }

    // ================================================================
    // Pass 3: Composite (SmoothedRT + ThicknessRT + SceneColorRT -> mainRTV)
    // SRV 힙 인덱스: Smoothed(2), Thickness(3), SceneColor(4) -> 연속 3개
    // ================================================================
    {
        pCmdList->OMSetRenderTargets(1, &mainRTV, FALSE, &mainDSV);

        pCmdList->SetPipelineState(m_pCompositePSO.Get());
        pCmdList->SetGraphicsRootSignature(m_pCompositeRootSig.Get());

        pCmdList->SetGraphicsRootConstantBufferView(0, cbBase + 512);  // CompositeCB

        // SRV: Smoothed(2) + Thickness(3) + SceneColor(4) - 연속 3개
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = m_pSRVHeap->GetGPUDescriptorHandleForHeapStart();
        srvHandle.ptr += 2 * m_SRVIncrSize;  // SRV 인덱스 2부터 시작
        pCmdList->SetGraphicsRootDescriptorTable(1, srvHandle);

        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->DrawInstanced(3, 1, 0, 0);
    }

    // ================================================================
    // 상태 복원: 다음 프레임을 위해 모든 RT를 RENDER_TARGET로 복원
    // ================================================================
    {
        D3D12_RESOURCE_BARRIER barriers[4] = {};

        // FluidDepthRT: NON_PIXEL_SHADER_RESOURCE -> RENDER_TARGET
        // (CS blur에서 NON_PIXEL_SR로 전환됨, CS blur 미사용 시 PSR 상태)
        barriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource   = m_pFluidDepthRT.Get();
        barriers[0].Transition.StateBefore = (m_pBlurCSRootSig) ?
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        // TempRT: NON_PIXEL_SHADER_RESOURCE -> RENDER_TARGET
        barriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource   = m_pTempRT.Get();
        barriers[1].Transition.StateBefore = (m_pBlurCSRootSig) ?
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        // SmoothedRT: PIXEL_SHADER_RESOURCE -> RENDER_TARGET
        barriers[2].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[2].Transition.pResource   = m_pSmoothedRT.Get();
        barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[2].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        // ThicknessRT: PIXEL_SHADER_RESOURCE -> RENDER_TARGET
        barriers[3].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[3].Transition.pResource   = m_pThicknessRT.Get();
        barriers[3].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[3].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[3].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        pCmdList->ResourceBarrier(4, barriers);
    }
    // SceneColorRT는 PIXEL_SHADER_RESOURCE 상태 유지 (다음 CaptureSceneColor에서 전환됨)
}
