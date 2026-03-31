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
	out float4 oPos : POSITION,
    uniform FrameInfo IN,
    uniform FrameInfo ORIG,
	uniform float4x4 modelViewProj
) {
    oPos = mul(modelViewProj, float4(pos.xy, 0, 1));
    oT0 = uv1 * ORIG.video_size / ORIG.texture_size;
    oT1 = ((uv1 - 1) * IN.output_size + 130) / 128.0f;
}

float4 main_fragment(
    float2 baseUV : TEXCOORD0,
    float2 overlayUV : TEXCOORD1,
    uniform FrameInfo IN,
    uniform FrameInfo ORIG,
    uniform sampler2D IN_texture,
    uniform sampler2D ORIG_texture
) : SV_Target {
    float3 px = tex2D(ORIG_texture, baseUV).rgb;

    if (all(overlayUV >= 0) && all(overlayUV < 1)) {
        px = tex2D(IN_texture, overlayUV * (IN.video_size / IN.texture_size));
    }

    return float4(px, 0);
}
