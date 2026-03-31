void main_vertex(
	float2 pos : POSITION,
	float2 uv0 : TEXCOORD0,
	float2 uv1 : TEXCOORD1,
	out float2 oT0 : TEXCOORD0,
	out float4 oPos : POSITION,
	uniform float4x4 modelViewProj
) {
	oPos = mul(modelViewProj, float4(pos.xy, 0, 1));
	oT0 = (uv1 - 0.5) * 20;
}

float4 main_fragment(float2 vec : TEXCOORD0) : COLOR0 {
    float v = length(vec);
    
    return sin(v*v) * 0.5f + 0.5f;
}
