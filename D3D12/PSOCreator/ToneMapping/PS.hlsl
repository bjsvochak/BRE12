#include "../ShaderUtils/Utils.hlsli"

struct Input {
	float4 mPosH : SV_POSITION;
	float2 mTexCoordO : TEXCOORD;
};

SamplerState TexSampler : register (s0);
Texture2D ColorBufferTexture : register(t0);

struct Output {
	float4 mColor : SV_Target0;
};

Output main(const in Input input){
	Output output = (Output)0;

	const float4 color = ColorBufferTexture.Sample(TexSampler, input.mTexCoordO);
	output.mColor = color;//float4(HableToneMap(color.rgb), color.a);
	
	return output;
}