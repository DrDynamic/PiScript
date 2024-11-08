#include "common.h"
#include "chunk.h"
#include "util/debug.h"

int main(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;

    Chunk chunk;
    initChunk(&chunk);
    int constant = addConstant(&chunk, 1.2);

    writeChunk(&chunk, OP_CONSTANT);
    writeChunk(&chunk, constant);

    writeChunk(&chunk, OP_RETURN);

    disassambleChunk(&chunk, "test chunk");

    freeChunk(&chunk);

    return 0;
}