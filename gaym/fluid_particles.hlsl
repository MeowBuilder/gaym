// Fluid Particle Shader -- 빌보드 쿼드, 소프트 글로우 원
// 참고: FluidParticleSystem.cpp에서 인라인 컴파일을 사용합니다.
//       이 파일은 참조/디버그 용도입니다.

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

// TRIANGLE_STRIP 순서: 쿼드당 4개 정점
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
    // UV를 [-1, 1]로 변환
    float2 centered = input.uv * 2.0f - 1.0f;
    float  d        = length(centered);

    // 소프트 디스크 알파
    float alpha = 1.0f - smoothstep(0.5f, 1.0f, d);
    clip(alpha - 0.02f);

    // 내부 광택 부스트
    float glow = 1.0f - smoothstep(0.0f, 0.4f, d);
    float3 col = input.color.rgb * (1.0f + glow * 0.8f);

    return float4(col, input.color.a * alpha);
}
