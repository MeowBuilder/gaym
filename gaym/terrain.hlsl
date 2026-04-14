// terrain.hlsl
// Terrain 전용 셰이더 (splatmap 기반 레이어 블렌딩)
// Root Signature:
//   Param 0: CBV b0  → TerrainCB
//   Param 1: CBV b1  → PassCB
//   Param 2: SRV t0  → splatmap0 (RGBA, layer 0~3 가중치)
//   Param 3: SRV t1~t4 → layer 0~3 diffuse
//   Param 4: SRV t5~t8 → layer 0~3 normal

// ────────────────────────────────────────────────────────────────
// Constant Buffers
// ────────────────────────────────────────────────────────────────

cbuffer TerrainCB : register(b0)
{
    matrix g_World;
    // xy = tileSizeX/tileSizeZ,  zw = tileOffsetX/tileOffsetZ
    float4 g_LayerTiling[4];
    int    g_LayerCount;
    float  g_TerrainSizeX;
    float  g_TerrainSizeZ;
    float  g_Pad0;
    float  g_Pad1[28];
};

// PassCB: shaders.hlsl과 동일한 레이아웃 (필요한 필드만 사용)
#define MAX_TORCH_LIGHTS 8
struct TorchLight { float3 Position; float Range; float3 Color; float Intensity; };
struct WaveParams  { float wavelength; float amplitude; float steepness; float speed;
                     float2 direction; float fadeSpeed; float pad; };

cbuffer cbPass : register(b1)
{
    matrix g_ViewProj;
    matrix g_LightViewProj;
    float4 g_LightColor;
    float3 g_LightDirection;    float g_pad0;
    float4 g_PointLightColor;
    float3 g_PointLightPos;     float g_pad1;
    float  g_PointLightRange;   float g_pad2; float g_pad3; float g_pad4;
    float4 g_AmbientLight;
    float3 g_CameraPos;         float g_padCam;
    float4 g_SpotLightColor;
    float3 g_SpotLightPos;      float g_SpotLightRange;
    float3 g_SpotLightDir;      float g_SpotInnerCone;
    float  g_SpotOuterCone;     float g_pad5; float g_pad6; float g_pad7;
    float  g_Time;              float g_TimePad1; float g_TimePad2; float g_TimePad3;
    TorchLight g_TorchLights[MAX_TORCH_LIGHTS];
    int g_nActiveTorchLights;   int g_tp1; int g_tp2; int g_tp3;
    WaveParams g_Waves[5];
};

// ────────────────────────────────────────────────────────────────
// Textures & Samplers
// ────────────────────────────────────────────────────────────────

Texture2D gSplatmap0       : register(t0);  // layer 0~3 가중치 (RGBA)
Texture2D gLayerDiffuse[4] : register(t1);  // t1~t4: layer 0~3 베이스컬러
Texture2D gLayerNormal[4]  : register(t5);  // t5~t8: layer 0~3 노멀맵

SamplerState gSampler : register(s0);

// ────────────────────────────────────────────────────────────────
// VS/PS 인터페이스
// ────────────────────────────────────────────────────────────────

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct PS_INPUT
{
    float4 clipPos   : SV_POSITION;
    float3 worldPos  : POSITION;
    float3 normal    : NORMAL;
    float2 uv        : TEXCOORD0;   // 0~1 스플랫맵 UV
};

// ────────────────────────────────────────────────────────────────
// Vertex Shader
// 터레인 메쉬는 이미 높이가 구워진 상태 → 변위 없음
// ────────────────────────────────────────────────────────────────
PS_INPUT VS_Terrain(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = mul(float4(input.position, 1.0f), g_World);
    output.clipPos  = mul(worldPos, g_ViewProj);
    output.worldPos = worldPos.xyz;
    output.normal   = normalize(mul(input.normal, (float3x3)g_World));
    output.uv       = input.uv;

    return output;
}

// ────────────────────────────────────────────────────────────────
// Pixel Shader
// ────────────────────────────────────────────────────────────────
float4 PS_Terrain(PS_INPUT input) : SV_TARGET
{
    float3 N = normalize(input.normal);

    // ── 스플랫맵 가중치 읽기 ──
    float4 splat = gSplatmap0.Sample(gSampler, input.uv);  // RGBA = layer 0~3 가중치
    float weights[4] = { splat.r, splat.g, splat.b, splat.a };

    // ── 각 레이어 텍스처 샘플링 + 노멀 블렌딩 ──
    float3 albedo      = float3(0, 0, 0);
    float3 blendNormal = float3(0, 0, 0);

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        if (i >= g_LayerCount) break;

        // 레이어 UV: 월드 XZ / tileSizeXZ + offset
        float2 layerUV;
        layerUV.x = input.worldPos.x / g_LayerTiling[i].x + g_LayerTiling[i].z;
        layerUV.y = input.worldPos.z / g_LayerTiling[i].y + g_LayerTiling[i].w;

        // Diffuse
        float3 diff = gLayerDiffuse[i].Sample(gSampler, layerUV).rgb;
        albedo += diff * weights[i];

        // Normal map (tangent space XY → world space 근사 편향)
        float3 nm = gLayerNormal[i].Sample(gSampler, layerUV).rgb * 2.0f - 1.0f;
        blendNormal += nm * weights[i];
    }

    // 노멀맵 블렌딩: 지오메트리 노멀에 적용
    blendNormal = normalize(blendNormal);
    float3 tangent   = normalize(float3(1, 0, 0) - N * dot(float3(1,0,0), N));
    float3 bitangent = cross(N, tangent);
    float3 shadingN  = normalize(N
                        + tangent   * blendNormal.x * 0.3f
                        + bitangent * blendNormal.y * 0.3f);

    // ── 라이팅 ──
    // Directional light
    float3 lightDir = normalize(-g_LightDirection);
    float  diff     = saturate(dot(shadingN, lightDir));
    float3 diffuse  = albedo * g_LightColor.rgb * diff;

    // Specular (약하게)
    float3 viewDir  = normalize(g_CameraPos - input.worldPos);
    float3 halfVec  = normalize(viewDir + lightDir);
    float  spec     = pow(saturate(dot(shadingN, halfVec)), 32.0f) * 0.15f;

    // Ambient
    float3 ambient  = albedo * g_AmbientLight.rgb;

    float3 finalColor = diffuse + float3(spec, spec, spec) + ambient;

    return float4(finalColor, 1.0f);
}
