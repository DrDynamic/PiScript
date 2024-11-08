#include <stdio.h>

#include "debug.h"
#include "value.h"

void disassambleChunk(Chunk* chunk, const char* name)
{
    printf("== %s ==\n", name);

    for (BytecodeIndex offset = 0; offset < chunk->count;) {
        offset = disassambleInstruction(chunk, offset);
    }
}

static int constantInstruction(const char* name, Chunk* chunk, int offset)
{
    uint8_t constantIndex = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constantIndex);
    printValue(chunk->constants.values[constantIndex]);
    printf("'\n");

    return offset + 2;
}

static int simpleInstruction(const char* name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

int disassambleInstruction(Chunk* chunk, int offset)
{
    printf("[%04d] ", offset);

    // TODO: optimize - get linenumber could be quieried one time less
    if (offset > 0 && getLinenumber(chunk, offset) == getLinenumber(chunk, offset - 1)) {
        printf("%-4s| ", "");
    } else {
        printf("% 4d: ", getLinenumber(chunk, offset));
    }

    uint8_t instruction = chunk->code[offset];

    switch (instruction) {
    case OP_CONSTANT:
        return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_RETURN:
        return simpleInstruction("OP_RETURN", offset);
    default:
        printf("unknown opcode: 0x%02X\n", instruction);
        return offset + 1;
    }
}
