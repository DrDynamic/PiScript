#include "common.h"
#include "chunk/chunk.h"
#include "util/debug.h"
#include "chunk/sourceinfo.h"

#include <stdio.h>

int main(int argc, const char* argv[])
{
    (void)argc;
    (void)argv;

    Chunk chunk;
    initChunk(&chunk);

    for (int i = 0; i < 0x1FF; i++) {
        writeConstant(&chunk, i, 1);
    }

    writeChunk(&chunk, OP_RETURN, 2);

    disassambleChunk(&chunk, "test chunk");

    freeChunk(&chunk);

    return 0;
}