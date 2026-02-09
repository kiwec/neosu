cbuffer FragParams : register(b0, space3) {
    float max_opacity;
    float flashlight_radius;
    float2 flashlight_center;
};

struct PSInput {
    float4 position : SV_Position;
};

float4 main(PSInput input) : SV_Target0 {
    float dist = distance(flashlight_center, input.position.xy);
    float opacity = 1.0 - smoothstep(flashlight_radius, flashlight_radius * 1.4, dist);
    opacity = 1.0 - min(opacity, max_opacity);
    return float4(0.0, 0.0, 0.0, opacity);
}
