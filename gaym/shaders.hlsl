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
};

Texture2D gAlbedoMap    : register(t0);
Texture2D gShadowMap    : register(t1);
Texture2D gNormalMap    : register(t2);  // Water normal map
Texture2D gHeightMap    : register(t3);  // Water height map
Texture2D gEmissiveMap  : register(t4);  // Emissive map
Texture2D gAOMap        : register(t5);  // Ambient Occlusion map (stylized water)
Texture2D gRoughnessMap : register(t6);  // Roughness map (stylized water)
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
};

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

    // Water vertex displacement using height map
    if (bIsWater)
    {
        // UV 설정 (PS와 동일)
        float uvScale = 3.0f;
        float2 tiledUV = input.uv * uvScale;

        // 느린 흐름 (PS와 동일)
        float2 flow = float2(g_Time * 0.015f, g_Time * 0.01f);

        // Height map 샘플링
        float2 heightUV1 = tiledUV + flow;
        float2 heightUV2 = tiledUV * 1.3f - flow * 0.7f;

        float height1 = gHeightMap.SampleLevel(gSampler, heightUV1, 0).r;
        float height2 = gHeightMap.SampleLevel(gSampler, heightUV2, 0).r;

        // 부드러운 파도
        float combinedHeight = (height1 + height2) * 0.5f - 0.5f;

        // 완만한 변위 (원신 스타일은 과격하지 않음)
        float displacementScale = 2.5f;
        posL.y += combinedHeight * displacementScale;
    }

    // Transform the position from object space to clip space
    float4 worldPos = mul(float4(posL, 1.0f), World);
    output.position = mul(worldPos, ViewProj);

    // Transform the normal from object space to world space
    // mul(n, World3x3) == inverse-transpose for diagonal scale matrices (correct for mirrored objects too)
    output.worldNormal = mul(normalL, (float3x3)World);

    // Pass world position for point light calculation
    output.worldPosition = worldPos.xyz;

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
        // === 원신 스타일 물 셰이더 (깔끔하고 단순하게) ===

        // UV 설정
        float uvScale = 3.0f;
        float2 tiledUV = input.uv * uvScale;

        // 느린 단일 방향 흐름
        float2 flow = float2(g_Time * 0.015f, g_Time * 0.01f);

        // === 노말맵 (부드럽게) ===
        float2 normalUV1 = tiledUV + flow;
        float2 normalUV2 = tiledUV * 1.3f - flow * 0.7f;

        float3 normal1 = gNormalMap.Sample(gSampler, normalUV1).rgb * 2.0f - 1.0f;
        float3 normal2 = gNormalMap.Sample(gSampler, normalUV2).rgb * 2.0f - 1.0f;

        float3 blendedNormal = normalize(normal1 + normal2 * 0.5f);

        // 노말 강도 약하게 (부드러운 물결)
        float normalStrength = 0.25f;
        float3 tangent = float3(1, 0, 0);
        float3 bitangent = float3(0, 0, 1);
        waterNormal = normalize(
            tangent * blendedNormal.x * normalStrength +
            normal +
            bitangent * blendedNormal.y * normalStrength
        );

        // === Fresnel (가장자리 밝게) ===
        float NdotV = saturate(dot(waterNormal, vToCamera));
        waterFresnel = pow(1.0f - NdotV, 3.0f);

        // === 절차적 Caustics (물 아래 빛 패턴) ===
        float2 causticsUV1 = tiledUV * 2.0f + flow * 2.0f;
        float2 causticsUV2 = tiledUV * 2.3f - flow * 1.5f;
        float caustics1 = sin(causticsUV1.x * 6.28f) * sin(causticsUV1.y * 6.28f);
        float caustics2 = sin(causticsUV2.x * 6.28f + 1.0f) * sin(causticsUV2.y * 6.28f + 1.0f);
        float caustics = (caustics1 + caustics2) * 0.5f;
        caustics = smoothstep(0.3f, 0.8f, caustics * 0.5f + 0.5f) * 0.15f;

        // Caustics를 fresnel에 추가 (밝은 부분)
        waterFresnel += caustics;

        // UV는 사용 안 함 (색상은 절차적으로)
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
        // === 원신 스타일 깔끔한 물 색상 ===

        // 색상 정의 (청록 ↔ 파랑)
        float3 shallowColor = float3(0.2f, 0.65f, 0.7f);   // 청록색 (얕은 곳)
        float3 deepColor = float3(0.05f, 0.2f, 0.4f);      // 진한 파랑 (깊은 곳)
        float3 highlightColor = float3(0.85f, 0.95f, 1.0f); // 하이라이트 (거의 흰색)

        // Fresnel 기반 색상 블렌딩
        float3 waterColor = lerp(deepColor, shallowColor, waterFresnel);

        // 조명 영향 (약하게)
        float lighting = saturate(dot(shadingNormal, -g_LightDirection)) * 0.3f + 0.7f;
        waterColor *= lighting;

        // === SSS 역광 효과 (Subsurface Scattering 근사) ===
        // 카메라가 태양 반대편에 있을 때 물이 밝게 빛남
        float3 backLightDir = g_LightDirection + shadingNormal * 0.3f;
        float sss = saturate(dot(vToCamera, backLightDir));
        sss = pow(sss, 3.0f) * 0.6f;
        float3 sssColor = float3(0.3f, 0.8f, 0.7f);  // 청록빛 투과광
        waterColor += sss * sssColor * shadowFactor;

        // === 큰 태양 스펙큘러 (원신 스타일 - 크고 강하게) ===
        float3 halfVec = normalize(vToCamera + (-g_LightDirection));
        float spec = pow(max(dot(shadingNormal, halfVec), 0.0f), 32.0f);  // 넓게
        float specSharp = pow(max(dot(shadingNormal, halfVec), 0.0f), 256.0f);  // 중심은 날카롭게
        spec = spec * 0.5f + specSharp * 2.0f;  // 합성

        // === 가장자리 밝게 (Fresnel 림 라이트) ===
        float rim = waterFresnel * 0.4f;

        // === 파도 거품 (밝은 부분 강조) ===
        float waveTop = smoothstep(0.6f, 0.9f, waterFresnel);
        float foam = waveTop * 0.3f;

        // === 최종 합성 ===
        finalColor.rgb = waterColor;
        finalColor.rgb = lerp(finalColor.rgb, highlightColor, rim);
        finalColor.rgb += spec * highlightColor * 1.5f * shadowFactor;  // 스펙큘러 강화
        finalColor.rgb += foam * highlightColor;  // 거품

        // 투명도: 깊은 곳은 더 불투명, 가장자리는 살짝 투명
        float waterAlpha = lerp(0.85f, 0.65f, waterFresnel);
        return float4(finalColor.rgb, waterAlpha);
    }

    return float4(finalColor.rgb, 1.0f);
}