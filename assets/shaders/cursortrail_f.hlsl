###DirectX11Interface::PixelShader##############################################################################################

Texture2D tex2D;
SamplerState samplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct PS_INPUT
{
	float4 pos	: SV_Position;
	float2 tex	: TEXCOORD0;
	float time  : PSIZE0;
	float vtx_alpha : PSIZE1;
};

struct PS_OUTPUT
{
	float4 col	: SV_Target;
};

PS_OUTPUT psmain(in PS_INPUT In)
{
	PS_OUTPUT Out;

	Out.col = tex2D.Sample(samplerState, In.tex);
	Out.col.a *= In.vtx_alpha;
	
	/*
	float speedMultiplier = 0.8f;
	float spreadMultiplier = 2.0f;
	Out.col.r = sin(In.time*speedMultiplier + In.vtx_alpha*spreadMultiplier + 0.0f)*0.5f + 0.5f;
	Out.col.g = sin(In.time*speedMultiplier + In.vtx_alpha*spreadMultiplier + 2.0f)*0.5f + 0.5f;
	Out.col.b = sin(In.time*speedMultiplier + In.vtx_alpha*spreadMultiplier + 4.0f)*0.5f + 0.5f;
	*/

	return Out;
};
