#pragma once

#include "../common.h"
#include "object.h"


typedef struct {
    int depth;
    ObjString* identifier;
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
