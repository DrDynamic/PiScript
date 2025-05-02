#include <time.h>

#include "natives.h"
#include "values/value.h"
#include "compiler.h"
#include "util/memory.h"

static Value clockNative(int argCount, Value* args)
{
    (void)args;
    (void)argCount;
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}


static Value collectGarbageNative(int argCount, Value* args)
{
    (void)args;
    (void)argCount;

    collectGarbage();
    return NIL_VAL;
}

void defineNatives()
{
    defineNative("clock", clockNative);
    defineNative("collectGarbage", collectGarbageNative);
}