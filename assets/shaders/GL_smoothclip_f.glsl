#version 110

uniform sampler2D texture;
uniform vec2 rect_min; // clip rect in gl_FragCoord space (bottom-left origin)
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