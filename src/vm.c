#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "vm.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "natives.h"

VM vm;

static void resetStack()
{
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        const ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - frame->closure->function->chunk.code - 1;
        int line
            = getSourceInfoLinenumber(&frame->closure->function->chunk.sourceinfo, instruction);
        fprintf(stderr, "[line %d] in ", line);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

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

static bool call(ObjClosure* closure, int argCount)
{
    if (argCount != closure->function->arity) {
        runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
        return false;
    }
    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }
    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount)
{
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
        case OBJ_CLOSURE:
            return call(AS_CLOSURE(callee), argCount);
        case OBJ_NATIVE: {
            NativeFn native = AS_NATIVE(callee);
            Value result = native(argCount, vm.stackTop - argCount);
            vm.stackTop -= argCount + 1;
            push(result);
            return true;
        }

        default:
            break;
        }
    }
    runtimeError("Can only call functions and classes.");
    return false;
}

static ObjUpvalue* captureUpvalue(Value* local)
{
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;
    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }
    return createdUpvalue;
}

static void closeUpvalues(Value* last)
{
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

static bool isFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatinate()
{
    const ObjString* b = AS_STRING(pop());
    const ObjString* a = AS_STRING(pop());

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

    initTable(&vm.globalAddresses);
    initVarArray(&vm.globalProps);
    vm.globalCount = 0;

    defineNatives();
}

void freeVM()
{
    freeValueArray(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();

    freeTable(&vm.globalAddresses);
    freeVarArray(&vm.globalProps);
    vm.globalCount = 0;
}

static inline bool checkGlobalDefined(uint32_t addr)
{
    return addr >= vm.globals.count
        || (IS_OBJ(vm.globals.values[addr]) && vm.globals.values[addr].as.obj == NULL);
}

static InterpretResult run()
{
    CallFrame* frame = &vm.frames[vm.frameCount - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_UINT16() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_UINT24()                                                                              \
    (frame->ip += 3, (uint32_t)((frame->ip[-3] << 16) | (frame->ip[-2] << 8) | frame->ip[-1]))
#define GET_CONSTANT(addr) (frame->closure->function->chunk.constants.values[addr])
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
        disassembleInstruction(&frame->closure->function->chunk,
            (int)(frame->ip - frame->closure->function->chunk.code));
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
            frame->ip += offset;
            break;
        }
        case OP_JUMP_IF_FALSE: {
            uint16_t offset = READ_UINT16();
            if (isFalsey(peek(0))) {
                frame->ip += offset;
            }
            break;
        }
        case OP_LOOP: {
            uint16_t offset = READ_UINT16();
            frame->ip -= offset;
            break;
        }
        case OP_CALL: {
            int argCount = READ_BYTE();
            if (!callValue(peek(argCount), argCount)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        case OP_CLOSURE: {
            uint8_t addr = READ_BYTE();
            Value constant = GET_CONSTANT(addr);
            ObjFunction* function = AS_FUNCTION(constant);
            ObjClosure* closure = newClosure(function);
            push(OBJ_VAL(closure));
            for (int i = 0; i < closure->upvalueCount; i++) {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();
                if (isLocal) {
                    closure->upvalues[i] = captureUpvalue(frame->slots + index);
                } else {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }
            break;
        }
        case OP_CLOSURE_LONG: {
            uint8_t addr = READ_UINT24();
            Value constant = GET_CONSTANT(addr);
            ObjFunction* function = AS_FUNCTION(constant);
            ObjClosure* closure = newClosure(function);
            push(OBJ_VAL(closure));
            break;
        }
        case OP_CLOSE_UPVALUE:
            closeUpvalues(vm.stackTop - 1);
            pop();
            break;
        case OP_RETURN: {
            Value result = pop();
            closeUpvalues(frame->slots);
            vm.frameCount--;
            if (vm.frameCount == 0) {
                pop();
                return INTERPRET_OK;
            }

            vm.stackTop = frame->slots;
            push(result);
            frame = &vm.frames[vm.frameCount - 1];
            break;
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
            push(frame->slots[slot]);
            break;
        }
        case OP_GET_LOCAL_LONG: {
            uint32_t slot = READ_UINT24();
            push(frame->slots[slot]);
            break;
        }
        case OP_SET_LOCAL: {
            uint32_t slot = READ_BYTE();
            frame->slots[slot] = peek(0);
            break;
        }
        case OP_SET_LOCAL_LONG: {
            uint32_t slot = READ_UINT24();
            frame->slots[slot] = peek(0);
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
        case OP_GET_UPVALUE: {
            uint8_t slot = READ_BYTE();
            push(*frame->closure->upvalues[slot]->location);
            break;
        }
        case OP_SET_UPVALUE: {
            uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = peek(0);
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
    ObjFunction* function = compile(source);
    if (function == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }

    push(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}