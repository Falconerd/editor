#include "common.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#define NOMINMAX
#include <SDL2/SDL.h>
#include <glad/gl.h>

#include <tree_sitter/api.h>
#include <tree_sitter/parser.h>

#include <assert.h>
#include "math.c"
#include "mem.c"
#include "bump.c"
#include "arena.c"
#include "pool.c"
#include "string.c"
#include "os.c"
#include "draw.c"

typedef enum Mode {
    MODE_NORMAL,
    MODE_INSERT,
} Mode;

Mode mode = MODE_NORMAL;

void test_all(void);

b32 app_should_run = 1;
arena arena_permanent;
arena arena_scratch;
window_handle window;

#define TEXT_BUFFER_COUNT 10
u8 *text_buffers[TEXT_BUFFER_COUNT] = {0};
usize used_text_buffers = 0; // Undo steps, can't be greater than TEXT_BUFFER_COUNT
usize current_text_buffer = 0;
usize current_buffer_len = 0;

u64 cursor_line = 1;
u64 cursor_col = 1;
u64 line_count = 0;
usize line_lengths[200] = {0}; // TODO: Use dynamic length

/*
    So, my idea is that we can use a rolling set of text buffers.
    How will these buffers be sized? Each one could just be a 10MB
    block that gets overwritten on each insert... Speaking of insert,
    how is that going to work?
*/

TSLanguage *tree_sitter_c();

usize cursor_index() {
    usize pos = 0;
    for (usize i = 1; i <= line_lengths[i]; i += 1) {
        if (i == cursor_line) {
            break;
        }
        pos += line_lengths[i];
    }
    pos += cursor_col - 1;
    return pos;
}

u8 char_at_cursor() {
    // FIXME Inefficient?
    // Count the characters until this line...
    return text_buffers[current_text_buffer][cursor_index()];
}

typedef enum Cursor_Movement {
    CURSOR_MOVEMENT_LEFT,
    CURSOR_MOVEMENT_RIGHT,
    CURSOR_MOVEMENT_DOWN,
    CURSOR_MOVEMENT_UP,
} Cursor_Movement;

void cursor_move(Cursor_Movement m) {
    switch (m) {
    case CURSOR_MOVEMENT_LEFT: {
        if (cursor_col > 1) {
            cursor_col -= 1;
        }
    } break;
    case CURSOR_MOVEMENT_RIGHT: {
        if (cursor_col < line_lengths[cursor_line])
        cursor_col += 1;
    } break;
    case CURSOR_MOVEMENT_DOWN: {
        if (cursor_line == line_count) {
            break;
        }

        // Special case to stick to the ends of lines.
        b32 at_eol = char_at_cursor() == '\n' || char_at_cursor() == 0;

        cursor_line += 1;
        if (line_lengths[cursor_line] < cursor_col || at_eol) {
            cursor_col = line_lengths[cursor_line];
        }
    } break;
    case CURSOR_MOVEMENT_UP: {
        if (cursor_line == 1) {
            break;
        }

        // Special case to stick to the ends of lines.
        b32 at_eol = char_at_cursor() == '\n' || char_at_cursor() == 0;

        cursor_line -= 1;
        if (line_lengths[cursor_line] < cursor_col || at_eol) {
            cursor_col = line_lengths[cursor_line];
        }
    } break;
    }
    printf("char: %c\n", char_at_cursor());
}

usize next_buffer_index() {
    usize index = current_text_buffer + 1;
    if (index == TEXT_BUFFER_COUNT) {
        index = 0;
    }
    return index;
}

void populate_line_lengths() {
    s8 llstr = {text_buffers[current_text_buffer], current_buffer_len};
    for (usize i = 1; i <= line_count; i += 1) {
        // FIXME: This is super stupid as we check on every line.
        if (i == line_count) {
            line_lengths[i] = string_count_until(llstr, 0) + 1;
        } else {
            line_lengths[i] = string_count_until(llstr, '\n') + 1;
        }
        llstr.data += line_lengths[i];
    }
}

void insert(u8 c, usize index) {
    usize from_buffer = current_text_buffer;
    usize to_buffer = next_buffer_index();

    mem_copy(text_buffers[to_buffer], text_buffers[from_buffer], index);
    text_buffers[to_buffer][index] = c;
    mem_copy(&text_buffers[to_buffer][index + 1], &text_buffers[from_buffer][index], current_buffer_len + 1 - index);
    current_buffer_len += 1;
    current_text_buffer = to_buffer;

    cursor_col += 1;
    line_lengths[cursor_line] += 1;
}

void delete(usize index) {
    printf("delet from index: %zu\n", index);
    usize to_buffer = next_buffer_index();
    mem_copy(text_buffers[to_buffer], text_buffers[current_text_buffer], index);
    mem_copy(&text_buffers[to_buffer][index - 1], &text_buffers[current_text_buffer][index], current_buffer_len - index);
    current_buffer_len -= 1;
    current_text_buffer = to_buffer;
    cursor_col -= 1;
    line_lengths[cursor_line] -= 1;
}

void linebreak(usize index) {
    // TODO: Make this smarter.
    // Insert the '\n' character.
    insert('\n', index);
    // Re-count the line-lengths.
    populate_line_lengths();
    // Move down 1 line.
    cursor_line += 1;
    cursor_col = 1;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    assert(arena_init(&arena_permanent));
    assert(arena_init(&arena_scratch));

    // Init text buffers...
    for (int i = 0; i < TEXT_BUFFER_COUNT; i += 1) {
        u8 *buf = (u8 *)arena_alloc(&arena_permanent, 10 * MB);
        if (!buf) {
            panic("Unable to allocate space for buffer.");
        }
        text_buffers[i] = buf;
    }

    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_c());

    pool test_pool;
    assert(pool_init(&test_pool, &arena_permanent, sizeof(int), 10));
    printf("%p %p\n", test_pool.buf, test_pool.used);
    printf("%zu\n", (uptr)test_pool.used - (uptr)test_pool.buf);
    for (int i = 0; i < 10; i += 1) {
        void *item = pool_next(&test_pool);
        int *ptr= item;
        *ptr = i;
    }

    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetSwapInterval(1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    window = SDL_CreateWindow("Editor", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, RENDER_WIDTH, RENDER_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        printf("Failed to load GL: %s\n", SDL_GetError());
    }
    
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
    printf("Vendor: %s\n", glGetString(GL_VENDOR));
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    draw_init();

    s8 s = s8("This is a string test.");


    file_read_result fr = os_file_read("test_file.c", &arena_permanent, &arena_scratch);
    if (!fr.ok) {
        printf("Error reading test file.\n");
        exit(1);
    }

    // Copy data into the first text buffer
    for (usize i = 0; i < fr.str.len; i += 1) {
        text_buffers[0][i] = fr.str.data[i];
        // FIXME: Hack to just have a 0 at the end.
        text_buffers[0][i + 1] = 0;
        printf("copied '%c'\n", text_buffers[0][i]);
    }
    current_buffer_len = fr.str.len;

    line_count = string_line_count(fr.str) + 1;
    populate_line_lengths();
    
    TSTree *tree = ts_parser_parse_string(parser, NULL, (const char *)fr.str.data, fr.str.len);

    TSNode root_node = ts_tree_root_node(tree);

    char *string = ts_node_string(root_node);
    // printf("%s\n", string);
    free(string);

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    v2 cursor_size = {draw_font_texture.frame_width, draw_font_texture.frame_height};

    SDL_Event event;
    while (app_should_run) {
        b32 was_in_insert = mode == MODE_INSERT;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                app_should_run = 0;
                break;
             case SDL_TEXTINPUT:
                if (mode == MODE_INSERT && was_in_insert) {
                    // Here, event.text.text is the input text
                    char c = event.text.text[0]; // If you just want the first character
                    insert(c, cursor_index());
                    break;
                }
                break;
            case SDL_KEYDOWN:
                if (mode == MODE_INSERT) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        mode = MODE_NORMAL;
                    } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
                        delete(cursor_index());
                    } else if (event.key.keysym.sym == SDLK_RETURN) {
                        linebreak(cursor_index());
                    }
                } else {
                    switch (event.key.keysym.sym) {
                    case SDLK_h:
                        cursor_move(CURSOR_MOVEMENT_LEFT);
                        break;
                    case SDLK_l:
                        cursor_move(CURSOR_MOVEMENT_RIGHT);
                        break;
                    case SDLK_j:
                        cursor_move(CURSOR_MOVEMENT_DOWN);
                        break;
                    case SDLK_k:
                        cursor_move(CURSOR_MOVEMENT_UP);
                        break;
                    case SDLK_i:
                        mode = MODE_INSERT;
                        break;
                    }
                }
                break;
            }
        }

        v3 cursor_pos = {(f32)(cursor_col - 1.f) * draw_font_texture.frame_width,
                        (f32)(cursor_line - 1.f) * draw_font_texture.frame_height, 0};
        
        
        draw_begin();
        // for (int i = 0; i < 8; i += 1) {
        //     f32 x = rand() % RENDER_WIDTH;
        //     f32 y = rand() % RENDER_HEIGHT;
        //     f32 w = rand() % 30;
        //     f32 h = rand() % 30;
        //     f32 r = (rand() % 100) / 100.f;
        //     f32 g = (rand() % 100) / 100.f;
        //     f32 b = (rand() % 100) / 100.f;
        //     f32 a = (rand() % 100) / 100.f;
        //     draw_rect((v3){x, y, 0}, (v2){w, h}, 0, (v4){r, g, b, a}, 0);

        // }
        // draw_rect_subtexture((v3){8, 8, 0}, (v2){8, 16}, (v4){1, 1, 1, 1}, draw_font_texture, 0, 0);

        v3 starting_position = {4, 4, 0};
        // draw_rope(&root, &starting_position, (v4){1, 1, 1, 1}, starting_position.x);
    
        draw_text((v3){0, 0, 0}, (s8){text_buffers[current_text_buffer], current_buffer_len}, (v4){1, 1, 1, 1});
        
        draw_rect(cursor_pos, cursor_size, 0, (v4){1, 1, 1, 1}, 0);

        char buf[40] = {0};
        char *mode_text = "NORMAL";
        if (mode == MODE_INSERT) {
            mode_text = "INSERT";
        }
        sprintf(buf, "%zu:%zu %zuL, %zu, %s", cursor_line, cursor_col, line_count, cursor_index(), mode_text);

        draw_text((v3){RENDER_WIDTH - draw_font_texture.frame_width * 40, 0, 0}, s8(buf), COLOR_WHITE);
// void draw_rect_subtexture(v3 p, v2 sz, v4 c, texture t, int col, int row) {
        draw_end();
    }

    test_all();

    return 0;
}

void do_something_with(void *x) {
    return;
}

void test_all(void) {
    // s8 s = s8("Wow!");
    // printf("s: %s. s.len: %zu\n", s.data, s.len);
    // printf("Press <return> to continue...\n");
    // getc(stdin);
}