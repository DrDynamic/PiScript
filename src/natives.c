#include <time.h>

#include "natives.h"
#include "value.h"
#include "compiler.h"

static Value clockNative(int argCount, Value* args)
{
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}


void defineNatives()
{
    defineNative("clock", clockNative);
}