###DirectX11Interface::PixelShader##############################################################################################

struct PS_INPUT
{
	float4 pos	: SV_Position;
	float max_opacity : PSIZE0;
	float flashlight_radius : PSIZE1;
	float2 flashlight_center : PSIZE2;
};

struct PS_OUTPUT
{
	float4 col	: SV_Target;
};

PS_OUTPUT psmain(in PS_INPUT In)
{
	PS_OUTPUT Out;

	float dist = distance(In.flashlight_center, In.pos.xy);
	float opacity = 1.0f - smoothstep(In.flashlight_radius, In.flashlight_radius * 1.4f, dist);
	opacity = 1.0f - min(opacity, In.max_opacity);

	Out.col = float4(0.0f, 0.0f, 0.0f, opacity);

	return Out;
};
