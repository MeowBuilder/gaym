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
    float2 pad;
    MATERIAL gMaterial; // Replaced BaseColor with full Material
    matrix gBoneTransforms[96];
};

// Per-Pass Constant Buffer
cbuffer cbPass : register(b1)
{
    matrix ViewProj;
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
};

Texture2D gAlbedoMap : register(t0);
SamplerState gSampler : register(s0);

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

    // Transform the position from object space to clip space
    float4 worldPos = mul(float4(posL, 1.0f), World);
    output.position = mul(worldPos, ViewProj);

    // Transform the normal from object space to world space
    output.worldNormal = mul(normalL, (float3x3)World);

    // Pass world position for point light calculation
    output.worldPosition = worldPos.xyz;
    
    output.uv = input.uv;

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    // Normalize the world normal
    float3 normal = normalize(input.worldNormal);
    float3 vToCamera = normalize(g_CameraPosition - input.worldPosition); // Vector from fragment to camera

    float4 albedoColor = gAlbedoMap.Sample(gSampler, input.uv);
    // Combine with material diffuse (optional: multiply)
    float4 baseColor = albedoColor * gMaterial.m_cDiffuse;

    // --- Directional Light Calculation ---
    float directionalDiffuseFactor = saturate(dot(normal, -g_LightDirection));
    float3 vHalfDirectional = normalize(vToCamera + (-g_LightDirection)); // Half vector for directional specular
    float directionalSpecularFactor = pow(max(dot(vHalfDirectional, normal), 0.0f), gMaterial.m_cSpecular.a);

    float4 directionalDiffuse = directionalDiffuseFactor * g_LightColor * baseColor;
    float4 directionalSpecular = directionalSpecularFactor * g_LightColor * gMaterial.m_cSpecular;
    float4 directionalTotal = directionalDiffuse + directionalSpecular;

    // --- Point Light Calculation ---
    float3 lightVec = g_PointLightPosition - input.worldPosition;
    float dist = length(lightVec);
    float3 pointLightDir = normalize(lightVec);

    float attenuation = saturate(1.0f - dist / g_PointLightRange);
    
    float pointDiffuseFactor = saturate(dot(normal, pointLightDir));
    float3 vHalfPoint = normalize(vToCamera + pointLightDir); // Half vector for point specular
    float pointSpecularFactor = pow(max(dot(vHalfPoint, normal), 0.0f), gMaterial.m_cSpecular.a);

    float4 pointDiffuse = pointDiffuseFactor * g_PointLightColor * baseColor * attenuation;
    float4 pointSpecular = pointSpecularFactor * g_PointLightColor * gMaterial.m_cSpecular * attenuation;
    float4 pointTotal = pointDiffuse + pointSpecular;
    
    // --- Ambient Light Calculation ---
    float4 ambient = g_AmbientLight * gMaterial.m_cAmbient * albedoColor; // Apply texture to ambient too
    
    // Final color is the sum of all light components + emissive
    float4 finalColor = directionalTotal + pointTotal + ambient + gMaterial.m_cEmissive;

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
        float spotDiffuseFactor = saturate(dot(normal, spotLightDir));
        float3 vHalfSpot = normalize(vToCamera + spotLightDir);
        float spotSpecularFactor = pow(max(dot(vHalfSpot, normal), 0.0f), gMaterial.m_cSpecular.a);

        float4 spotDiffuse = spotDiffuseFactor * g_SpotLightColor * baseColor * spotAttenuation;
        float4 spotSpecular = spotSpecularFactor * g_SpotLightColor * gMaterial.m_cSpecular * spotAttenuation;
        float4 spotTotal = spotDiffuse + spotSpecular;
        finalColor += spotTotal;
    }

    return finalColor;
}