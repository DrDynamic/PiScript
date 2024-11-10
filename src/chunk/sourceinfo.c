
#include "sourceinfo.h"
#include "../util/memory.h"


void initSourceInfo(SourceInfo* info)
{
    info->count = 0;
    info->capacity = 0;
    info->linenumbers = NULL;
    info->linenumberCounter = NULL;
}

static void writeLine(SourceInfo* info, Linenumber line)
{
    if (info->capacity < info->count + 1) {
        int oldCapacity = info->capacity;
        info->capacity = GROW_CAPACITY(oldCapacity);
        info->linenumbers = GROW_ARRAY(Linenumber, info->linenumbers, oldCapacity, info->capacity);
        info->linenumberCounter
            = GROW_ARRAY(uint32_t, info->linenumberCounter, oldCapacity, info->capacity);
    }

    info->linenumbers[info->count] = line;
    info->linenumberCounter[info->count] = 1;
    info->count++;
}

void freeSourceInfo(SourceInfo* info)
{
    FREE_ARRAY(Linenumber, info->linenumbers, info->capacity);
    FREE_ARRAY(uint32_t, info->linenumberCounter, info->capacity);

    initSourceInfo(info);
}

void addLinenumer(SourceInfo* info, Linenumber linenumber)
{
    if (info->linenumbers == NULL) {
        writeLine(info, linenumber);
        return;
    }

    Linenumber last = info->linenumbers[info->count - 1];
    if (last == linenumber) {
        info->linenumberCounter[info->count - 1] += 1;
    } else {
        writeLine(info, linenumber);
    }
}

Linenumber getSourceInfoLinenumber(SourceInfo* info, BytecodeIndex offset)
{
    int64_t lastOffset = -1;
    BytecodeIndex currentOffset = 0;
    for (BytecodeIndex i = 0; i < info->count; i++) {
        currentOffset += info->linenumberCounter[i];

        if (lastOffset <= offset && currentOffset > offset) {
            return info->linenumbers[i];
        }

        lastOffset = currentOffset;
    }
    return -1;
}
