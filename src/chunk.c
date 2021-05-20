#include <stdint.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "value.h"

void initChunk(Chunk *chunk) {
    chunk->len = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk *chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

void writeChunk(Chunk *chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->len + 1) {
        size_t old_cap = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_cap);
        chunk->code =
            GROW_ARRAY(uint8_t, chunk->code, old_cap, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, old_cap, chunk->capacity);
    }

    chunk->code[chunk->len] = byte;
    chunk->lines[chunk->len] = line;
    chunk->len++;
}

int addConstant(Chunk *chunk, Value val) {
    writeValueArray(&chunk->constants, val);
    return chunk->constants.len - 1;
}
