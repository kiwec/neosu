#version 110

uniform float max_opacity;
uniform float flashlight_radius;
uniform vec2 flashlight_center;

void main(void) {
	float dist = distance(flashlight_center, gl_FragCoord.xy);
	float opacity = smoothstep(flashlight_radius, flashlight_radius * 1.4, dist);
	opacity = 1.0 - min(opacity, max_opacity);
	gl_FragColor = vec4(1.0, 1.0, 0.9, opacity);
}
