#include "clientcfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utility.h"

int isComment(char *line) {
    return line[0] == '#';
}

void parseSetting(struct ClientCfg *clientCfg, char *key, char *val) {
    if (strcmp(key, SETTING_SERVER_IP) == 0) {
        strcpy(clientCfg->serverIp, val);
    } else if (strcmp(key, SETTING_SERVER_PORT) == 0) {
        sscanf(val, "%d", &clientCfg->serverPort);
    }
}

void setCfg(struct ClientCfg *clientCfg) {
    char line[MAX_CFG_LINE_SIZE];
    char key[MAX_CFG_LINE_SIZE];
    char val[MAX_CFG_LINE_SIZE];
    FILE *file;

    file = fopen(CLIENT_CFG_FILENAME, "r");
    if (file == NULL) {
        printf("Could not open configuration file '%s'", CLIENT_CFG_FILENAME);
        perror("");
        exit(1);
    }

    /* Read config file */
    while (getLine(line, MAX_CFG_LINE_SIZE, file) != NULL) {
        if (!isComment(line)) {
            sscanf(line, "%s = %s", key, val);
            parseSetting(clientCfg, key, val);
        }
    }

    printf("Client's configuration:\n");
    printf("\%s = %s\n", SETTING_SERVER_IP, clientCfg->serverIp);
    printf("\t%s = %d\n", SETTING_SERVER_PORT, clientCfg->serverPort);
}