#include "log.h"

#include <stdio.h>

void logOut(const char *format, ...) {
    va_list arg;
    FILE *logFile = fopen(LOG_FILE_NAME, "a");

    if (logFile != NULL) {
        va_start (arg, format);
        vfprintf (logFile, format, arg);
        va_end (arg);

        fclose(logFile);
    }
}

void logOutBytes(char *bytes, int bytesCnt) {
    int i;
    for (i = 0; i < bytesCnt; i++) {
        if (bytes[i] == '\0') {
            logOut("\\0");
        } else {
            logOut("%c", bytes[i]);
        }
    }
    logOut("\n");
}
