#version {RUNTIME_VERSION}

#ifdef GL_ES

precision highp float;

#endif

uniform sampler2D tex;

varying vec2 tex_coord;
varying float vtx_alpha;

void main()
{
	vec4 color = texture2D(tex, tex_coord);
	color.a *= vtx_alpha;

	gl_FragColor = color; 
}
