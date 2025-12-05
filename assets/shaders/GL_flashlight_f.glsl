#version {RUNTIME_VERSION}

#ifdef GL_ES

precision highp float;

uniform float max_opacity;
uniform float flashlight_radius;
uniform vec2 flashlight_center;

void main()
{
	float dist = distance(flashlight_center, gl_FragCoord.xy);
	float opacity = 1.0 - smoothstep(flashlight_radius, flashlight_radius * 1.4, dist);
	opacity = 1.0 - min(opacity, max_opacity);
	gl_FragColor = vec4(0.0, 0.0, 0.0, opacity);
}

#else

uniform float max_opacity;
uniform float flashlight_radius;
uniform vec2 flashlight_center;

void main(void) {
	float dist = distance(flashlight_center, gl_FragCoord.xy);
	float opacity = 1.0 - smoothstep(flashlight_radius, flashlight_radius * 1.4, dist);
	opacity = 1.0 - min(opacity, max_opacity);
	gl_FragColor = vec4(0.0, 0.0, 0.0, opacity);
}

#endif
