#include "utility.h"
#include <stdio.h>
#include <string.h>

char *getLine(char *buff, int buffMaxSize, FILE *stream) {
    char *line;
    int buffSize;

    line = fgets(buff, buffMaxSize, stream);

    if (line == NULL) {
        return line;
    }

    buffSize = strlen(buff);
    if (buff[buffSize - 1] == '\n') {
        buff[buffSize - 1] = 0;

        if (buff[buffSize - 2] == '\r') {
            buff[buffSize - 2] = 0;
        }
    }

    return buff;
}

void printBytes(char *buff, int size) {
    int i;
    for (i = 0; i < size; i++) {
        if (buff[i] == '\0') {
            printf("\\0");
        } else if (buff[i] == '\n') {
            printf("\\n");
        } else {
            printf("%c", buff[i]);
        }
    }
    printf("\n");
}