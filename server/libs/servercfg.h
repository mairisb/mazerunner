#ifndef SERVER_CFG_H
#define SERVER_CFG_H

#include <stdio.h>

#define SERVER_CFG_FILENAME "server.cfg"
#define MAX_CFG_LINE_SIZE 64

#define SETTING_SERVER_PORT "server_port"
#define SETTING_MAP_FILENAME "map_filename"

struct ServerCfg {
    int serverPort;
    char mapFilename[54];
};

struct ServerCfg cfg;

int isComment(char *);
void printServerCfg();
void parseSetting(char *, char *);
void initCfg();
void validateCfg();
void loadCfg();

#endif /* SERVER_CFG_H */
