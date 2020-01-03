#include "servercfg.h"
#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int isComment(char *line) {
    return line[0] == '#';
}

void printServerCfg() {
    printf("Server's configuration:\n");
    printf("\t%s = %d\n", SETTING_SERVER_PORT, cfg.serverPort);
    printf("\t%s = %s\n", SETTING_MAP_FILENAME, cfg.mapFilename);
}

void parseSetting(char *key, char *val) {
    if (strcmp(key, SETTING_SERVER_PORT) == 0) {
        sscanf(val, "%d", &cfg.serverPort);
    } else if (strcmp(key, SETTING_MAP_FILENAME) == 0) {
        strcpy(cfg.mapFilename, val);
    }
}

void initCfg() {
    cfg.serverPort = -1;
    memset(cfg.mapFilename, 0, sizeof(cfg.mapFilename));
}

void validateCfg() {
    if (cfg.serverPort == -1) {
        fprintf(stderr, "Configuration '%s' is mandatory\n", SETTING_SERVER_PORT);
        exit(1);
    } else if (strlen(cfg.mapFilename) == 0) {
        fprintf(stderr, "Configuration '%s' is mandatory\n", SETTING_MAP_FILENAME);
        exit(1);
    }
}

void loadCfg() {
    char line[MAX_CFG_LINE_SIZE];
    char key[MAX_CFG_LINE_SIZE];
    char val[MAX_CFG_LINE_SIZE];
    FILE *file;

    file = fopen(SERVER_CFG_FILENAME, "r");
    if (file == NULL) {
        fprintf(stderr, "Could not open configuration file '%s'\n", SERVER_CFG_FILENAME);
        perror("");
        exit(1);
    }

    initCfg();

    /* Read config file */
    while (getLine(line, MAX_CFG_LINE_SIZE, file) != NULL) {
        if (strlen(line) > 0 && !isComment(line)) {
            sscanf(line, "%s = %s", key, val);
            parseSetting(key, val);
        }
    }

    validateCfg();
    printServerCfg();

    fclose(file);
}
