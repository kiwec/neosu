#version 450

layout(location = 0) in vec2 tex_coord;
layout(location = 1) in float vtx_alpha;

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D tex0;

void main() {
    vec4 color = texture(tex0, tex_coord);
    color.a *= vtx_alpha;
    outColor = color;
}
