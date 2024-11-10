#include "common.h"
#include "chunk/chunk.h"
#include "util/debug.h"
#include "vm.h"

// playground includes
#include <stdio.h>

int main(int argc, const char* argv[])
{
    (void)argc;
    (void)argv;

    initVM();

    Chunk chunk;
    initChunk(&chunk);

    for (int i = 0; i < 0x1FF; i++) {
        //        writeConstant(&chunk, i, 1);
    }
    writeConstant(&chunk, 13.37, 1);
    writeChunk(&chunk, OP_NEGATE, 1);
    writeChunk(&chunk, OP_RETURN, 2);

    //    disassambleChunk(&chunk, "test chunk");

    interpret(&chunk);

    freeVM();
    freeChunk(&chunk);

    return 0;
}