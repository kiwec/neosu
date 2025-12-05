#version {RUNTIME_VERSION}

#ifdef GL_ES

precision highp float;

uniform sampler2D texture;
uniform vec2 rect_min;
uniform vec2 rect_max;
uniform float edge_softness;
uniform vec4 col;

varying vec2 tex_coord;

void main()
{
	vec2 dist_to_edges = min(gl_FragCoord.xy - rect_min, rect_max - gl_FragCoord.xy);
	float min_dist = min(dist_to_edges.x, dist_to_edges.y);

	float alpha = smoothstep(-edge_softness, 0.0, min_dist);
	vec4 texColor = texture2D(texture, tex_coord);
	gl_FragColor = vec4(texColor.rgb * col.rgb, texColor.a * col.a * alpha);
}

#else

uniform sampler2D texture;
uniform vec2 rect_min;
uniform vec2 rect_max;
uniform float edge_softness;
varying vec2 tex_coord;

void main() {
	vec2 dist_to_edges = min(gl_FragCoord.xy - rect_min, rect_max - gl_FragCoord.xy);
	float min_dist = min(dist_to_edges.x, dist_to_edges.y);

	float alpha = smoothstep(-edge_softness, 0.0, min_dist);
	vec4 texColor = texture2D(texture, tex_coord);
	gl_FragColor = vec4(texColor.rgb * gl_Color.rgb, texColor.a * gl_Color.a * alpha);
}

#endif
