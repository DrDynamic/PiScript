#include <stdio.h>

#include "debug.h"
#include "value.h"
#include "object.h"

void disassembleChunk(Chunk* chunk, const char* name)
{
    printf("== %s ==\n", name);

    for (BytecodeIndex offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
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
static int constantLongInstruction(const char* name, Chunk* chunk, int offset)
{
    uint8_t idx1 = chunk->code[offset + 1];
    uint8_t idx2 = chunk->code[offset + 2];
    uint8_t idx3 = chunk->code[offset + 3];

    uint32_t constantIndex = (idx1 << 16) | (idx2 << 8) | idx3;

    printf("%-16s %4d '", name, constantIndex);
    printValue(chunk->constants.values[constantIndex]);
    printf("'\n");

    return offset + 4;
}

static int simpleInstruction(const char* name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset)
{
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int uint24Instruction(const char* name, Chunk* chunk, int offset)
{
    uint8_t idx1 = chunk->code[offset + 1];
    uint8_t idx2 = chunk->code[offset + 2];
    uint8_t idx3 = chunk->code[offset + 3];

    uint32_t constantIndex = (idx1 << 16) | (idx2 << 8) | idx3;

    printf("%-16s %4d\n", name, constantIndex);

    return offset + 4;
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset)
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static int closureInstruction(const char* name, uint32_t constantIndex, Chunk* chunk, int offset)
{
    printf("%-16s %4d ", name, constantIndex);
    printValue(chunk->constants.values[constantIndex]);
    printf("\n");

    const ObjFunction* function = AS_FUNCTION(chunk->constants.values[constantIndex]);
    for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04d      |                     %s %d\n", offset - 2, isLocal ? "local" : "upvalue",
            index);
    }
    return offset;
}

int disassembleInstruction(Chunk* chunk, int offset)
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
    case OP_ADD:
        return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
        return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
        return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
        return simpleInstruction("OP_DIVIDE", offset);
    case OP_NOT:
        return simpleInstruction("OP_NOT", offset);
    case OP_NEGATE:
        return simpleInstruction("OP_NEGATE", offset);
    case OP_JUMP:
        return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_JUMP_IF_FALSE:
        return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
    case OP_LOOP:
        return jumpInstruction("OP_LOOP", -1, chunk, offset);
    case OP_CALL:
        return byteInstruction("OP_CALL", chunk, offset);
    case OP_CLOSURE: {
        offset++;
        uint8_t constant = chunk->code[offset++];

        offset = closureInstruction("OP_CLOSURE", constant, chunk, offset);
        return offset;
    }
    case OP_CLOSURE_LONG: {
        offset++;
        uint8_t idx1 = chunk->code[offset++];
        uint8_t idx2 = chunk->code[offset++];
        uint8_t idx3 = chunk->code[offset++];
        uint32_t constantIndex = (idx1 << 16) | (idx2 << 8) | idx3;

        offset = closureInstruction("OP_CLOSURE_LONG", constantIndex, chunk, offset);
        return offset;
    }
    case OP_CLOSE_UPVALUE:
        return simpleInstruction("OP_CLOSE_UPVALUE", offset);
    case OP_RETURN:
        return simpleInstruction("OP_RETURN", offset);
    case OP_CONSTANT:
        return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_CONSTANT_LONG:
        return constantLongInstruction("OP_CONSTANT_LONG", chunk, offset);
    case OP_NIL:
        return simpleInstruction("OP_NIL", offset);
    case OP_TRUE:
        return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE:
        return simpleInstruction("OP_FALSE", offset);
    case OP_POP:
        return simpleInstruction("OP_POP", offset);
    case OP_GET_LOCAL:
        return byteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_GET_LOCAL_LONG:
        return uint24Instruction("OP_GET_LOCAL_LONG", chunk, offset);
    case OP_SET_LOCAL:
        return byteInstruction("OP_SET_LOCAL", chunk, offset);
    case OP_SET_LOCAL_LONG:
        return uint24Instruction("OP_SET_LOCAL_LONG", chunk, offset);
    case OP_GET_GLOBAL:
        return byteInstruction("OP_GET_GLOBAL", chunk, offset);
    case OP_GET_GLOBAL_LONG:
        return uint24Instruction("OP_GET_GLOBAL_LONG", chunk, offset);
    case OP_DEFINE_GLOBAL:
        return byteInstruction("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_DEFINE_GLOBAL_LONG:
        return uint24Instruction("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL:
        return byteInstruction("OP_SET_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL_LONG:
        return uint24Instruction("OP_SET_GLOBAL_LONG", chunk, offset);
    case OP_GET_UPVALUE:
        return byteInstruction("OP_GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE:
        return byteInstruction("OP_SET_UPVALUE", chunk, offset);
    case OP_EQUAL:
        return simpleInstruction("OP_EQUAL", offset);
    case OP_NOT_EQUAL:
        return simpleInstruction("OP_NOT_EQUAL", offset);
    case OP_GREATER:
        return simpleInstruction("OP_GREATER", offset);
    case OP_GREATER_EQUAL:
        return simpleInstruction("OP_GREATER_EQUAL", offset);
    case OP_LESS:
        return simpleInstruction("OP_LESS", offset);
    case OP_LESS_EQUAL:
        return simpleInstruction("OP_LESS_EQUAL", offset);
    case OP_PRINT:
        return simpleInstruction("OP_PRINT", offset);
    case OP_UNDEFINED:
        return simpleInstruction("OP_UNDEFINED", offset);
    default:
        printf("unknown opcode: 0x%02X\n", instruction);
        return offset + 1;
    }
}
