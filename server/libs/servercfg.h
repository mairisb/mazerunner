#ifndef SERVER_CFG_H
#define SERVER_CFG_H

#include <stdio.h>

#define SERVER_CFG_FILENAME "server.cfg"
#define MAX_CFG_LINE_SIZE 64

#define SETTING_SERVER_PORT "server_port"
#define SETTING_MAP_WIDTH "map_width"
#define SETTING_MAP_HEIGHT "map_height"
#define SETTING_MAP_FILENAME "map_filename"
#define SETTING_FOOD_COUNT "food_count"
#define SETTING_FOOD_TRHESHOLD "food_respawn_threshold"
#define SETTING_MOVE_RESOLUTION_MODE "move_resolution_mode"
#define SETTING_POINT_WIN_COUNT "point_win_count"
#define SETTING_GAME_START_TIMEOUT "game_start_timeout"
#define SETTING_GAME_END_TIMEOUT "game_end_timeout"
#define SETTING_FOOD_GEN_ATTEMPT_COUNT "food_gen_attempt_count"

struct ServerCfg {
    int serverPort;
    int mapHeight;
    int mapWidth;
    char mapFilename[54];
    int foodCount;
    int foodRespawnThreshold;
    char moveResolutionMode;
    int pointWinCount;
    int gameStartTimeout;
    int gameEndTimeout;
    int foodGenAttemptCount;
};

struct ServerCfg cfg;

int isComment(char *);
void printServerCfg();
void parseSetting(char *, char *);
int validateCfg();
int loadCfg();

#endif /* SERVER_CFG_H */
