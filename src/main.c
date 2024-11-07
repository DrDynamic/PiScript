#include "common.h"
#include "chunk.h"
#include "util/debug.h"

int main(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;

    Chunk chunk;
    initChunk(&chunk);
    writeChunk(&chunk, OP_RETURN);
    writeChunk(&chunk, OP_RETURN);
    writeChunk(&chunk, OP_RETURN);
    writeChunk(&chunk, OP_RETURN);
    writeChunk(&chunk, 0x42);

    disassambleChunk(&chunk, "test chunk");

    freeChunk(&chunk);

    return 0;
}