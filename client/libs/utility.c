#include "utility.h"
#include <math.h>
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

int strToInt(char *str, int strLen) {
    int res = 0;
    int powOf10;

    if (strLen <= 0) {
        return -1;
    }

    powOf10 = strLen-1;
    for (int i = 0; i < strLen; i++) {
        res += (str[i] - '0') * pow(10, powOf10);
        powOf10--;
    }

    return res;
}