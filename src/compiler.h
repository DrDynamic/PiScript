
#pragma once

#include "values/object.h"
#include "vm.h"

void defineNative(const char* name, NativeFn function);
ObjFunction* compile(const char* source);
void markCompilerRoots();
