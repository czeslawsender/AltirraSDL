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
	out float2 clockParams : TEXCOORD2,
	out float4 oPos : SV_Position,
    uniform FrameInfo IN,
	uniform bool enableClock = true
) {
	oPos = float4(pos.xy, 0, 1);
	oT0 = uv0;
	oT1 = uv1 - 0.5;
	oT1 *= IN.output_size;
	oT1.y = -oT1.y;
	
	// invert and invert back every 500 frames (1000 frames total)
    float phase = frac(IN.frame_count / 1000);
    float inv = phase >= 0.5 ? -1 : 1;
    phase = frac(phase * 2);
    
    // rotate the clock around once every 500 frames and encode to a threshold
    // in [-2, 2]
    float threshold = (cos(phase * (3.1415926535f * 2.0f)) + 1) * (phase > 0.5 ? -1 : 1);
    
    if (!enableClock)
	threshold = 0;

    // encode rendering parameters
    clockParams.x = threshold;
    clockParams.y = inv;
}

float4 main_fragment(
    float2 uv0 : TEXCOORD0,
    float2 vec : TEXCOORD1,
    float2 clockParams : TEXCOORD2,
    sampler IN_texture,
//    Texture2D IN_texture,
    uniform FrameInfo IN
) : SV_Target {
	bool enableClock = true;
    float threshold = clockParams.x;
    float inv = clockParams.y;

    // compute angular position of current pixel in [-2, 2]
    float2 scale = 1.0f / sqrt(max(1e-10f, dot(vec, vec)));    
    float angle = (vec.y * scale + 1) * (vec.x < 0 ? -1 : 1);

    // sample source texture
    float4 px = 0;

    if (enableClock) {    
    px.rgb += tex2D(IN_texture, uv0).rgb;
//	px.rgb += IN_texture.Sample(IN_texture, uv0).rgb;
    }
    px.a = 1;

    // modify intensity based on whether pixel is before or after clock threshold
    px.rgb *= ((angle < threshold ? 0.25f : -0.25f) * inv + 0.75f);

    return px;
}
