#include <stdio.h>

#include "common.h"
#include "vm.h"
#include "debug.h"

VM vm;

void initVM()
{
}

void freeVM()
{
}

static InterpretResult run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        disassambleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
        case OP_RETURN: {
            return INTERPRET_OK;
        }
        case OP_CONSTANT: {
            Value constant = READ_CONSTANT();
            printValue(constant);
            printf("\n");
            break;
        }
        case OP_CONSTANT_LONG: {
            uint8_t addr1 = READ_BYTE();
            uint8_t addr2 = READ_BYTE();
            uint8_t addr3 = READ_BYTE();
            int constantAddr = (addr1 << 16) | (addr2 << 8) | addr3;
            Value constant = vm.chunk->constants.values[constantAddr];
            printValue(constant);
            printf("\n");
            break;
        }
        default:
            printf("undefined instruction: 0x%02X\n", instruction);
            return INTERPRET_RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult interpret(Chunk* chunk)
{
    vm.chunk = chunk;
    vm.ip = chunk->code;

    return run();
}
