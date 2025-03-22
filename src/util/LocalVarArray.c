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
