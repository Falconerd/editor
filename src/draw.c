#include <assert.h>
#include <glad/gl.h>
#include <SDL2/SDL.h>
#include "common.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define BATCH_VERTICES_MAX 4096

u32 draw_vao, draw_vbo;
u32 draw_shader_id;
u32 draw_texture_ids[8] = {0};
int draw_texture_count = 1; // Starts at 1 because of color texture.
u32 draw_texture_color;
bump draw_vertices = {0};
texture draw_font_texture;

extern window_handle window;
extern arena arena_scratch;

void draw_batch();
void draw_begin();
void draw_end();

// OpenGL guarantees that glCreateProgram will return a non-zero value
// if it is successful. If this procedure returns 0, we know that is
// not the case.
u32 draw_shader_create(const char *path_v, const char *path_f, arena *a) {
    b32 ok = 0;
    u32 id = 0;
    char log[512] = {0};

    // Errors printed by os_file_read.
    // NOTE: Maybe os function shouldn't produce errors for users?
    // Perhaps the caller should do something with it.
    file_read_result text_v = os_file_read(path_v, a, a);
    if (!text_v.ok) {
        return 0;
    }
    
    file_read_result text_f = os_file_read(path_f, a, a);
    if (!text_f.ok) {
        return 0;
    }

    u32 shader_v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shader_v, 1, (const char *const *)&text_v.str.data, 0);
    glCompileShader(shader_v);
    glGetShaderiv(shader_v, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(shader_v, 512, 0, log);
        printf("Error compiling shader '%s': %s\n", path_v, log);
        return 0;
    }

    u32 shader_f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shader_f, 1, (const char *const *)&text_f.str.data, 0);
    glCompileShader(shader_f);
    glGetShaderiv(shader_f, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(shader_f, 512, 0, log);
        printf("Error compiling shader '%s': %s\n", path_f, log);
        return 0;
    }

    id = glCreateProgram();
    glAttachShader(id, shader_v);
    glAttachShader(id, shader_f);
    glLinkProgram(id);
    glGetProgramiv(id, GL_LINK_STATUS, &ok);
    if (!ok) {
        glGetProgramInfoLog(id, 512, 0, log);
        printf("Error linking shader\n\t%s\n\t%s\n%s\n", path_v, path_f, log);
        return 0;
    }

    if (!id) {
        printf("Failed to create shader program.\n");
        return 0;
    }

    return id;
}

void draw_init() {
    glEnable(GL_BLEND);
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Setup shader.
    draw_shader_id = draw_shader_create("data/shader_v.glsl", "data/shader_f.glsl", &arena_scratch);

    m4 projection = {0};
    m4_ortho(&projection, 0, RENDER_WIDTH, RENDER_HEIGHT, 0, -1, 1);

    // TODO: Set this on resize.
    glViewport(0, 0, RENDER_WIDTH, RENDER_HEIGHT);

    glUseProgram(draw_shader_id);
    glUniformMatrix4fv(
        glGetUniformLocation(draw_shader_id, "projection"),
        1,
        0,
        &projection.data[0][0]
    );

    int ts[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    glUniform1iv(glGetUniformLocation(draw_shader_id, "texture_slots"), 8, &ts[0]);

    glGenVertexArrays(1, &draw_vao);
    glBindVertexArray(draw_vao);
    glGenBuffers(1, &draw_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, draw_vbo);
    glBufferData(GL_ARRAY_BUFFER, BATCH_VERTICES_MAX * sizeof(batch_vertex), 0, GL_DYNAMIC_DRAW);

    // [x, y, z] [r, g, b, a] [u, v] [t].
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, sizeof(batch_vertex), (void *)offsetof(batch_vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, 0, sizeof(batch_vertex), (void *)offsetof(batch_vertex, color));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, 0, sizeof(batch_vertex), (void *)offsetof(batch_vertex, uvs));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, 0, sizeof(batch_vertex), (void *)offsetof(batch_vertex, slot));

    // Create arena for vertices.
    assert(bump_init_sized(&draw_vertices, BATCH_VERTICES_MAX * sizeof(batch_vertex)));

    // Setup color shader.
    glGenTextures(1, &draw_texture_color);
    glBindTexture(GL_TEXTURE_2D, draw_texture_color);

    u8 solid_white[4] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, solid_white);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Load textures...
    // TODO: Make this nicer.
    {
        int w, h, c;
        byte *data = stbi_load("data/iosevka.png", &w, &h, &c, 0);
        if (data) {
            u32 id;
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);

            draw_font_texture.id = id;
            draw_font_texture.width = w;
            draw_font_texture.height = h;
            draw_font_texture.frame_width = 11;
            draw_font_texture.frame_height = h;
            
            stbi_image_free(data);
        } else {
            printf("Failed to load image: %s\n", stbi_failure_reason());
        }
    }
}

int texture_find_or_append(u32 id) {
    int slot = -1;

    for (int i = 0; i < draw_texture_count; i += 1) {
        if (draw_texture_ids[i] == id) {
            slot = i;
            break;
        }
    }

    if (slot < 0 && draw_texture_count < 8) {
        draw_texture_ids[draw_texture_count] = id;
        slot = draw_texture_count;
        draw_texture_count += 1;
    }

    return slot;
}

int texture_slot_or_flush(u32 id) {
    int slot = texture_find_or_append(id);

    if (draw_vertices.offset == draw_vertices.size || slot < 0) {
        draw_end();
        draw_begin();
        slot = texture_find_or_append(id);
    }

    return slot;
}

void draw_rect(v3 p, v2 sz, v4 *uvs, v4 col, int s) {
    if (draw_vertices.offset + sizeof(batch_vertex) * 6 > draw_vertices.size) {
        draw_batch();
        bump_free_all(&draw_vertices);
    }
    
    v4 _uvs = {0, 0, 1, 1};

    if (uvs) {
        _uvs = *uvs;
    }

    batch_vertex *vert_a = bump_alloc(&draw_vertices, sizeof(batch_vertex));
    batch_vertex *vert_b = bump_alloc(&draw_vertices, sizeof(batch_vertex));
    batch_vertex *vert_c = bump_alloc(&draw_vertices, sizeof(batch_vertex));
    batch_vertex *vert_d = bump_alloc(&draw_vertices, sizeof(batch_vertex));
    batch_vertex *vert_e = bump_alloc(&draw_vertices, sizeof(batch_vertex));
    batch_vertex *vert_f = bump_alloc(&draw_vertices, sizeof(batch_vertex));

    if (!(vert_a && vert_b && vert_c && vert_d && vert_e && vert_f)) {
        panic("Not enough space for vertex.\n");
    }

    // I find these little diagrams useful every time I do this.
    // d--c
    // | /|
    // |/ |
    // a--b
    *vert_a = (batch_vertex){{p.x,        p.y,        p.z}, {col.r, col.g, col.b, col.a}, {_uvs.x, _uvs.y}, s};
    *vert_b = (batch_vertex){{p.x + sz.x, p.y,        p.z}, {col.r, col.g, col.b, col.a}, {_uvs.z, _uvs.y}, s};
    *vert_c = (batch_vertex){{p.x + sz.x, p.y + sz.y, p.z}, {col.r, col.g, col.b, col.a}, {_uvs.z, _uvs.w}, s};
    *vert_d = (batch_vertex){{p.x,        p.y + sz.y, p.z}, {col.r, col.g, col.b, col.a}, {_uvs.x, _uvs.w}, s};
    *vert_e = *vert_a;
    *vert_f = *vert_c;
}

void draw_rect_textured(v3 p, v2 s, v4 c, u32 tid, v4 *uvs) {
    int slot = texture_slot_or_flush(tid);
    draw_rect(p, s, uvs, c, slot);
}

void draw_rect_subtexture(v3 p, v2 sz, v4 c, texture t, int col, int row) {
    f32 w = 1.f / (t.width / t.frame_width);
    f32 h = 1.f / (t.height / t.frame_height);
    f32 x = (f32)col * w;
    f32 y = (f32)row * h;
    v4 uvs = {x, y, x + w, y + h};
    int slot = texture_slot_or_flush(t.id);
    draw_rect(p, sz, &uvs, c, slot);
}

void draw_text(v3 p, s8 s, v4 c) {
    f32 x = p.x;
    f32 y = p.y;
    texture t = draw_font_texture;
    for (int i = 0; i < s.len; i += 1) {
        v3 pos = {x, y, p.z};
        v2 size = {t.frame_width, t.frame_height};
        draw_rect(pos, size, 0, (v4){0, 0, 0, 1}, 0);
        draw_rect_subtexture(pos, (v2){t.frame_width, t.frame_height}, c, t, (int)s.data[i] - '!', 0);
        x += t.frame_width;
        if (s.data[i] == '\n') {
            y += t.frame_height;
            x = p.x;
        }
    }
}

void draw_rope(rope_node *n, v3 *p, v4 c, f32 initial_x) {
    if (!n) {
        return;
    }

    if (!n->left && !n->right) {
        for (usize i = 0; i < n->str.len; i += 1) {
            if (n->str.data[i] == '\n') {
                p->y += draw_font_texture.frame_height;
                p->x = initial_x;
            } else {
                v2 size = {draw_font_texture.frame_width, draw_font_texture.frame_height};
                draw_rect(*p, size, 0, (v4){0, 0, 0, 1}, 0);
                draw_rect_subtexture(*p, size, c, draw_font_texture, (int)n->str.data[i] - '!', 0);
                p->x += draw_font_texture.frame_width;
            }
        }
    } else {
        draw_rope(n->left, p, c, initial_x);
        draw_rope(n->right, p, c, initial_x);
    }
}

v4 style_default = {1, 1, 1, 1};
v4 style_literal = {1, 0, 0, 1};
v4 style_keyword = {1, 1, 0, 1};

// void draw_ast_node(TSNode node, v3 *p);
// void draw_ast_node(TSNode node, v3 *p) {
//     v4 style = {1, 1, 1, 1};
//     const char *node_type = ts_node_type(node);
//     if (strcmp(node_type, "function_definition") == 0) {
//         style = style_keyword;
//     } else if (strcmp(node_type, "number_literal") == 0) {
//         style = style_literal;
//     }

//     rope_node n = ast_node_to_rope(node);
//     draw_rope(&n, p, style, p->x);

//     u32 child_count = ts_node_name_child_count(node);
//     for (u32 i = 0; i < child_count; i += 1) {
//         TSNode child = ts_node_named_child(node, i);
//         draw_ast_node(child, position);
//     }
// }

void draw_batch() {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, draw_texture_color);

    for (int i = 1; i < 8; i += 1) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, draw_texture_ids[i]);
    }

    glUseProgram(draw_shader_id);
    glBindVertexArray(draw_vao);
    glBindBuffer(GL_ARRAY_BUFFER, draw_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, draw_vertices.offset, draw_vertices.buf);
    glDrawArrays(GL_TRIANGLES, 0, draw_vertices.offset / sizeof(batch_vertex));
}

void draw_begin() {
    glClearColor(0.f, 0.1098f, 0.1098f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    bump_free_all(&draw_vertices);
}

void draw_end() {
    draw_batch();
    SDL_GL_SwapWindow(window);
}
