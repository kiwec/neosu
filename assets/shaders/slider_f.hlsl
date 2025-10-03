###DirectX11Interface::PixelShader##############################################################################################

struct PS_INPUT
{
	float4 pos	: SV_Position;
	float2 tex	: TEXCOORD0;
	int style	: BLENDINDICES0;
	float bodyAlphaMultiplier	: PSIZE0;
	float bodyColorSaturation	: PSIZE1;
	float borderSizeMultiplier	: PSIZE2;
	float borderFeather			: PSIZE3;
	float4 colBorder	: COLOR1;
	float4 colBody		: COLOR2;
};

struct PS_OUTPUT
{
	float4 col	: SV_Target;
};

static float defaultTransitionSize = 0.011f;
static float defaultBorderSize = 0.11f;
static float outerShadowSize = 0.08f;

float4 getInnerBodyColor(in float4 bodyColor)
{
	float brightnessMultiplier = 0.25f;
	bodyColor.r = min(1.0f, bodyColor.r * (1.0f + 0.5f * brightnessMultiplier) + brightnessMultiplier);
	bodyColor.g = min(1.0f, bodyColor.g * (1.0f + 0.5f * brightnessMultiplier) + brightnessMultiplier);
	bodyColor.b = min(1.0f, bodyColor.b * (1.0f + 0.5f * brightnessMultiplier) + brightnessMultiplier);
	return float4(bodyColor);
}

float4 getOuterBodyColor(in float4 bodyColor)
{
	float darknessMultiplier = 0.1f;
	bodyColor.r = min(1.0f, bodyColor.r / (1.0f + darknessMultiplier));
	bodyColor.g = min(1.0f, bodyColor.g / (1.0f + darknessMultiplier));
	bodyColor.b = min(1.0f, bodyColor.b / (1.0f + darknessMultiplier));
	return float4(bodyColor);
}

PS_OUTPUT psmain(in PS_INPUT In)
{
	PS_OUTPUT Out;

	float borderSize = (defaultBorderSize + In.borderFeather) * In.borderSizeMultiplier;
	float transitionSize = defaultTransitionSize + In.borderFeather;

	// dynamic color calculations
	float4 borderColor = float4(In.colBorder.x, In.colBorder.y, In.colBorder.z, 1.0f);
	float4 bodyColor = float4(In.colBody.x, In.colBody.y, In.colBody.z, 0.7f*In.bodyAlphaMultiplier);
	float4 outerShadowColor = float4(0, 0, 0, 0.25f);
	float4 innerBodyColor = getInnerBodyColor(bodyColor);
	float4 outerBodyColor = getOuterBodyColor(bodyColor);

	innerBodyColor.rgb *= In.bodyColorSaturation;
	outerBodyColor.rgb *= In.bodyColorSaturation;

	// osu!next style color modifications
	if (In.style == 1)
	{
		outerBodyColor.rgb = bodyColor.rgb * In.bodyColorSaturation;
		outerBodyColor.a = 1.0f*In.bodyAlphaMultiplier;
		innerBodyColor.rgb = bodyColor.rgb * 0.5f * In.bodyColorSaturation;
		innerBodyColor.a = 0.0f;
	}

	// a bit of a hack, but better than rough edges
	if (In.borderSizeMultiplier < 0.01f)
		borderColor = outerShadowColor;

	// conditional variant

	if (In.tex.x < outerShadowSize - transitionSize) // just shadow
	{
		float delta = In.tex.x / (outerShadowSize - transitionSize);
		Out.col = lerp(float4(0, 0, 0, 0), outerShadowColor, delta);
	}
	if (In.tex.x > outerShadowSize - transitionSize && In.tex.x < outerShadowSize + transitionSize) // shadow + border
	{
		float delta = (In.tex.x - outerShadowSize + transitionSize) / (2.0f*transitionSize);
		Out.col = lerp(outerShadowColor, borderColor, delta);
	}
	if (In.tex.x > outerShadowSize + transitionSize && In.tex.x < outerShadowSize + borderSize - transitionSize) // just border
	{
		Out.col = borderColor;
	}
	if (In.tex.x > outerShadowSize + borderSize - transitionSize && In.tex.x < outerShadowSize + borderSize + transitionSize) // border + outer body
	{
		float delta = (In.tex.x - outerShadowSize - borderSize + transitionSize) / (2.0f*transitionSize);
		Out.col = lerp(borderColor, outerBodyColor, delta);
	}
	if (In.tex.x > outerShadowSize + borderSize + transitionSize) // outer body + inner body
	{
		float size = outerShadowSize + borderSize + transitionSize;
		float delta = ((In.tex.x - size) / (1.0f-size));
		Out.col = lerp(outerBodyColor, innerBodyColor, delta);
	}

	return Out;
};
