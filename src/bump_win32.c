#include "common.h"

#ifndef BUMP_DEFAULT_SIZE
#define BUMP_DEFAULT_SIZE (1 * GB)
#endif

b32 bump_init_sized(bump *b, int size) {
    b->buf = VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
    if (!b->buf) {
        return 0;
    }
    b->size = size;
    b->offset = 0;
    return 1;
}

b32 bump_init(bump *b) {
    return bump_init_sized(b, BUMP_DEFAULT_SIZE);
}

void *bump_alloc(bump *b, int size) {
    void *try = VirtualAlloc(b->buf + b->offset, size, MEM_COMMIT, PAGE_READWRITE);
    if (!try) {
        return 0;
    }
    int offset = b->offset;
    b->offset += size;
    return b->buf + offset;
}

b32 bump_free(bump *b, int size) {
    if (b->offset - size < 0) {
        return 0;
    }
    b->offset -= size;
    return 1;
}

void bump_free_all(bump *b) {
    b->offset = 0;
}
