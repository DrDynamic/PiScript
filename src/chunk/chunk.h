#pragma once

#include "../common.h"
#include "chunkDefs.h"
#include "../value.h"
#include "sourceinfo.h"

typedef enum {
    OP_CONSTANT,
    OP_RETURN,
} OpCode;

typedef struct {
    BytecodeIndex count;
    BytecodeIndex capacity;
    uint8_t* code;
    ValueArray constants;
    SourceInfo sourceinfo;
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);
Linenumber getLinenumber(Chunk* chunk, BytecodeIndex offset);
void freeChunk(Chunk* chunk);
