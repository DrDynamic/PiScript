#pragma once

#include "../common.h"
#include "object.h"


typedef struct {
    int depth;
    ObjString* identifier;
    bool readonly;
    int shadowAddr;
} LocalVar;

typedef struct {
    unsigned int capacity;
    unsigned int count;
    LocalVar* values;
} LocalVarArray;

void initLocalVarArray(LocalVarArray* array);
void writeLocalVarArray(LocalVarArray* array, LocalVar local);
void freeLocalVarArray(LocalVarArray* array);

void printVarArray(LocalVarArray* array, const char* title, ...);