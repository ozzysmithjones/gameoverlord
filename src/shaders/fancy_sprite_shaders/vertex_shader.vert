#version 450

// TODO: work out how to bind these buffers from the vulkan side

layout(std430, set = 0, binding = 0) buffer translate_block {
    vec2 translations[]; 
}

layout(std430, set = 0, binding = 1) buffer scale_block {
    vec2 scales[]; 
}

layout(std430, set = 0, binding = 2) buffer rotation_block {
    float rotations[]; 
}

layout(std430, set = 0, binding = 3) buffer material_id_block {
    uint texture_indices[];
}

vec2 transform2d(in vec2 vertex, const in vec2 translation, const int vec2 scale, const in float rotation) {
    mat2x2 rot_matrix = mat2x2(cos(rotation), -sin(rotation), sin(rotation),  cos(rotation));
    vertex *= scale;
    vertex = rot_matrix * vertex;
    vertex += translation;
    return vertex;      
}

layout(location = 0) out uint texture_index;
layout(location = 1) out vec2 texcoords;
void main() {
    const uint quad_tri_count = 6;
    const uint sprite_index = gl_VertexIndex / quad_tri_count;
    const uint vertex_index = gl_VertexIndex % quad_tri_count;
    const uint corner_index = vertex_index > 2 ? (vertex_index - 1) % 4 : vertex_index;

    vec2 vertex = vec2(float(corner_index == 2 || corner_index == 3), float(corner_index == 1 || corner_index == 2));
    vec2 translation = translations[sprite_index];
    vec2 scale = scales[sprite_index];
    float rotation = rotations[sprite_index];
    texture_index = texture_indices[sprite_index];
    texcoords = vertex;
    vertex = transform2D(vertex, translation, scale, rotation);
    gl_Position = vec4(vertex, 0.0, 1.0);
}