#include "common.h"
#include "chunk.h"
#include "util/debug.h"

int main(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;

    Chunk chunk;
    initChunk(&chunk);
    int constant1 = addConstant(&chunk, 1.2);
    int constant2 = addConstant(&chunk, 3.4);

    writeChunk(&chunk, OP_CONSTANT, 1);
    writeChunk(&chunk, constant1, 1);
    writeChunk(&chunk, OP_CONSTANT, 1);
    writeChunk(&chunk, constant2, 1);

    writeChunk(&chunk, OP_RETURN, 2);

    disassambleChunk(&chunk, "test chunk");

    freeChunk(&chunk);

    return 0;
}