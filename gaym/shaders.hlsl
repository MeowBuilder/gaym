cbuffer cbMatrix : register(b0)
{
    matrix matWVP;
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
    output.position = mul(float4(input.position, 1.0f), matWVP);
    output.color = input.color;
    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    return input.color;
}
