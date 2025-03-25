#include <stdarg.h>
#include <stdio.h>

#include "LocalVarArray.h"
#include "memory.h"

void initLocalVarArray(LocalVarArray* array)
{
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void writeLocalVarArray(LocalVarArray* array, LocalVar local)
{
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(LocalVar, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = local;
    array->count++;
}

void freeLocalVarArray(LocalVarArray* array)
{
    FREE_ARRAY(LocalVar, array->values, array->capacity);
    initLocalVarArray(array);
}

void printVarArray(LocalVarArray* array, const char* title, ...)
{
    printf("==== ");
    va_list args;
    va_start(args, title);
    vfprintf(stdout, title, args);
    va_end(args);
    printf("[%d/%d] ====\n", array->count, array->capacity);

    for (unsigned int i = 0; i < array->count; i++) {
        LocalVar* var = &array->values[i];
        printf("%02d - {identifier: %s, shadows: %d, depth: %d, %s}\n", i, var->identifier->chars,
            var->shadowAddr, var->depth, var->readonly ? "readonly" : "read-write");
    }

    printf("============\n");
}
