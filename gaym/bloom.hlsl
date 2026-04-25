// LDR bloom: snapshot back buffer -> bright pass -> H/V gaussian blur ->
// additive composite back onto the back buffer.
//
// All PS take a fullscreen-triangle VS output. Single SRV at t0.

cbuffer BloomCB : register(b0)
{
    float2 texelSize;   // 1 / source texture size
    float  threshold;   // luminance threshold for bright pass
    float  intensity;   // bloom add-strength in composite
    float  exposure;    // unused in LDR path, kept for CB alignment
    float  _pad0;
    float  _pad1;
    float  _pad2;
};

Texture2D    g_tex0    : register(t0);
SamplerState g_sampler : register(s0);

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VS_OUT VS_Fullscreen(uint id : SV_VertexID)
{
    VS_OUT o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.uv  = uv;
    o.pos = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0f, 1.0f);
    return o;
}

float4 PS_BrightPass(VS_OUT i) : SV_TARGET
{
    float3 c = g_tex0.Sample(g_sampler, i.uv).rgb;
    float  maxC = max(c.r, max(c.g, c.b));
    float  minC = min(c.r, min(c.g, c.b));
    // Chromaticity: 0 for grayscale (lit character diffuse + specular often
    // clips near-white here), ~1 for pure saturated skill-particle colors.
    float  chroma = (maxC > 1e-5f) ? ((maxC - minC) / maxC) : 0.0f;

    // Brightness gate: only bright pixels qualify.
    float  soft = saturate((maxC - threshold) / max(1.0f - threshold, 1e-4f));
    soft = soft * soft;

    // Chroma gate: unsaturated bright stuff (character spec highlights, lit
    // skin/armor, bright stone) is fully killed. Saturated colors
    // (skill particles, fire/ice FX, water caustics) pass through cleanly.
    //  chroma <= 0.22  -> gate 0       (no bloom on near-white highlights / 약채도 라이팅)
    //  chroma 0.55+    -> gate 1.0     (full bloom on saturated FX)
    float  chromaGate = smoothstep(0.22f, 0.55f, chroma);
    soft *= chromaGate;

    return float4(c * soft, 1.0f);
}

static const float g_w[5] = { 0.2270270270f, 0.1945945946f, 0.1216216216f, 0.0540540541f, 0.0162162162f };

float4 BlurDir(float2 uv, float2 dir)
{
    float3 acc = g_tex0.Sample(g_sampler, uv).rgb * g_w[0];
    [unroll]
    for (int k = 1; k < 5; ++k)
    {
        float2 off = dir * (float)k;
        acc += g_tex0.Sample(g_sampler, uv + off).rgb * g_w[k];
        acc += g_tex0.Sample(g_sampler, uv - off).rgb * g_w[k];
    }
    return float4(acc, 1.0f);
}

float4 PS_BlurH(VS_OUT i) : SV_TARGET
{
    return BlurDir(i.uv, float2(texelSize.x, 0.0f));
}

float4 PS_BlurV(VS_OUT i) : SV_TARGET
{
    return BlurDir(i.uv, float2(0.0f, texelSize.y));
}

// Additive composite: output bloom contribution only. The pipeline's blend
// state adds this on top of whatever is already in the back buffer.
float4 PS_Composite(VS_OUT i) : SV_TARGET
{
    float3 bloom = g_tex0.Sample(g_sampler, i.uv).rgb;
    return float4(bloom * intensity, 1.0f);
}
