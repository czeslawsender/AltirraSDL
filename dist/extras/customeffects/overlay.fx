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
	out float2 oT0 : TEXCOORD0,
	out float2 oT1 : TEXCOORD1,
	out float4 oPos : SV_Position
) {
    oPos = float4(pos.xy, 0, 1);
    oT0 = uv0;
    oT1 = uv1;
}

sampler image;

float4 main_fragment(
    float2 uv0 : TEXCOORD0,
    float2 uv1 : TEXCOORD1,
    sampler IN_texture,
    uniform FrameInfo IN
) : SV_Target {
    float3 px = tex2D(IN_texture, uv0).rgb;

    float2 logoUV = ((uv1 - 1) * IN.output_size + 130) / 128.0f;

    if (all(logoUV >= 0) && all(logoUV < 1)) {
        float3 imgc = tex2D(image, logoUV);
        px = lerp(px, float3(1,1,1), imgc);
    }

    return float4(px, 0);
}
