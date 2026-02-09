#version 450

layout(location = 0) out vec4 outColor;

layout(set = 3, binding = 0) uniform FragParams {
    float max_opacity;
    float flashlight_radius;
    vec2 flashlight_center;
};

void main() {
    float dist = distance(flashlight_center, gl_FragCoord.xy);
    float opacity = smoothstep(flashlight_radius, flashlight_radius * 1.4, dist);
    opacity = 1.0 - min(opacity, max_opacity);
    outColor = vec4(1.0, 1.0, 0.9, opacity);
}
