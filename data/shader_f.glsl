#version 330 core
out vec4 o_color;

in vec4 v_color;
in vec2 v_uvs;
flat in int v_texture_slot;

uniform sampler2D texture_slots[8];

void main() {
    switch (v_texture_slot) {
        case 0: o_color = texture(texture_slots[0], v_uvs) * v_color; break;
        case 1: o_color = texture(texture_slots[1], v_uvs) * v_color; break;
        case 2: o_color = texture(texture_slots[2], v_uvs) * v_color; break;
        case 3: o_color = texture(texture_slots[3], v_uvs) * v_color; break;
        case 4: o_color = texture(texture_slots[4], v_uvs) * v_color; break;
        case 5: o_color = texture(texture_slots[5], v_uvs) * v_color; break;
        case 6: o_color = texture(texture_slots[6], v_uvs) * v_color; break;
        case 7: o_color = texture(texture_slots[7], v_uvs) * v_color; break;
        default: discard;
    }
}