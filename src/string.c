#include "common.h"

s8 string_clone_from(arena *a, char *s) {
    int len = lengthof(s);
    u8 *mem = arena_alloc(a, len);
    if (!mem) {
        return (s8){0};
    }
    mem_copy(mem, s, len);
    return (s8){mem, len};
}

usize string_line_count(s8 s) {
    usize c = 0;
    for (usize i = 0; i < s.len; i += 1) {
        if (s.data[i] == '\n') {
            c += 1;
        }
    }
    return c;
}

usize string_count_until(s8 s, u8 ch) {
    usize c = (usize)-1;
    for (usize i = 0; i < s.len; i += 1) {
        c += 1;
        if (s.data[i] == ch) {
            break;
        }
    }
    return c;
}

s8 string_case_insensitive_search(s8 s, s8 q) {
    if (q.len > s.len) {
        return (s8){0};
    }

    for (usize i = 0; i <= s.len - q.len; i += 1) {
        usize j;
        for (j = 0; j < q.len; j += 1) {
            if (tolower(s.data[i + j]) != tolower(q.data[j])) {
                break;
            }
        }
        if (j == q.len) {
            return (s8){&s.data[i], q.len};
        }
    }
    return (s8){0};
}
