#include <stdio.h>

#include "debug.h"
#include "value.h"

void disassembleChunk(Chunk *chunk, const char *name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->len;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int constInst(const char *name, Chunk *chunk, int offset) {
    uint8_t i = chunk->code[offset + 1];
    printf("%-16s %4d '", name, i);
    printValue(chunk->constants.values[i]);
    printf("'\n");

    return offset + 2;
}
static int simpleInst(const char *name, int offset) {
    puts(name);
    return offset + 1;
}

int disassembleInstruction(Chunk *chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && (chunk->lines[offset] == chunk->lines[offset - 1])) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t inst = chunk->code[offset];
    switch (inst) {
    case OP_CONSTANT:
        return constInst("OP_CONSTANT", chunk, offset);
    case OP_NEGATE:
        return simpleInst("OP_NEGATE", offset);
    case OP_RETURN:
        return simpleInst("OP_RETURN", offset);
    default:
        printf("Unknown opcode %d\n", inst);
        return offset + 1;
    }
}
