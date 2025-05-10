#include <stdio.h>
#include <string.h>

#include "util.h"


char* readFile(const char* path, char** content)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        const char* message = "Could not open file \"%s\".\n";
        char* error = malloc(sizeof(char) * (strlen(path) + strlen(message)));
        sprintf(error, message, path);
        return error;
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        const char* message = "Not enough memory to read \"%s\".\n";
        char* error = malloc(sizeof(char) * (strlen(path) + strlen(message)));
        sprintf(error, message, path);
        return error;
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    buffer[bytesRead] = '\0';

    fclose(file);
    *content = buffer;
    return NULL;
}