Texture2D<float4> tex0 : register(t0, space2);
SamplerState samp0 : register(s0, space2);

cbuffer FragParams : register(b0, space3) {
    int style;
    float bodyColorSaturation;
    float bodyAlphaMultiplier;
    float borderSizeMultiplier;
    float borderFeather;
    float _pad0; float _pad1; float _pad2;
    float3 colBorder;
    float _pad3;
    float3 colBody;
    float _pad4;
};

static const float defaultTransitionSize = 0.011;
static const float defaultBorderSize = 0.11;
static const float outerShadowSize = 0.08;

float4 getInnerBodyColor(float4 bodyColor) {
    float brightnessMultiplier = 0.25;
    bodyColor.r = min(1.0, bodyColor.r * (1.0 + 0.5 * brightnessMultiplier) + brightnessMultiplier);
    bodyColor.g = min(1.0, bodyColor.g * (1.0 + 0.5 * brightnessMultiplier) + brightnessMultiplier);
    bodyColor.b = min(1.0, bodyColor.b * (1.0 + 0.5 * brightnessMultiplier) + brightnessMultiplier);
    return float4(bodyColor);
}

float4 getOuterBodyColor(float4 bodyColor) {
    float darknessMultiplier = 0.1;
    bodyColor.r = min(1.0, bodyColor.r / (1.0 + darknessMultiplier));
    bodyColor.g = min(1.0, bodyColor.g / (1.0 + darknessMultiplier));
    bodyColor.b = min(1.0, bodyColor.b / (1.0 + darknessMultiplier));
    return float4(bodyColor);
}

struct PSInput {
    float4 position : SV_Position;
    float2 tex_coord : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0 {
    float borderSize = (defaultBorderSize + borderFeather) * borderSizeMultiplier;
    float transitionSize = defaultTransitionSize + borderFeather;

    // output var
    float4 out_color = float4(0.0, 0.0, 0.0, 0.0);

    // dynamic color calculations
    float4 borderColor = float4(colBorder.x, colBorder.y, colBorder.z, 1.0);
    float4 bodyColor = float4(colBody.x, colBody.y, colBody.z, 0.7 * bodyAlphaMultiplier);
    float4 outerShadowColor = float4(0, 0, 0, 0.25);
    float4 innerBodyColor = getInnerBodyColor(bodyColor);
    float4 outerBodyColor = getOuterBodyColor(bodyColor);

    innerBodyColor.rgb *= bodyColorSaturation;
    outerBodyColor.rgb *= bodyColorSaturation;

    // osu!next style color modifications
    if (style == 1) {
        outerBodyColor.rgb = bodyColor.rgb * bodyColorSaturation;
        outerBodyColor.a = 1.0 * bodyAlphaMultiplier;
        innerBodyColor.rgb = bodyColor.rgb * 0.5 * bodyColorSaturation;
        innerBodyColor.a = 0.0;
    }

    // a bit of a hack, but better than rough edges
    if (borderSizeMultiplier < 0.01)
        borderColor = outerShadowColor;

    // conditional variant

    if (input.tex_coord.x < outerShadowSize - transitionSize) // just shadow
    {
        float delta = input.tex_coord.x / (outerShadowSize - transitionSize);
        out_color = lerp(float4(0, 0, 0, 0), outerShadowColor, delta);
    }
    if (input.tex_coord.x > outerShadowSize - transitionSize && input.tex_coord.x < outerShadowSize + transitionSize) // shadow + border
    {
        float delta = (input.tex_coord.x - outerShadowSize + transitionSize) / (2.0 * transitionSize);
        out_color = lerp(outerShadowColor, borderColor, delta);
    }
    if (input.tex_coord.x > outerShadowSize + transitionSize && input.tex_coord.x < outerShadowSize + borderSize - transitionSize) // just border
    {
        out_color = borderColor;
    }
    if (input.tex_coord.x > outerShadowSize + borderSize - transitionSize && input.tex_coord.x < outerShadowSize + borderSize + transitionSize) // border + outer body
    {
        float delta = (input.tex_coord.x - outerShadowSize - borderSize + transitionSize) / (2.0 * transitionSize);
        out_color = lerp(borderColor, outerBodyColor, delta);
    }
    if (input.tex_coord.x > outerShadowSize + borderSize + transitionSize) // outer body + inner body
    {
        float size = outerShadowSize + borderSize + transitionSize;
        float delta = ((input.tex_coord.x - size) / (1.0 - size));
        out_color = lerp(outerBodyColor, innerBodyColor, delta);
    }

    return out_color;
}
