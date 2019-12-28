#include "utility.h"
#include <stdio.h>
#include <string.h>

char *getLine(char *buffer, int bufferMaxSize, FILE *stream) {
    char *result;
    int bufferSize;

    result = fgets(buffer, bufferMaxSize, stream);

    if (result == NULL) {
        return result;
    }

    bufferSize = strlen(buffer);
    if (buffer[bufferSize - 1] == '\n') {
        buffer[bufferSize - 1] = 0;

        if (buffer[bufferSize - 2] == '\r') {
            buffer[bufferSize - 2] = 0;
        }
    }

    return buffer;
}