#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "vm.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"

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

static bool isFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatinate()
{
    ObjString* b = AS_STRING(pop());
    ObjString* a = AS_STRING(pop());

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    push(OBJ_VAL(result));
}

void initVM()
{
    resetStack();
    vm.objects = NULL;
    initValueArray(&vm.globals);
    initTable(&vm.strings);
}

void freeVM()
{
    freeValueArray(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
}

static inline bool checkGlobalDefined(uint32_t addr)
{
    return addr >= vm.globals.count
        || (IS_OBJ(vm.globals.values[addr]) && vm.globals.values[addr].as.obj == NULL);
}

static InterpretResult run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_UINT16() (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))
#define READ_UINT24() (vm.ip += 3, (uint32_t)((vm.ip[-3] << 16) | (vm.ip[-2] << 8) | vm.ip[-1]))
#define GET_CONSTANT(addr) (vm.chunk->constants.values[addr])
#define GET_STRING(addr) AS_STRING(GET_CONSTANT(addr))
#define BINARY_OP(valueType, op)                                                                   \
    do {                                                                                           \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                                          \
            runtimeError("Operands must be numbers.");                                             \
            return INTERPRET_RUNTIME_ERROR;                                                        \
        }                                                                                          \
        double b = AS_NUMBER(pop());                                                               \
        double a = AS_NUMBER(pop());                                                               \
        push(valueType(a op b));                                                                   \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        if (vm.stack >= vm.stackTop) {
            printf("[]");
        }
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
            if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                concatinate();
            } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(a + b));
            } else {
                runtimeError("Operands must be two numbers or two strings.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        case OP_SUBTRACT:
            BINARY_OP(NUMBER_VAL, -);
            break;
        case OP_MULTIPLY:
            BINARY_OP(NUMBER_VAL, *);
            break;
        case OP_DIVIDE:
            BINARY_OP(NUMBER_VAL, /);
            break;
        case OP_NOT:
            push(BOOL_VAL(isFalsey(pop())));
            break;
        case OP_NEGATE:
            if (!IS_NUMBER(peek(0))) {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;
        case OP_PRINT: {
            printValue(pop());
            printf("\n");
            break;
        }
        case OP_JUMP: {
            uint16_t offset = READ_UINT16();
            vm.ip += offset;
            break;
        }
        case OP_JUMP_IF_FALSE: {
            uint16_t offset = READ_UINT16();
            if (isFalsey(peek(0))) {
                vm.ip += offset;
            }
            break;
        }
        case OP_LOOP: {
            uint16_t offset = READ_UINT16();
            vm.ip -= offset;
            break;
        }
        case OP_RETURN: {
            return INTERPRET_OK;
        }
        case OP_CONSTANT: {
            uint8_t addr = READ_BYTE();
            Value constant = GET_CONSTANT(addr);
            push(constant);
            break;
        }
        case OP_CONSTANT_LONG: {
            uint32_t addr = READ_UINT24();
            Value constant = GET_CONSTANT(addr);
            push(constant);
            break;
        }
        case OP_NIL:
            push(NIL_VAL);
            break;
        case OP_TRUE:
            push(BOOL_VAL(true));
            break;
        case OP_FALSE:
            push(BOOL_VAL(false));
            break;
        case OP_POP:
            pop();
            break;
        case OP_GET_LOCAL: {
            uint8_t slot = READ_BYTE();
            push(vm.stack[slot]);
            break;
        }
        case OP_GET_LOCAL_LONG: {
            uint32_t slot = READ_UINT24();
            push(vm.stack[slot]);
            break;
        }
        case OP_SET_LOCAL: {
            uint32_t slot = READ_BYTE();
            vm.stack[slot] = peek(0);
            break;
        }
        case OP_SET_LOCAL_LONG: {
            uint32_t slot = READ_UINT24();
            vm.stack[slot] = peek(0);
            break;
        }
        case OP_GET_GLOBAL: {
            uint32_t addr = READ_BYTE();
            if (checkGlobalDefined(addr)) {
                runtimeError("Undefined variable.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(vm.globals.values[addr]);
            break;
        }
        case OP_GET_GLOBAL_LONG: {
            uint32_t addr = READ_UINT24();
            if (checkGlobalDefined(addr)) {
                runtimeError("Undefined variable.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(vm.globals.values[addr]);
            break;
        }
        case OP_DEFINE_GLOBAL: {
            uint8_t addr = READ_BYTE();
            while (addr >= vm.globals.count) {
                writeValueArray(&vm.globals, OBJ_VAL(NULL));
            }
            vm.globals.values[addr] = peek(0);
            pop();
            break;
        }
        case OP_DEFINE_GLOBAL_LONG: {
            uint32_t addr = READ_UINT24();
            while (addr >= vm.globals.count) {
                writeValueArray(&vm.globals, OBJ_VAL(NULL));
            }
            vm.globals.values[addr] = peek(0);
            pop();
            break;
        }
        case OP_SET_GLOBAL: {
            uint8_t addr = READ_BYTE();
            if (checkGlobalDefined(addr)) {
                runtimeError("Undefined variable.");
                return INTERPRET_RUNTIME_ERROR;
            }
            vm.globals.values[addr] = peek(0);
            break;
        }
        case OP_SET_GLOBAL_LONG: {
            uint8_t addr = READ_UINT24();
            if (checkGlobalDefined(addr)) {
                runtimeError("Undefined variable.");
                return INTERPRET_RUNTIME_ERROR;
            }
            vm.globals.values[addr] = peek(0);
            break;
        }
        case OP_EQUAL: {
            Value a = pop();
            Value b = pop();
            push(BOOL_VAL(valuesEqual(a, b)));
            break;
        }
        case OP_NOT_EQUAL: {
            Value a = pop();
            Value b = pop();
            push(BOOL_VAL(!valuesEqual(a, b)));
            break;
        }
        case OP_GREATER:
            BINARY_OP(BOOL_VAL, >);
            break;
        case OP_GREATER_EQUAL:
            BINARY_OP(BOOL_VAL, >=);
            break;
        case OP_LESS:
            BINARY_OP(BOOL_VAL, <);
            break;
        case OP_LESS_EQUAL:
            BINARY_OP(BOOL_VAL, <=);
            break;
        default:
            printf("undefined instruction: 0x%02X\n", instruction);
            return INTERPRET_RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef READ_UINT16
#undef READ_UINT24
#undef GET_CONSTANT
#undef GET_STRING
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
