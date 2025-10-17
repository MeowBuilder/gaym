// Per-Object Constant Buffer
cbuffer cbGameObject : register(b0)
{
    matrix World;
    uint MaterialIndex;
};

// Per-Pass Constant Buffer
cbuffer cbPass : register(b1)
{
    matrix ViewProj;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    // Transform the position from object space to clip space
    output.position = mul(float4(input.position, 1.0f), World);
    output.position = mul(output.position, ViewProj);

    output.color = input.color;
    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    return input.color;
}