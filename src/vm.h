#pragma once

#include "values/object.h"
#include "table.h"
#include "values/value.h"
#include "util/VarArray.h"
#include "util/addresstable.h"

#define TEMPS_MAX 16
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct sVM {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Value temps[TEMPS_MAX];
    int tempsCount;

    Value stack[STACK_MAX];
    Value* stackTop;
    ValueArray globals;

    Table strings;
    ObjUpvalue* openUpvalues;
    Obj* objects;

    ObjString* initString;

    // used by compiler
    AddressTable gloablsTable;

    // garbage collection
    size_t bytesAllocated;
    size_t nextGC;

    int grayCount;
    int grayCapacity;
    Obj** grayStack;
} VM;


typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpretFile(const char* path, const char* source);
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();