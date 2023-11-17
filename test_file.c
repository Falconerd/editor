#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MB (1024 * 1024)

typedef unsigned char u8;
typedef size_t usize;

typedef struct {
    u8 *data;
    usize len;
} s8;

typedef struct {
    usize lines;
    usize words;
    usize characters;
} file_stats;

file_stats analyze_file(const s8 content) {
    file_stats stats = {0, 0, 0};

    usize i = 0;
    u8 in_word = 0;
    while (i < content.len) {
        stats.characters++;
        if (content.data[i] == '\n') {
            stats.lines++;
            in_word = 0;
        } else if (content.data[i] == ' ' || content.data[i] == '\t') {
            in_word = 0;
        } else if (!in_word) {
            stats.words++;
            in_word = 1;
        }
        i++;
    }
    return stats;
}

s8 read_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    u8 *buffer = (u8 *)malloc(size);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    fread(buffer, 1, size, file);
    fclose(file);

    return (s8){buffer, (usize)size};
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    s8 file_content = read_file(argv[1]);
    file_stats stats = analyze_file(file_content);

    printf("Lines: %zu\n", stats.lines);
    printf("Words: %zu\n", stats.words);
    printf("Characters: %zu\n", stats.characters);

    free(file_content.data);
    return 0;
}
