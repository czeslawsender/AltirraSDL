void main_vertex(
	float2 pos : POSITION,
	float2 uv0 : TEXCOORD0,
	out float2 oT0 : TEXCOORD0,
	out float4 oPos : POSITION
) {
	oPos = float4(pos.xy, 0, 1);
	oT0 = uv0;
}

float4 main_fragment(float2 uv0 : TEXCOORD0, sampler src : register(s0)) : COLOR0 {
	float3 c = tex2D(src, uv0).rgb;
	float y = dot(c, float3(0.30, 0.59, 0.11));

	c = y + (c - y) * 0.5f;

	return float4(c, 1);
}
