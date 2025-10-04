#version 110

varying vec2 tex_coord;

void main() {
	gl_Position = gl_ModelViewProjectionMatrix * vec4(gl_Vertex.x, gl_Vertex.y, 0.0, 1.0);
	gl_FrontColor = gl_Color;
	tex_coord = gl_MultiTexCoord0.xy;
}
