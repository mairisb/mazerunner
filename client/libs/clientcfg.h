#ifndef CLIENT_CFG_H
#define CLIENT_CFG_H

#include <stdio.h>

#define CLIENT_CFG_FILENAME "client.cfg"
#define MAX_CFG_LINE_SIZE 64

#define SETTING_SERVER_IP "server_ip"
#define SETTING_SERVER_PORT "server_port"
#define SETTING_SCREEN_HEIGHT "screen_height"

struct ClientCfg {
    char serverIp[16];
    int serverPort;
};

struct ClientCfg cfg;

void loadCfg();
char *getLine(char *, int, FILE *);

#endif /* CLIENT_CFG_H */
