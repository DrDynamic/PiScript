#include <stdlib.h>
#include <stdio.h>

#include "chunk.h"
#include "util/memory.h"

void initChunk(Chunk* chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;

    initSourceInfo(&chunk->sourceinfo);
    initValueArray(&chunk->constants);
}


void writeChunk(Chunk* chunk, uint8_t byte, int line)
{
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }
    addLinenumer(&chunk->sourceinfo, line);
    chunk->code[chunk->count] = byte;
    chunk->count++;
}

void writeConstant(Chunk* chunk, Value value, int line, OpCode opCodeShort, OpCode opCodeLong)
{
    uint32_t index = addConstant(chunk, value);
    if (index > 0xFF) {
        uint8_t idx1 = index & 0xFF;
        uint8_t idx2 = (index >> 8) & 0xFF;
        uint8_t idx3 = (index >> 16) & 0xFF;

        writeChunk(chunk, opCodeLong, line);
        writeChunk(chunk, idx3, line);
        writeChunk(chunk, idx2, line);
        writeChunk(chunk, idx1, line);
    } else {
        writeChunk(chunk, opCodeShort, line);
        writeChunk(chunk, index, line);
    }
}

uint32_t addConstant(Chunk* chunk, Value value)
{
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

Linenumber getLinenumber(Chunk* chunk, BytecodeIndex offset)
{
    return getSourceInfoLinenumber(&chunk->sourceinfo, offset);
}

void freeChunk(Chunk* chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);

    freeSourceInfo(&chunk->sourceinfo);
    freeValueArray(&chunk->constants);

    initChunk(chunk);
}