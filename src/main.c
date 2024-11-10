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
    writeConstant(&chunk, 13, 1);
    writeChunk(&chunk, OP_NEGATE, 1);
    writeConstant(&chunk, 26, 2);
    writeChunk(&chunk, OP_ADD, 2);
    writeConstant(&chunk, 11, 2);
    writeChunk(&chunk, OP_SUBTRACT, 2);
    writeConstant(&chunk, 5, 2);
    writeChunk(&chunk, OP_MULTIPLY, 2);
    writeConstant(&chunk, 4, 2);
    writeChunk(&chunk, OP_DIVIDE, 2);
    writeChunk(&chunk, OP_RETURN, 3);

    //    disassambleChunk(&chunk, "test chunk");

    interpret(&chunk);

    freeVM();
    freeChunk(&chunk);

    return 0;
}