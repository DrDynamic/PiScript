#include <stdarg.h>
#include <stdio.h>

#include "VarArray.h"
#include "memory.h"

void initVarArray(VarArray* array)
{
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void writeVarArray(VarArray* array, Var local)
{
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Var, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = local;
    array->count++;
}

void freeVarArray(VarArray* array)
{
    FREE_ARRAY(Var, array->values, array->capacity);
    initVarArray(array);
}

void printVarArray(VarArray* array, const char* title, ...)
{
    printf("==== ");
    va_list args;
    va_start(args, title);
    vfprintf(stdout, title, args);
    va_end(args);
    printf("[%d/%d] ====\n", array->count, array->capacity);

    for (unsigned int i = 0; i < array->count; i++) {
        Var* var = &array->values[i];
        printf("%02d - {identifier: %s, shadows: %d, depth: %d, %s}\n", i, var->identifier->chars,
            var->shadowAddr, var->depth, var->readonly ? "readonly" : "read-write");
    }

    printf("============\n");
}
void markVarArray(VarArray* array)
{
    for (unsigned int i = 0; i < array->count; i++) {
        markObject((Obj*)array->values[i].identifier);
    }
}