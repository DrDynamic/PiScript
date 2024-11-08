#include <stdlib.h>

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

int addConstant(Chunk* chunk, Value value)
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