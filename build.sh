#!/bin/bash

COMPILER_FLAGS="-g -I./third_party/include -I./third_party/src"
LINKER_FLAGS="-L./third_party/lib -lSDL2main -lSDL2 -lm"

mkdir -p ./build

gcc $COMPILER_FLAGS src/main.c third_party/src/gl.c third_party/src/lib.c third_party/tree-sitter-c/src/parser.c -o ./build/editor $LINKER_FLAGS

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

echo "Compilation successful!"
