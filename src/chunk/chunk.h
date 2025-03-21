#pragma once

#include "../common.h"
#include "chunkDefs.h"
#include "../value.h"
#include "sourceinfo.h"

typedef enum {
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_GLOBAL,
    OP_GET_GLOBAL_LONG,
    OP_DEFINE_GLOBAL,
    OP_DEFINE_GLOBAL_LONG,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
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
void writeConstant(Chunk* chunk, Value value, int line, OpCode opCodeShort, OpCode opCodeLong);
uint32_t addConstant(Chunk* chunk, Value value);
Linenumber getLinenumber(Chunk* chunk, BytecodeIndex offset);
void freeChunk(Chunk* chunk);
