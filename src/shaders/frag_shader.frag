#version 450

layout(binding = 1) uniform sampler samp;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

void main() {
    out_color = texture(samp, uv);
}
