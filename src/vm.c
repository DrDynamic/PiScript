#include <stdio.h>

#include "common.h"
#include "vm.h"
#include "compiler.h"
#include "debug.h"

VM vm;

static void resetStack()
{
    vm.stackTop = vm.stack;
}

static void runtimeError(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = getSourceInfoLinenumber(&vm.chunk->sourceinfo, instruction);
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}

void initVM()
{
    resetStack();
}

void freeVM()
{
}

static InterpretResult run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op)                                                                              \
    do {                                                                                           \
        double b = pop();                                                                          \
        double a = pop();                                                                          \
        push(a op b);                                                                              \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[");
            printValue(*slot);
            printf("]");
        }
        printf("\n");
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
        case OP_ADD:
            BINARY_OP(+);
            break;
        case OP_SUBTRACT:
            BINARY_OP(-);
            break;
        case OP_MULTIPLY:
            BINARY_OP(*);
            break;
        case OP_DIVIDE:
            BINARY_OP(/);
            break;
        case OP_NEGATE:
            if (!IS_NUMBER(peek(0))) {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;
        case OP_RETURN: {
            printValue(pop());
            printf("\n");
            return INTERPRET_OK;
        }
        case OP_CONSTANT: {
            Value constant = READ_CONSTANT();
            push(constant);
            break;
        }
        case OP_CONSTANT_LONG: {
            uint8_t addr1 = READ_BYTE();
            uint8_t addr2 = READ_BYTE();
            uint8_t addr3 = READ_BYTE();
            int constantAddr = (addr1 << 16) | (addr2 << 8) | addr3;
            Value constant = vm.chunk->constants.values[constantAddr];
            push(constant);
            break;
        }
        default:
            printf("undefined instruction: 0x%02X\n", instruction);
            return INTERPRET_RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(const char* source)
{
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}

void push(Value value)
{
    // TODO: check for stackoverflow
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop()
{
    // TODO: chack for stack underflow?
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance)
{
    return vm.stackTop[-1 - distance];
}