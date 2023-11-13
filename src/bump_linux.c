#include <sys/mman.h>
#include <unistd.h>
#include "common.h"

#ifndef BUMP_DEFAULT_SIZE
#define BUMP_DEFAULT_SIZE (1 * GB)
#endif

b32 bump_init_sized(bump *b, int size) {
    b->buf = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (b->buf == MAP_FAILED) {
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
    size_t pagesize = sysconf(_SC_PAGESIZE);

    // Check if there's enough space left in the total reserved memory
    if (b->offset + size > b->size) {
        return 0;
    }

    // Calculate the end address of the current allocation request
    char* end_address = b->buf + b->offset + size;

    // Calculate the start address of the page where the current offset resides
    char* page_start_address = b->buf + (b->offset - (b->offset % pagesize));

    // Try to commit memory using mprotect to make it accessible
    if (mprotect(page_start_address, end_address - page_start_address, PROT_READ | PROT_WRITE) == -1) {
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
