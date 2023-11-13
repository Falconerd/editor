#include "common.h"

// Remember, VirtualAlloc already zeroes memory.
// No need to zero it again ourselves.
// This is for other scenarios.
void mem_zero(void *mem, int size) {
    byte *ptr = mem;
    int mid = size / 2;
    for (int i = 0; i < mid; i += 1) {
        ptr[i] = 0;
        ptr[size - i - 1] = 0;
    }
}

// TODO: Optimise this?
// NOTE: Test it first.
void mem_copy(void *dest, void *src, int size) {
    byte *d = dest;
    byte *s = src;
    for (int i = 0; i < size; i += 1) {
        d[i] = s[i];
    }
}
