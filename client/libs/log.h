#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

#define LOG_FILE_NAME "client.log"

void logOut(const char *format, ...);

#endif /* LOG_H */
