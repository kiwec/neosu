###DirectX11Interface::PixelShader##############################################################################################

Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

struct PS_INPUT
{
	float4 pos	: SV_Position;
	float4 col	: COLOR0;
	float2 tex	: TEXCOORD0;
	float2 rect_min : TEXCOORD1;
	float2 rect_max : TEXCOORD2;
	float edge_softness : PSIZE0;
};

struct PS_OUTPUT
{
	float4 col	: SV_Target;
};

PS_OUTPUT psmain(in PS_INPUT In)
{
	PS_OUTPUT Out;

	float2 dist_to_edges = min(In.pos.xy - In.rect_min, In.rect_max - In.pos.xy);
	float min_dist = min(dist_to_edges.x, dist_to_edges.y);

	float alpha = smoothstep(-In.edge_softness, 0.0f, min_dist);
	float4 texColor = txDiffuse.Sample(samLinear, In.tex);

	Out.col = float4(texColor.rgb * In.col.rgb, texColor.a * In.col.a * alpha);

	return Out;
};
