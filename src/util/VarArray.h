#pragma once

#include "../common.h"
#include "../object.h"


typedef struct {
    int depth;
    ObjString* identifier;
    bool readonly;
    int shadowAddr;
    bool isCaptured;
} Var;

typedef struct {
    unsigned int capacity;
    unsigned int count;
    Var* values;
} VarArray;

void initVarArray(VarArray* array);
void writeVarArray(VarArray* array, Var local);
void freeVarArray(VarArray* array);

void printVarArray(VarArray* array, const char* title, ...);
void markVarArray(VarArray* array);