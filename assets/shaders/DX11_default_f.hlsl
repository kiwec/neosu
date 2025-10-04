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
	float4 col	: COLOR0;
	float4 misc	: COLOR1;
	float2 tex	: TEXCOORD0;
};

struct PS_OUTPUT
{
	float4 col	: SV_Target;
};

PS_OUTPUT psmain(in PS_INPUT In)
{
	PS_OUTPUT Out;
	if (In.misc.x < 0.5f)
	{
		Out.col = In.col;
	}
	else
	{
		Out.col = tex2D.Sample(samplerState, In.tex) * In.col;
	}

	// color inversion
	if (In.misc.y > 0.5f)
	{
		Out.col.rgb = 1.0f - Out.col.rgb;
	}

	return Out;
}
