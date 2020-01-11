#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

#define LOG_FILE_NAME "client.log"

void logOut(const char *format, ...);
void logOutBytes(char *bytes, int bytesCnt);

#endif /* LOG_H */
