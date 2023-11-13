#include "common.h"
#include <sys/mman.h>
#include <unistd.h>

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
        PAGE_SIZE = sysconf(_SC_PAGESIZE);
    }
    a->base = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (a->base == MAP_FAILED) {
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

byte *arena_alloc_align(arena *a, int bytes, int alignment) {
    uptr curr_ptr = (uptr)a->base + (uptr)a->offset;
    uptr offset = align_forward(curr_ptr, alignment);
    offset -= (uptr)a->base;

    if (offset + bytes > a->size) {
        return 0;
    }

    int size = PAGE_SIZE * ((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
    if (a->committed < offset + bytes) {
        void *try = mmap(a->base + a->committed, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (try == MAP_FAILED) {
            return 0;
        }
        a->committed += size;
    }

    byte *ptr = a->base + offset;
    a->offset = offset + bytes;

    return ptr;
}

byte *arena_alloc(arena *a, int bytes) {
    return arena_alloc_align(a, bytes, DEFAULT_ALIGNMENT);
}

void arena_free_all(arena *a) {
    a->offset = 0;
    a->committed = 0;
}

