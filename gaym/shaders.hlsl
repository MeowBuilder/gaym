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
    MATERIAL gMaterial; // Replaced BaseColor with full Material
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

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 worldNormal : NORMAL;
    float3 worldPosition : POSITION; // Added for point light calculation
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    // Transform the position from object space to clip space
    float4 worldPos = mul(float4(input.position, 1.0f), World);
    output.position = mul(worldPos, ViewProj);

    // Transform the normal from object space to world space
    output.worldNormal = mul(input.normal, (float3x3)World);

    // Pass world position for point light calculation
    output.worldPosition = worldPos.xyz;

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    // Normalize the world normal
    float3 normal = normalize(input.worldNormal);
    float3 vToCamera = normalize(g_CameraPosition - input.worldPosition); // Vector from fragment to camera

    // --- Directional Light Calculation ---
    float directionalDiffuseFactor = saturate(dot(normal, -g_LightDirection));
    float3 vHalfDirectional = normalize(vToCamera + (-g_LightDirection)); // Half vector for directional specular
    float directionalSpecularFactor = pow(max(dot(vHalfDirectional, normal), 0.0f), gMaterial.m_cSpecular.a);

    float4 directionalDiffuse = directionalDiffuseFactor * g_LightColor * gMaterial.m_cDiffuse;
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

    float4 pointDiffuse = pointDiffuseFactor * g_PointLightColor * gMaterial.m_cDiffuse * attenuation;
    float4 pointSpecular = pointSpecularFactor * g_PointLightColor * gMaterial.m_cSpecular * attenuation;
    float4 pointTotal = pointDiffuse + pointSpecular;
    
    // --- Ambient Light Calculation ---
    float4 ambient = g_AmbientLight * gMaterial.m_cAmbient;
    
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

        float4 spotDiffuse = spotDiffuseFactor * g_SpotLightColor * gMaterial.m_cDiffuse * spotAttenuation;
        float4 spotSpecular = spotSpecularFactor * g_SpotLightColor * gMaterial.m_cSpecular * spotAttenuation;
        float4 spotTotal = spotDiffuse + spotSpecular;
        finalColor += spotTotal;
    }

    return finalColor;
}