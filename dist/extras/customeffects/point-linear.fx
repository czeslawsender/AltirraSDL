struct FrameInfo {
    float2 video_size;
    float2 texture_size;
    float2 output_size;
    float frame_count;
    float frame_direction;
};

void main_vertex(
	float2 pos : POSITION,
	float2 uv0 : TEXCOORD0,
	float2 uv1 : TEXCOORD1,
	uniform float4x4 modelViewProj,
	out float2 oT0 : TEXCOORD0,
	out float2 oT1 : TEXCOORD1,
	out float4 oPos : SV_Position
) {
    oPos = mul(modelViewProj, float4(pos.xy, 0, 1));
    oT0 = uv0;
    oT1 = uv1;
}

float4 main_fragment(
    float2 uv0 : TEXCOORD0,
    float2 uv1 : TEXCOORD1,
    sampler IN_texture,
    sampler PREV_texture,
    uniform FrameInfo IN
) : SV_Target {
    return tex2D(IN_texture, uv0);
}
