#include <stdio.h>
#include <stdlib.h>
#include "common.h"

file_read_result os_file_read(const char *path, arena *a, arena *scratch) {
    file_read_result r = {0};

    FILE *f = fopen(path, "rb");
    if (!f) {
        return r;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        goto close_ret;
    }

    usize size = (usize)ftell(f);
    if (size < 0) {
        goto close_ret;
    }
    rewind(f);

    u8 *src_mem = arena_alloc(scratch, size);
    if (!src_mem) {
        goto close_ret;
    }

    usize len = fread(src_mem, 1, size, f);
    if (len != size) {
        goto close_ret;
    }

    usize final_len = 0;
    for (usize i = 0; i < len; i += 1) {
        if (src_mem[i] == '\r') {
            if (i + 1 < len && src_mem[i + 1] == '\n') {
                i += 1;
            }
            final_len += 1;
        } else {
            final_len += 1;
        }
    }

    u8 *dest_mem = arena_alloc(a, final_len);
    if (!dest_mem) {
        goto close_ret;
    }

    usize j = 0;
    for (usize i = 0; i < len; i += 1) {
        if (src_mem[i] == '\r') {
            if (i + 1 < len && src_mem[i + 1] == '\n') {
                i += 1;
            }
            dest_mem[j] = '\n';
            j += 1;
        } else {
            dest_mem[j] = src_mem[i];
            j += 1;
        }
    }


    fclose(f);

    r.str.data = dest_mem;
    r.str.len = final_len;
    r.ok = 1;
    return r;
    
close_ret:
    fclose(f);
    return r;
}
