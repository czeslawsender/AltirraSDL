extern sampler src : register(s0);

void main_vertex(
    float4 pos : POSITION,
    float2 uv : TEXCOORD0,
    uniform float4x4 modelViewProj,
    out float2 oT0 : TEXCOORD0,
    out float4 oPos : POSITION)
{
    oPos = mul(modelViewProj, pos);
    oT0 = uv;
}

half4 main_fragment(float2 uv : TEXCOORD0) : COLOR0 {
    half4 c = tex2D(src, uv);
    c.rgb = 1.0h - c.rgb;

    return c;
}