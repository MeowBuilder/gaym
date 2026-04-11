// Material Struct
struct MATERIAL
{
    float4 m_cAmbient;
    float4 m_cDiffuse;
    float4 m_cSpecular; // a = power
    float4 m_cEmissive;
};

// Per-Object Constant Buffer
cbuffer cbGameObject : register(b0)
{
    matrix World;
    uint MaterialIndex;
    uint bIsSkinned;
    uint bHasTexture;
    uint bIsLava;
    uint bIsWater;
    uint bHasEmissiveTexture;
    uint cbPad1;
    MATERIAL gMaterial;
    matrix gBoneTransforms[128];
};

// Torch light struct
#define MAX_TORCH_LIGHTS 8

struct TorchLight
{
    float3 Position; float Range;
    float3 Color;    float Intensity;
};

// Gerstner Wave Parameters
struct WaveParams
{
    float wavelength;
    float amplitude;
    float steepness;
    float speed;
    float2 direction;
    float fadeSpeed;
    float pad;
};

// Per-Pass Constant Buffer
cbuffer cbPass : register(b1)
{
    matrix ViewProj;
    matrix LightViewProj; // Shadow Map용 Light View-Projection
    float4 g_LightColor; // Directional Light Color
    float3 g_LightDirection; float pad0; // Directional Light Direction
    float4 g_PointLightColor; // Point Light Color
    float3 g_PointLightPosition; float pad1; // Point Light Position
    float g_PointLightRange; float pad2; float pad3; float pad4; // Point Light Range and padding
    float4 g_AmbientLight; // Ambient Light Color
    float3 g_CameraPosition; float pad_cam; // Camera World Position for specular

    // SpotLight
    float4 g_SpotLightColor;
    float3 g_SpotLightPosition; float g_SpotLightRange;
    float3 g_SpotLightDirection; float g_SpotLightInnerCone;
    float g_SpotLightOuterCone; float pad5; float pad6; float pad7;

    // Time for animations
    float g_Time; float g_TimePad1; float g_TimePad2; float g_TimePad3;

    // Torch lights array
    TorchLight g_TorchLights[MAX_TORCH_LIGHTS];
    int g_nActiveTorchLights; int g_TorchPad1; int g_TorchPad2; int g_TorchPad3;

    // Gerstner Waves (5 waves for ocean simulation)
    WaveParams g_Waves[5];
};

Texture2D gAlbedoMap    : register(t0);
Texture2D gShadowMap    : register(t1);
Texture2D gNormalMap    : register(t2);  // Water normal map 1
Texture2D gHeightMap    : register(t3);  // Water height map 1
Texture2D gEmissiveMap  : register(t4);  // Emissive map
Texture2D gAOMap        : register(t5);  // Ambient Occlusion map (stylized water)
Texture2D gRoughnessMap : register(t6);  // Roughness map (stylized water)
Texture2D gNormalMap2   : register(t7);  // Water normal map 2 (Water_6)
Texture2D gHeightMap2   : register(t8);  // Water height map 2 (Water_6)
Texture2D gFoamOpacity  : register(t9);  // Foam opacity map (foam4)
Texture2D gFoamDiffuse  : register(t10); // Foam diffuse map (foam4)
SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    int4 boneIndices : BONEINDICES;
    float4 boneWeights : BONEWEIGHTS;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 worldNormal : NORMAL;
    float3 worldPosition : POSITION; // Added for point light calculation
    float2 uv : TEXCOORD;
    float4 posLightSpace : TEXCOORD1; // Shadow Map용 Light 공간 위치
    float crestFactor : TEXCOORD2; // Wave crest factor for foam (0~1)
};

// ========================================================================
// Gerstner Wave Functions
// Based on: https://developer.nvidia.com/gpugems/GPUGems/gpugems_ch01.html
// ========================================================================

// Calculate single Gerstner wave displacement
// Returns: float4(offsetX, offsetY, offsetZ, crestFactor)
float4 GerstnerWave(float3 worldPos, WaveParams wave, float time)
{
    float2 d = normalize(wave.direction);
    float f = 2.0 / wave.wavelength;  // frequency
    float phi = wave.speed * f * time;
    float theta = f * dot(d, worldPos.xz) + phi;

    // Fade amplitude over time (breathing effect)
    float fade = cos(wave.fadeSpeed * time) * 0.5 + 0.5;
    float amp = wave.amplitude * fade;

    // Gerstner wave displacement
    float3 offset;
    offset.x = (wave.steepness / wave.wavelength) * d.x * cos(theta);
    offset.z = (wave.steepness / wave.wavelength) * d.y * cos(theta);
    offset.y = amp * sin(theta);

    // Crest factor: sin value normalized to 0~1 (peaks at wave crests)
    float crest = sin(theta) * 0.5 + 0.5;
    crest = crest * saturate(wave.steepness) * fade;

    return float4(offset, crest);
}

// Calculate Gerstner wave normal
float3 GerstnerNormal(float3 worldPos, WaveParams wave, float time)
{
    float2 d = normalize(wave.direction);
    float f = 2.0 / wave.wavelength;
    float phi = wave.speed * f * time;
    float theta = f * dot(d, worldPos.xz) + phi;

    float fade = cos(wave.fadeSpeed * time) * 0.5 + 0.5;
    float amp = wave.amplitude * fade;

    float WA = f * amp;
    float C = cos(theta);
    float S = sin(theta);

    float3 normal;
    normal.x = -d.x * WA * C;
    normal.z = -d.y * WA * C;
    normal.y = 1.0 - (wave.steepness / wave.wavelength) * WA * S;

    return normal;
}

// Apply all 5 Gerstner waves and return combined offset + crest
void ApplyGerstnerWaves(inout float3 worldPos, inout float3 normal, out float crestFactor)
{
    float3 totalOffset = float3(0, 0, 0);
    float3 totalNormal = float3(0, 0, 0);
    float totalCrest = 0.0;
    float totalSteepness = 0.0;

    // Sum all 5 waves
    [unroll]
    for (int i = 0; i < 5; i++)
    {
        float4 wave = GerstnerWave(worldPos, g_Waves[i], g_Time);
        totalOffset += wave.xyz;
        totalCrest += wave.w;
        totalNormal += GerstnerNormal(worldPos + totalOffset, g_Waves[i], g_Time);
        totalSteepness += g_Waves[i].steepness;
    }

    // Apply displacement to world position
    worldPos += totalOffset;

    // Normalize combined normal
    normal = normalize(float3(-totalNormal.x, 1.0 - totalNormal.y, -totalNormal.z));

    // Normalize crest factor
    crestFactor = saturate(totalCrest / max(0.01, totalSteepness));
}

// ========================================================================

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;

    float3 posL = input.position;
    float3 normalL = input.normal;

    if (bIsSkinned)
    {
        posL = float3(0.0f, 0.0f, 0.0f);
        normalL = float3(0.0f, 0.0f, 0.0f);
        
        for(int i = 0; i < 4; ++i)
        {
            int idx = input.boneIndices[i];
            float weight = input.boneWeights[i];
            
            if (weight > 0.0f)
            {
                posL += weight * mul(float4(input.position, 1.0f), gBoneTransforms[idx]).xyz;
                normalL += weight * mul(input.normal, (float3x3)gBoneTransforms[idx]);
            }
        }
    }

    // Initialize crest factor (default 0 for non-water)
    float crestFactor = 0.0;

    // Transform to world space first (needed for Gerstner waves)
    float4 worldPos = mul(float4(posL, 1.0f), World);

    // Water vertex displacement: Gerstner waves + Heightmap (Medium 글 방식)
    if (bIsWater)
    {
        // === 1. Gerstner Waves (큰 파도, 2개만 사용) ===
        float3 waveNormal = normalL;
        ApplyGerstnerWaves(worldPos.xyz, waveNormal, crestFactor);

        // === 2. Heightmap Displacement (2개 텍스처 + 5레이어!) ===
        // Vol_36_5 (Stylized) - 3개 레이어
        float2 heightUV1 = input.uv * 5.0f + float2(g_Time * 0.03f, g_Time * 0.025f);
        float2 heightUV2 = input.uv * 8.0f - float2(g_Time * 0.022f, g_Time * 0.04f);
        float2 heightUV3 = input.uv * 12.0f + float2(g_Time * 0.015f, -g_Time * 0.018f);

        float height1 = gHeightMap.SampleLevel(gSampler, heightUV1, 0).r;
        float height2 = gHeightMap.SampleLevel(gSampler, heightUV2, 0).r;
        float height3 = gHeightMap.SampleLevel(gSampler, heightUV3, 0).r;

        // Water_6 (Realistic) - 2개 레이어 (다른 스케일 + 속도)
        float2 heightUV4 = input.uv * 3.5f + float2(g_Time * 0.02f, -g_Time * 0.03f);
        float2 heightUV5 = input.uv * 15.0f - float2(g_Time * 0.01f, g_Time * 0.025f);

        float height4 = gHeightMap2.SampleLevel(gSampler, heightUV4, 0).r;
        float height5 = gHeightMap2.SampleLevel(gSampler, heightUV5, 0).r;

        // 5레이어 블렌딩 (Vol_36_5 주도, Water_6는 디테일)
        float heightCombined = (height1 * 0.35f + height2 * 0.25f + height3 * 0.15f +
                                height4 * 0.15f + height5 * 0.1f) - 0.5f;

        // 매우 강하게 적용 (높이감 증가)
        worldPos.y += heightCombined * 5.5f;  // 더 증가!

        // Update normal with wave-modified normal
        normalL = waveNormal;
    }

    // Transform the position from world space to clip space
    output.position = mul(worldPos, ViewProj);

    // Transform the normal from object space to world space
    // mul(n, World3x3) == inverse-transpose for diagonal scale matrices (correct for mirrored objects too)
    output.worldNormal = mul(normalL, (float3x3)World);

    // Pass world position for point light calculation
    output.worldPosition = worldPos.xyz;

    // Pass crest factor for foam calculation in pixel shader
    output.crestFactor = crestFactor;

    output.uv = input.uv;

    // Calculate position in light space for shadow mapping
    output.posLightSpace = mul(worldPos, LightViewProj);

    return output;
}

// Shadow Pass Vertex Shader (depth only)
float4 VS_Shadow(VS_INPUT input) : SV_POSITION
{
    float3 posL = input.position;

    if (bIsSkinned)
    {
        posL = float3(0.0f, 0.0f, 0.0f);

        for (int i = 0; i < 4; ++i)
        {
            int idx = input.boneIndices[i];
            float weight = input.boneWeights[i];

            if (weight > 0.0f)
            {
                posL += weight * mul(float4(input.position, 1.0f), gBoneTransforms[idx]).xyz;
            }
        }
    }

    float4 worldPos = mul(float4(posL, 1.0f), World);
    return mul(worldPos, LightViewProj);
}

// PCF 3x3 Shadow Calculation
float CalculateShadow(float4 posLightSpace)
{
    // Perspective divide
    float3 projCoords = posLightSpace.xyz / posLightSpace.w;

    // Transform to [0, 1] range for texture sampling
    projCoords.x = projCoords.x * 0.5f + 0.5f;
    projCoords.y = projCoords.y * -0.5f + 0.5f;  // Y is flipped in DirectX

    // Check if outside shadow map bounds
    if (projCoords.x < 0.0f || projCoords.x > 1.0f ||
        projCoords.y < 0.0f || projCoords.y > 1.0f ||
        projCoords.z < 0.0f || projCoords.z > 1.0f)
    {
        return 1.0f;  // No shadow outside bounds
    }

    float currentDepth = projCoords.z;
    float shadow = 0.0f;

    // Depth bias: 경사 기반으로 acne 방지 (음수 스케일/큰 맵에서 더 큰 값 필요)
    float bias = 0.002f;

    // PCF 3x3 sampling
    float texelSize = 1.0f / 2048.0f;  // Shadow map size
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * texelSize;
            shadow += gShadowMap.SampleCmpLevelZero(gShadowSampler, projCoords.xy + offset, currentDepth - bias);
        }
    }
    shadow /= 9.0f;

    return shadow;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    // Normalize the world normal
    float3 normal = normalize(input.worldNormal);
    float3 vToCamera = normalize(g_CameraPosition - input.worldPosition); // Vector from fragment to camera

    // UV animation for lava or water
    float2 uv = input.uv;
    float3 waterNormal = normal;  // Default to geometry normal
    float waterFresnel = 0.0f;
    float waterAO = 1.0f;           // Default AO (no darkening)
    float waterSpecularPower = 0.0f; // 0 means use material default

    if (bIsLava)
    {
        // Very slow flowing lava effect
        float2 flow1 = float2(g_Time * 0.003f, g_Time * 0.002f);
        float2 flow2 = float2(-g_Time * 0.002f, g_Time * 0.0025f);
        uv = input.uv + flow1 + flow2 * 0.5f;
    }
    else if (bIsWater)
    {
        // === GPU Gems 스타일 물 셰이더 (Gerstner waves 기반) ===

        // Gerstner wave normal이 이미 VS에서 계산됨
        // 2개 노말맵으로 풍부한 디테일 추가
        float2 normalUV1 = input.uv * 2.0f + float2(g_Time * 0.02f, g_Time * 0.015f);
        float2 normalUV2 = input.uv * 4.5f - float2(g_Time * 0.012f, g_Time * 0.025f);

        float3 detailNormal1 = gNormalMap.Sample(gSampler, normalUV1).rgb * 2.0f - 1.0f;
        float3 detailNormal2 = gNormalMap2.Sample(gSampler, normalUV2).rgb * 2.0f - 1.0f;

        // 두 노말맵 블렌딩 (Vol_36_5 + Water_6)
        float3 detailNormal = normalize(detailNormal1 * 0.6f + detailNormal2 * 0.4f);

        // Gerstner normal + 디테일 노말 (Gerstner가 주도)
        float3 tangent = float3(1, 0, 0);
        float3 bitangent = float3(0, 0, 1);
        waterNormal = normalize(
            normal +
            tangent * detailNormal.x * 0.15f +
            bitangent * detailNormal.z * 0.15f
        );

        // 간단한 Fresnel
        float NdotV = saturate(dot(waterNormal, vToCamera));
        waterFresnel = pow(1.0f - NdotV, 5.0f);

        uv = input.uv;
    }

    float4 albedoColor;
    if (bHasTexture)
    {
        albedoColor = gAlbedoMap.Sample(gSampler, uv);

    }
    else
    {
        albedoColor = float4(1.0f, 1.0f, 1.0f, 1.0f); // White if no texture
    }
    // Combine with material diffuse (optional: multiply)
    float4 baseColor = albedoColor * gMaterial.m_cDiffuse;

    // Apply water AO to base color
    if (bIsWater)
        baseColor.rgb *= waterAO;

    // Use water normal for lighting if water, otherwise use geometry normal
    float3 shadingNormal = bIsWater ? waterNormal : normal;

    // Specular power: use roughness-based power for water, material default otherwise
    float specPower = (bIsWater && waterSpecularPower > 0.0f) ? waterSpecularPower : gMaterial.m_cSpecular.a;

    // --- Shadow Calculation ---
    float shadowFactor = CalculateShadow(input.posLightSpace);

    // --- Directional Light Calculation ---
    float directionalDiffuseFactor = saturate(dot(shadingNormal, -g_LightDirection));
    float3 vHalfDirectional = normalize(vToCamera + (-g_LightDirection)); // Half vector for directional specular
    float directionalSpecularFactor = pow(max(dot(vHalfDirectional, shadingNormal), 0.0f), specPower);

    float4 directionalDiffuse = directionalDiffuseFactor * g_LightColor * baseColor;
    float4 directionalSpecular = directionalSpecularFactor * g_LightColor * gMaterial.m_cSpecular;
    float4 directionalTotal = (directionalDiffuse + directionalSpecular) * shadowFactor;  // Apply shadow

    // --- Point Light Calculation ---
    float3 lightVec = g_PointLightPosition - input.worldPosition;
    float dist = length(lightVec);
    float3 pointLightDir = normalize(lightVec);

    float attenuation = saturate(1.0f - dist / g_PointLightRange);
    
    float pointDiffuseFactor = saturate(dot(shadingNormal, pointLightDir));
    float3 vHalfPoint = normalize(vToCamera + pointLightDir); // Half vector for point specular
    float pointSpecularFactor = pow(max(dot(vHalfPoint, shadingNormal), 0.0f), specPower);

    float4 pointDiffuse = pointDiffuseFactor * g_PointLightColor * baseColor * attenuation;
    float4 pointSpecular = pointSpecularFactor * g_PointLightColor * gMaterial.m_cSpecular * attenuation;
    float4 pointTotal = pointDiffuse + pointSpecular;
    
    // --- Ambient Light Calculation ---
    float4 ambient = g_AmbientLight * gMaterial.m_cAmbient * albedoColor; // Apply texture to ambient too
    
    // Final color is the sum of all light components + emissive
    // Emission Map이 있으면 _EmissionColor * EmissionMap, 없으면 _EmissionColor 그대로
    float4 emissiveContrib;
    if (bHasEmissiveTexture)
        emissiveContrib = float4(gMaterial.m_cEmissive.rgb * gEmissiveMap.Sample(gSampler, uv).rgb, 0.0f);
    else
        emissiveContrib = float4(gMaterial.m_cEmissive.rgb, 0.0f);
    float4 finalColor = directionalTotal + pointTotal + ambient + emissiveContrib;

    // --- Spot Light Calculation ---
    float3 spotLightVec = g_SpotLightPosition - input.worldPosition;
    float spotDist = length(spotLightVec);
    float3 spotLightDir = normalize(spotLightVec);

    // Distance attenuation
    float spotAttenuation = saturate(1.0f - spotDist / g_SpotLightRange);

    // Cone attenuation
    float cosTheta = dot(-spotLightDir, normalize(g_SpotLightDirection));
    float coneAttenuation = saturate((cosTheta - g_SpotLightOuterCone) / (g_SpotLightInnerCone - g_SpotLightOuterCone));

    spotAttenuation *= coneAttenuation;

    if (spotAttenuation > 0.0f)
    {
        float spotDiffuseFactor = saturate(dot(shadingNormal, spotLightDir));
        float3 vHalfSpot = normalize(vToCamera + spotLightDir);
        float spotSpecularFactor = pow(max(dot(vHalfSpot, shadingNormal), 0.0f), specPower);

        float4 spotDiffuse = spotDiffuseFactor * g_SpotLightColor * baseColor * spotAttenuation;
        float4 spotSpecular = spotSpecularFactor * g_SpotLightColor * gMaterial.m_cSpecular * spotAttenuation;
        float4 spotTotal = spotDiffuse + spotSpecular;
        finalColor += spotTotal;
    }

    // --- Torch Lights Calculation (multiple point lights) ---
    [loop]
    for (int t = 0; t < g_nActiveTorchLights && t < MAX_TORCH_LIGHTS; ++t)
    {
        float3 torchVec = g_TorchLights[t].Position - input.worldPosition;
        float torchDist = length(torchVec);

        if (torchDist < g_TorchLights[t].Range)
        {
            float3 torchDir = torchVec / torchDist;

            // Smooth quadratic attenuation for torch light
            float normalizedDist = torchDist / g_TorchLights[t].Range;
            float torchAtten = saturate(1.0f - normalizedDist * normalizedDist) * g_TorchLights[t].Intensity;

            // Diffuse
            float torchDiffuseFactor = saturate(dot(shadingNormal, torchDir));

            // Specular
            float3 vHalfTorch = normalize(vToCamera + torchDir);
            float torchSpecularFactor = pow(max(dot(vHalfTorch, shadingNormal), 0.0f), specPower);

            float4 torchColor = float4(g_TorchLights[t].Color, 1.0f);
            float4 torchDiffuse = torchDiffuseFactor * torchColor * baseColor * torchAtten;
            float4 torchSpecular = torchSpecularFactor * torchColor * gMaterial.m_cSpecular * torchAtten;

            finalColor += torchDiffuse + torchSpecular;
        }
    }

    if (bIsWater)
    {
        // === 균형잡힌 물 셰이더 (어둡고 거품 명확) ===

        // 기본 물 색상 (매우 어두운 바다)
        float3 waterColor = float3(0.01f, 0.08f, 0.18f);  // 진한 남색

        // 조명 계산 (약하게)
        float diff = saturate(dot(shadingNormal, -g_LightDirection));
        waterColor = waterColor * (1.0f + diff * 0.4f);  // 은은한 조명

        // Specular (태양 반사 - 적당히)
        float3 halfVec = normalize(vToCamera + (-g_LightDirection));
        float spec = pow(max(dot(shadingNormal, halfVec), 0.0f), 120.0f);  // 매우 날카롭게
        float3 specColor = float3(1.0f, 1.0f, 1.0f) * spec * 0.6f;  // 중간 강도

        // === Wave Crest Foam (foam4 텍스처로 강화!) ===
        float crestFoam = pow(input.crestFactor, 1.0f);  // 제곱 없음 (더 넓게)
        crestFoam = smoothstep(0.35f, 0.85f, crestFoam);

        // foam4 텍스처 샘플링 (거품이 있는 곳에만)
        float2 foamUV = input.uv * 6.0f + float2(g_Time * 0.015f, g_Time * 0.01f);
        float foamOpacity = gFoamOpacity.Sample(gSampler, foamUV).r;
        float3 foamDiffuse = gFoamDiffuse.Sample(gSampler, foamUV).rgb;

        // 거품 강도: crest factor + foam opacity
        float foamStrength = crestFoam * foamOpacity;
        foamStrength = saturate(foamStrength * 1.5f);  // 약간 증폭

        // 거품 색상 (foam4 diffuse + 기본 흰색 블렌딩)
        float3 foamColor = lerp(float3(0.9f, 0.95f, 1.0f), foamDiffuse, 0.3f);  // 약간 foam4 색상 반영

        // === 최종 합성 ===
        finalColor.rgb = waterColor;
        finalColor.rgb += specColor * shadowFactor;

        // 거품 명확하게 (foam4 텍스처 반영, 어두운 물 + 밝은 거품 = 대비)
        finalColor.rgb = lerp(finalColor.rgb, foamColor, foamStrength);

        // Fresnel 최소화 (거의 없음)
        float3 fresnelColor = float3(0.05f, 0.15f, 0.25f);
        finalColor.rgb = lerp(finalColor.rgb, fresnelColor, waterFresnel * 0.2f);

        // 투명도
        float waterAlpha = 0.9f;
        return float4(finalColor.rgb, waterAlpha);
    }

    return float4(finalColor.rgb, 1.0f);
}