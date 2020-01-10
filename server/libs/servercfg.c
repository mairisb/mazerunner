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
    printf("\t%s = %d\n", SETTING_MAP_HEIGHT, cfg.mapHeight);
    printf("\t%s = %d\n", SETTING_MAP_WIDTH, cfg.mapWidth);
    printf("\t%s = %d\n", SETTING_FOOD_COUNT, cfg.foodCount);
    printf("\t%s = %d\n", SETTING_FOOD_TRHESHOLD, cfg.foodRespawnThreshold);
    printf("\t%s = %c\n", SETTING_MOVE_RESOLUTION_MODE, cfg.moveResolutionMode);
}

void parseSetting(char *key, char *val) {
    if (strcmp(key, SETTING_SERVER_PORT) == 0) {
        sscanf(val, "%d", &cfg.serverPort);
    } else if (strcmp(key, SETTING_MAP_FILENAME) == 0) {
        strcpy(cfg.mapFilename, val);
    } else if (strcmp(key, SETTING_MAP_HEIGHT) == 0) {
        sscanf(val, "%d", &cfg.mapHeight);
    } else if (strcmp(key, SETTING_MAP_WIDTH) == 0) {
        sscanf(val, "%d", &cfg.mapWidth);
    } else if (strcmp(key, SETTING_FOOD_COUNT) == 0) {
        sscanf(val, "%d", &cfg.foodCount);
    } else if (strcmp(key, SETTING_FOOD_TRHESHOLD) == 0) {
        sscanf(val, "%d", &cfg.foodRespawnThreshold);
    } else if (strcmp(key, SETTING_MOVE_RESOLUTION_MODE) == 0) {
        cfg.moveResolutionMode = val[0];
    }
}

int validateCfg() {
    if (cfg.serverPort == 0) {
        fprintf(stderr, "Configuration '%s' is mandatory\n", SETTING_SERVER_PORT);
        return -1;
    } else if (strlen(cfg.mapFilename) == 0) {
        fprintf(stderr, "Configuration '%s' is mandatory\n", SETTING_MAP_FILENAME);
        return -1;
    } else if (cfg.mapHeight <= 0) {
        fprintf(stderr, "Configuration '%s' can not be <= 0\n", SETTING_MAP_HEIGHT);
        return -1;
    } else if (cfg.mapWidth <= 0) {
        fprintf(stderr, "Configuration '%s' can not be <= 0\n", SETTING_MAP_WIDTH);
        return -1;
    } else if (cfg.foodCount <= 0) {
        fprintf(stderr, "Configuration '%s' can not be <= 0\n", SETTING_FOOD_COUNT);
        return -1;
    } else if (cfg.foodRespawnThreshold >= cfg.foodCount) {
        fprintf(stderr, "Configuration '%s' can not be >= to setting '%s'\n", SETTING_FOOD_TRHESHOLD, SETTING_FOOD_COUNT);
        return -1;
    } else if (cfg.moveResolutionMode != 'F' && cfg.moveResolutionMode != 'L') {
        fprintf(stderr, "Configuration '%s' value can only be 'F' (first) or 'L' (last)\n", SETTING_MOVE_RESOLUTION_MODE);
        return -1;
    }

    return 0;
}

int loadCfg() {
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

    /* Read config file */
    while (getLine(line, MAX_CFG_LINE_SIZE, file) != NULL) {
        if (strlen(line) > 0 && !isComment(line)) {
            sscanf(line, "%s = %s", key, val);
            parseSetting(key, val);
        }
    }

    fclose(file);

    if (validateCfg() < 0) {
        return -1;
    }

    printServerCfg();
    return 0;
}
