#pragma once

#include "../chunk/chunk.h"

void disassambleChunk(Chunk *chunk, const char *name);
int disassambleInstruction(Chunk *chunk, int offset);