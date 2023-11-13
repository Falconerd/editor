#version 330 core

layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec4 a_color;
layout (location = 2) in vec2 a_uvs;
layout (location = 3) in int a_texture_slot;

out vec4 v_color;
out vec2 v_uvs;
flat out int v_texture_slot;

uniform mat4 projection;

void main() {
    v_color = a_color;
    v_uvs = a_uvs;
    v_texture_slot = a_texture_slot;
    gl_Position = projection * vec4(a_pos, 1);
}
