#version 450
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler samp;
layout(set = 0, binding = 1) uniform texture2D textures[8];

layout(location = 0) in uint texture_index;
layout(location = 1) in vec2 texcoords;

void main() {
    out_color = texture(sampler2D(textures[texture_index], samp), texcoords);
}