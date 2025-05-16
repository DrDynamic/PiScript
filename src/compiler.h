
#pragma once

#include "values/object.h"
#include "vm.h"

void defineNative(const char* name, NativeFn function);
ObjFunction* compile(const char* source);
ObjFunction* compileModule(ObjString* fqn, const char* source, ObjModule* caller);

void markModules();
void markCompilerRoots(Compiler* compiler);
