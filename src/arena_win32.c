#include "common.h"

#ifndef ARENA_DEFAULT_SIZE
#define ARENA_DEFAULT_SIZE (1 * GB)
#endif

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#endif

static int PAGE_SIZE = 0;

b32 is_power_of_two(uptr x) {
    return x != 0 && (x & (x - 1)) == 0;
}

uptr align_forward(uptr ptr, int alignment) {
    uptr p, a, modulo;
    if (!is_power_of_two(alignment)) {
        return 0;
    }

    p = ptr;
    a = (uptr)alignment;

    modulo = p & (a - 1);

    if (modulo) {
        p += a - modulo;
    }

    return p;
}

b32 arena_init_sized(arena *a, int size) {
    if (!PAGE_SIZE) {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        PAGE_SIZE = info.dwPageSize;
    }
    a->base = VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
    if (!a->base) {
        return 0;
    }
    a->size = size;
    a->committed = 0;
    a->offset = 0;
    return 1;
}

b32 arena_init(arena *a) {
    return arena_init_sized(a, ARENA_DEFAULT_SIZE);
}

void *arena_alloc_align(arena *a, int bytes, int alignment) {
    uptr curr_ptr = (uptr)a->base + (uptr)a->offset;
    uptr offset = align_forward(curr_ptr, alignment);
    offset -= (uptr)a->base;

    if (offset + bytes > a->size) {
        return 0;
    }

    int size = PAGE_SIZE * (bytes / PAGE_SIZE + 1);
    byte *try = VirtualAlloc(a->base + a->committed, size, MEM_COMMIT, PAGE_READWRITE);
    if (!try) {
        return 0;
    }
    a->committed += size;
    byte *ptr = a->base + offset;
    a->offset = offset + bytes;

    return ptr;
}

void *arena_alloc(arena *a, int bytes) {
    return arena_alloc_align(a, bytes, DEFAULT_ALIGNMENT);
}

void arena_free_all(arena *a) {
    a->offset = 0;
    a->committed = 0;
}