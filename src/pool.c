#include "common.h"

// NOTE: For now just do a simple linear search.
// It's dumb but we can change it later.

typedef struct pool {
    usize size;
    usize cap;
    void *buf;
    b32 *used;
} pool;

b32 pool_init(pool *p, arena *a, usize size, usize cap) {
    byte *buf = arena_alloc(a, size * cap);
    byte *used = arena_alloc(a, 32 * cap);
    // TODO: Figure out if this is necessary.
    mem_zero(used, 32 * cap);
    if (!buf || !used) {
        return 0;
    }
    p->buf = buf;
    p->used = (b32 *)used;
    p->size = size;
    p->cap = cap;
    return 1;
}

// Retrieves the next available element and marks it as used.
void *pool_next(pool *p) {
    for (usize i = 0; i < p->cap; i += 1) {
        if (!p->used[i]) {
            p->used[i] = 1;
            byte *ptr = p->buf;
            return ptr + (i * p->size);
        }
    }
    return 0;
}

void pool_free(pool *p, usize i) {
    p->used[i] = 0;
}