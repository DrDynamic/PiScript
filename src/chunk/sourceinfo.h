#pragma once

#include "../common.h"
#include "chunkDefs.h"

typedef struct {
    BytecodeIndex capacity;
    BytecodeIndex count;

    Linenumber* linenumbers; // lines per file
    uint8_t* linenumberCounter; // chunk bytes per line
} SourceInfo;

void initSourceInfo(SourceInfo* info);
void freeSourceInfo(SourceInfo* info);

void addLinenumer(SourceInfo* info, Linenumber linenumber);
Linenumber getSourceInfoLinenumber(SourceInfo* info, BytecodeIndex offset);
