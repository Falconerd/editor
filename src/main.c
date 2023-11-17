#include "common.h"
#include <SDL2/SDL_keycode.h>
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
// #include "fuzzy.c"

// TODO: Put somewhere?
u8 leap_chars[3] = {0};
u8 leap_chars_count = 0;
b32 ctrl_q_pressed = 0;
v3 PADDING = {4, 36, 0};
TSParser *parser = 0;
TSTree *tree = 0;
TSNode root_node = {0};

typedef enum Mode {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_INSERT_BEGIN, // The first insert.
    MODE_LEAP_1, // Waiting for 1st input
    MODE_LEAP_2, // Waiting for 2nd input
} Mode;

Mode mode = MODE_NORMAL;

void test_all(void);

b32 app_should_run = 1;
arena arena_permanent;
arena arena_scratch;
window_handle window;

#define TEXT_BUFFER_COUNT 10
u8 *text_buffers[TEXT_BUFFER_COUNT] = {0};
u8 *leap_buffer = 0;
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
    for (usize i = 1; i < cursor_line; i += 1) {
        pos += line_lengths[i];
    }
    pos += cursor_col - 1;
    return pos;
}

u8 char_at(usize index) {
    return text_buffers[current_text_buffer][index];
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
}

b32 is_alpha_numberic(u8 c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
}

b32 is_punctuation(u8 c) {
    return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') || (c >= '{' && c <= '~');
}

b32 is_whitespace(u8 c) {
    return c == ' ' || c == '\t' || c == '\n';
}

void update_cursor_position_from_index(usize index) {
    usize line = 1;
    usize col = 1;

    for (usize i = 0; i < index && i < current_buffer_len; i += 1) {
        if (text_buffers[current_text_buffer][i] == '\n') {
            line += 1;
            col = 1;
        } else {
            col += 1;
        }
    }

    cursor_line = line;
    cursor_col = col;
}

void move_cursor_to_next_word() {
    usize index = cursor_index();
    u8 *buf = text_buffers[current_text_buffer];

    if (is_alpha_numberic(buf[index])) {
        while (index < current_buffer_len && is_alpha_numberic(buf[index])) {
            index += 1;
        }
    } else if (is_punctuation(buf[index])) {
        while (index < current_buffer_len && is_punctuation(buf[index])) {
            index += 1;
        }
    }

    while (index < current_buffer_len && is_whitespace(buf[index])) {
        index += 1;
    }

    update_cursor_position_from_index(index);
}

void move_cursor_to_previous_word() {
    usize index = cursor_index();
    u8 *buf = text_buffers[current_text_buffer];

    if (index > 0) {
        index -= 1;

        while (index > 0 && is_whitespace(buf[index])) {
            index -= 1;
        }

        if (is_alpha_numberic(buf[index])) {
            while (index > 0 && is_alpha_numberic(buf[index - 1])) {
                index -= 1;
            }
        } else if (is_punctuation(buf[index])) {
            while (index > 0 && is_punctuation(buf[index - 1])) {
                index -= 1;
            }
        }
    }

    update_cursor_position_from_index(index);
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
    line_count = string_line_count(llstr) + 1;
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

void update_tree() {
    ts_tree_delete(tree);
    tree = ts_parser_parse_string(parser, NULL, (const char *)text_buffers[current_text_buffer], current_buffer_len);
    root_node = ts_tree_root_node(tree);
}

void update_leap_buffer() {
    for (usize i = 0; i < current_buffer_len; i += 1) {
        leap_buffer[i] = text_buffers[current_text_buffer][i];
    }
}

void insert(u8 c, usize index) {
    // Don't insert 'i'
    if (mode == MODE_INSERT_BEGIN) {
        mode = MODE_INSERT;
        return;
    }

    usize from_buffer = current_text_buffer;
    usize to_buffer = next_buffer_index();

    mem_copy(text_buffers[to_buffer], text_buffers[from_buffer], index);
    text_buffers[to_buffer][index] = c;
    mem_copy(&text_buffers[to_buffer][index + 1], &text_buffers[from_buffer][index], current_buffer_len + 1 - index);
    current_buffer_len += 1;
    current_text_buffer = to_buffer;

    cursor_col += 1;
    line_lengths[cursor_line] += 1;

    update_leap_buffer();
    update_tree();
}

void delete(usize index) {
    usize to_buffer = next_buffer_index();
    // Special case if deleting \n!
    if (char_at(cursor_index() - 1) == '\n') {
        mem_copy(text_buffers[to_buffer], text_buffers[current_text_buffer], index);
        mem_copy(&text_buffers[to_buffer][index - 1], &text_buffers[current_text_buffer][index], current_buffer_len - index);
        current_buffer_len -= 1;
        // Move cursor to end of previous line...
        cursor_line -= 1;
        populate_line_lengths();
        cursor_col = line_lengths[cursor_line];
        current_text_buffer = to_buffer;
    } else {
        mem_copy(text_buffers[to_buffer], text_buffers[current_text_buffer], index);
        mem_copy(&text_buffers[to_buffer][index - 1], &text_buffers[current_text_buffer][index], current_buffer_len - index);
        current_buffer_len -= 1;
        current_text_buffer = to_buffer;
        cursor_col -= 1;
        line_lengths[cursor_line] -= 1;
    }
    update_leap_buffer();
    update_tree();
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
    update_leap_buffer();
    update_tree();
}

s8 get_node_string(TSNode n) {
    u32 start = ts_node_start_byte(n);
    u32 end = ts_node_end_byte(n);
    return (s8){&text_buffers[current_text_buffer][start], end - start};
}

void draw_syntax_highlighted_text(TSNode n) {
    if (ts_node_is_null(n)) {
        return;
    }

    TSSymbol symbol = ts_node_symbol(n);
    TSPoint start = ts_node_start_point(n);
    TSPoint end = ts_node_end_point(n);

    const char *type = ts_node_type(n);
    // printf("Node type: %s\n", type);
    // printf("Node symbol: %d\n", symbol);
    // printf("Start: Line %u, Column %u\n", start.row, start.column);
    // printf("End: Line %u, Column %u\n", end.row, end.column);
    // char buf[100] = {0};
    s8 node_string = get_node_string(n);
    // mem_copy(buf, node_string.data, node_string.len);
    // printf("String: %s\n", buf);

        // v3 cursor_pos = {PADDING.x + (f32)(cursor_col - 1.f) * draw_font_texture.frame_width,
        //                  PADDING.y + (f32)(cursor_line - 1.f) * draw_font_texture.frame_height, 0};
    v3 text_pos = {
        PADDING.x + (f32)start.column * draw_font_texture.frame_width,
        PADDING.y + (f32)start.row * draw_font_texture.frame_height,
        0
    };

    v4 color_normal = {0.8f, 0.8f, 0.8f, 1};
    v4 color_type = {89.f / 255.f, 194.f / 255.f, 255.f / 255.f, 1};
    v4 color_ident = {230.f / 255.f, 180.f / 255.f, 80.f / 255.f, 1};
    v4 color_keyword = {255.f / 255.f, 143.f / 255.f, 64.f / 255.f, 1};
    v4 color_lit = {210.f / 255.f, 166.f / 255.f, 255.f / 255.f, 1};
    v4 color_comment = {0, 219.f / 255.f, 0, 1};

    switch (symbol) {
    case 1: // identifier
        draw_text(text_pos, node_string, color_ident);
        break;
    case 89: // primitive_type
        draw_text(text_pos, node_string, color_type);
        break;
    case 102: // return
    case 44: // typedef
    case 94: // if
    case 95: // else
    case 96: // switch
    case 97: // case
    case 98: // default
    case 99: // while
    case 100: // do
    case 101: // for
    case 103: // break
    case 104: // continue
    case 105: // goto
        draw_text(text_pos, node_string, color_keyword);
        break;
    case 135: // number_literal
    case 293: // char_literal
    case 294: // concatenated_string
    case 295: // string_literal
        draw_text(text_pos, node_string, color_lit);
        break;
    case 154: // comment
        draw_text(text_pos, node_string, color_normal);
        break;
    case 63: // LBRACE
    case 64: // RBRACE
    case 69: // LBRACK
    case 70: // RBRACK
    case 5:  // LPAREN
    case 8:  // RPAREN
    case 26: // STAR
    case 27: // SLASH
    case 28: // PERCENT
    case 25: // PLUS
    case 24: // DASH
    case 22: // BANG
    case 23: // TILDE
    case 29: // PIPE_PIPE
    case 30: // AMP_AMP
    case 31: // PIPE
    case 32: // CARET
    case 33: // AMP
    case 34: // EQ_EQ
    case 35: // BANG_EQ
    case 36: // GT
    case 37: // GT_EQ
    case 38: // LT_EQ
    case 39: // LT
    case 40: // LT_LT
    case 41: // GT_GT
    case 42: // SEMI
    case 91: // COLON
    case 133: // DOT
    case 134: // DASH_GT
    case 71: // EQ
    case 110: // QMARK
    case 111: // STAR_EQ
    case 112: // SLASH_EQ
    case 113: // PERCENT_EQ
    case 114: // PLUS_EQ
    case 115: // DASH_EQ
    case 116: // LT_LT_EQ
    case 117: // GT_GT_EQ
    case 118: // AMP_EQ
    case 119: // CARET_EQ
    case 120: // PIPE_EQ
    case 121: // DASH_DASH
    case 122: // PLUS_PLUS
    case 156: // INCLUDE
    case 157: // DEFINE
    case 66: // UNSIGNED
    case 81: // CONST
    case 92: // STRUCT
    case 330: // STRUCT FIELD
    case 332: // TYPEDEF IDENT
        draw_text(text_pos, node_string, color_normal);
        break;
    default:
        // printf("symbol not covered: %d (%s)\n", symbol, node_string.data);
        // draw_text(text_pos, node_string, (v4){1, 0, 0, 1});
        break;
    }

    u32 child_count = ts_node_child_count(n);

    for (u32 i = 0; i < child_count; i += 1) {
        TSNode c = ts_node_child(n, i);
        draw_syntax_highlighted_text(c);
    }
}

void draw_frame(void) {
    v3 cursor_pos = {PADDING.x + (f32)(cursor_col - 1.f) * draw_font_texture.frame_width,
                     PADDING.y + (f32)(cursor_line - 1.f) * draw_font_texture.frame_height, 0};
    
    draw_begin();

    // Text
    // draw_text(PADDING, (s8){text_buffers[current_text_buffer], current_buffer_len}, (v4){1, 1, 1, 1});

    if (mode == MODE_LEAP_1) {
        draw_text(PADDING, (s8){text_buffers[current_text_buffer], current_buffer_len}, (v4){0.2, 0.2, 0.2, 1});
    } else if (mode == MODE_LEAP_2) {
        draw_text(PADDING, (s8){text_buffers[current_text_buffer], current_buffer_len}, (v4){0.2, 0.2, 0.2, 1});
        draw_text(PADDING, (s8){leap_buffer, current_buffer_len}, (v4){1, 0.2, 0.2, 1});
    } else {
        draw_text(PADDING, (s8){text_buffers[current_text_buffer], current_buffer_len}, (v4){0.2, 0.2, 0.2, 1});
        draw_syntax_highlighted_text(root_node);
    }
    
    // Cursor
    v2 cursor_size = {draw_font_texture.frame_width, draw_font_texture.frame_height};
    draw_rect(cursor_pos, cursor_size, 0, (v4){1, 0.5f, 0.2f, 1}, 0);
    // Cursor text.
    // NOTE: Later this will be a range.
    char cursor_char[1] = {char_at_cursor()};
    draw_text(cursor_pos, (s8){cursor_char, 1}, (v4){0, 0, 0, 1});
    
    // Status line
    char buf[255] = {0};
    char *mode_text = "NORMAL";
    if (mode == MODE_INSERT) {
        mode_text = "INSERT";
    } else if (mode == MODE_LEAP_1 || mode == MODE_LEAP_2) {
        mode_text = "LEAP";
    }
    sprintf(buf, "%zu:%zu %zuLC, %zuLL, %zu, %s", cursor_line, cursor_col, line_count, line_lengths[cursor_line], cursor_index(), mode_text);
    draw_text((v3){0, 0, 0}, s8(buf), COLOR_WHITE);

    draw_end();
}

u8 leap_labels[] = {
                    'a', 'r', 's', 't', 'd',
                    'h', 'n', 'e', 'i', 'o',
                    'q', 'w', 'f', 'p', 'g',
                    'j', 'l', 'u', 'y',
                    'z', 'x', 'c', 'v', 'b',
                    'k', 'm',
                    'A', 'R', 'S', 'T', 'D',
                    'H', 'N', 'E', 'I', 'O',
                    'Q', 'W', 'F', 'P', 'G',
                    'J', 'L', 'U', 'Y',
                    'Z', 'X', 'C', 'V', 'B',
                    'K', 'M',
};

void assign_leap_labels(u8 fc) {
    usize li = 0;
    u8 *buf = text_buffers[current_text_buffer];
    for (usize i = 0; i < current_buffer_len; i += 1) {
        if (buf[i] == fc) {
            leap_buffer[i] = leap_labels[li];
            li = (li + 1) % sizeof(leap_labels);
            if (leap_labels[li] == fc) {
                li = (li + 1) % sizeof(leap_labels);
            }
        } else if (buf[i] == '\n' || buf[i] == 0) {
            leap_buffer[i] = buf[i];
        } else {
            leap_buffer[i] = ' ';
        }
    }
}

void leap() {
    if (leap_chars[0] == leap_chars[1]) {
        return;
    }
    mode = MODE_NORMAL;
    // TODO: Make less inefficient.
    u64 col = 1;
    u64 line = 1;
    for (usize i = 0; i < current_buffer_len; i += 1) {
        if (leap_buffer[i] == leap_chars[1]) {
            break;
        }
        col += 1;
        if (leap_buffer[i] == '\n') {
            line += 1;
            col = 1;
        }
    }
    
    cursor_col = col;
    cursor_line = line;
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

    // Init leap buffer.
    // Only has to be large enough to fit 1 screen of stuff...
    {
        u8 *buf = (u8 *)arena_alloc(&arena_permanent, 2048);
        if (!buf) {
            panic("");
        }
        leap_buffer = buf;
    }

    parser = ts_parser_new();
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

    file_read_result fr = os_file_read("test_file.c", &arena_permanent, &arena_scratch);
    if (!fr.ok) {
        printf("Error reading test file.\n");
        exit(1);
    }

    // Copy data into the first text buffer
    for (usize i = 0; i < fr.str.len; i += 1) {
        text_buffers[0][i] = fr.str.data[i];
        leap_buffer[i] = fr.str.data[i];
        // FIXME: Hack to just have a 0 at the end.
        text_buffers[0][i + 1] = 0;
        leap_buffer[i + 1] = 0;
    }
    current_buffer_len = fr.str.len;

    populate_line_lengths();
    
    tree = ts_parser_parse_string(parser, NULL, (const char *)fr.str.data, fr.str.len);
    root_node = ts_tree_root_node(tree);

    SDL_Event event;
    while (app_should_run) {
        while (SDL_WaitEvent(&event) && app_should_run) {
            // TODO: Clean up.
            // - probably have modal keybinds instead of keydown events that check the mode.
            switch (event.type) {
            case SDL_QUIT:
                app_should_run = 0;
                break;
             case SDL_TEXTINPUT:
                if (mode == MODE_INSERT || mode == MODE_INSERT_BEGIN) {
                    char c = event.text.text[0];
                    insert(c, cursor_index());
                    break;
                }
                break;
            case SDL_KEYUP:
                if (event.key.keysym.sym == SDLK_LCTRL || event.key.keysym.sym == SDLK_RCTRL) {
                    ctrl_q_pressed = 0;
                }
                break;
            case SDL_KEYDOWN: {
                switch (mode) {
                case MODE_INSERT_BEGIN: {

                } break;
                case MODE_INSERT: {
                    switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: {
                        mode = MODE_NORMAL;
                    } break;
                    case SDLK_BACKSPACE: {
                        delete(cursor_index());
                    } break;
                    case SDLK_RETURN: {
                        linebreak(cursor_index());
                    } break;
                    }
                } break;
                case MODE_NORMAL: {
                    // Quit.
                    if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod & KMOD_CTRL) > 0) {
                        if (ctrl_q_pressed) {
                            app_should_run = 0;
                        } else {
                            ctrl_q_pressed = 1;
                        }
                        break;
                    }

                    // Leap.
                    if (event.key.keysym.sym == SDLK_l) {
                        mode = MODE_LEAP_1;
                        break;
                    }

                    switch (event.key.keysym.sym) {
                    /*
                        NOTE: Rethinking movement.
                            Perhaps the idea of a single char/line move
                        is not even required.
                            It'll take 1 extra key-press to move 1 char
                        but, since most movements are more than 1, it
                        should be a net saving.

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
                    */
                    case SDLK_w:
                        move_cursor_to_next_word();
                        break;
                    case SDLK_b:
                        move_cursor_to_previous_word();
                        break;
                    case SDLK_i:
                        mode = MODE_INSERT_BEGIN;
                        break;
                    case SDLK_x:
                        printf("char: '%c'\n", char_at_cursor());
                        break;
                    case SDLK_t:
                        root_node = ts_tree_root_node(tree);
                        break;
                    }
                } break;
                case MODE_LEAP_1: {
                    SDL_Keycode key = event.key.keysym.sym;
                    SDL_Keymod mod = event.key.keysym.mod;

                    if (key == SDLK_ESCAPE) {
                        mode = MODE_NORMAL;
                        leap_chars_count = 0;
                        break;
                    }

                    if ((key >= SDLK_a && key <= SDLK_z) ||
                        (key >= SDLK_0 && key <= SDLK_9) ||
                        key == SDLK_LEFTBRACKET ||
                        key == SDLK_RIGHTBRACKET ||
                        key == SDLK_SEMICOLON ||
                        ((mod & KMOD_SHIFT) && ((key == SDLK_9) || (key == SDLK_0) || (key == SDLK_LEFTBRACKET) || (key == SDLK_RIGHTBRACKET)))) {

                        if (mod & KMOD_SHIFT) {
                            switch (key) {
                                case SDLK_9:
                                    leap_chars[0] = '(';
                                    break;
                                case SDLK_0:
                                    leap_chars[0] = ')';
                                    break;
                                case SDLK_LEFTBRACKET:
                                    leap_chars[0] = '{';
                                    break;
                                case SDLK_RIGHTBRACKET:
                                    leap_chars[0] = '}';
                                    break;
                                case SDLK_SEMICOLON:
                                    leap_chars[0] = ':';
                                    break;
                            }
                        } else {
                            leap_chars[0] = key;
                        }

                        leap_chars_count = 1;
                        mode = MODE_LEAP_2;
                        assign_leap_labels(leap_chars[0]);
                        break;
                    }
                } break;
                case MODE_LEAP_2: {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        mode = MODE_NORMAL;
                        leap_chars_count = 0;
                        break;
                    }
                    SDL_Keycode key = event.key.keysym.sym;
                    SDL_Keymod mod = event.key.keysym.mod;
                    if ((key >= SDLK_a && key <= SDLK_z) ||
                        (key >= SDLK_0 && key <= SDLK_9) ||
                        key == SDLK_LEFTBRACKET ||
                        key == SDLK_RIGHTBRACKET ||
                        key == SDLK_SEMICOLON ||
                        ((mod & KMOD_SHIFT) && ((key == SDLK_9) || (key == SDLK_0) || (key == SDLK_LEFTBRACKET) || (key == SDLK_RIGHTBRACKET)))) {

                        if (mod & KMOD_SHIFT) {
                            switch (key) {
                                case SDLK_9:
                                    leap_chars[1] = '(';
                                    break;
                                case SDLK_0:
                                    leap_chars[1] = ')';
                                    break;
                                case SDLK_LEFTBRACKET:
                                    leap_chars[1] = '{';
                                    break;
                                case SDLK_RIGHTBRACKET:
                                    leap_chars[1] = '}';
                                    break;
                                case SDLK_SEMICOLON:
                                    leap_chars[1] = ':';
                                    break;
                                default:
                                    if (key >= SDLK_a && key <= SDLK_z) {
                                        leap_chars[1] = toupper(key);
                                    }
                                    break;
                            }
                        } else {
                            leap_chars[1] = key;
                        }

                        leap_chars_count = 2;
                        leap();
                        break;
                    }
                } break;
                }
            } break;
            }

            draw_frame();
        }
    }

    return 0;
}
