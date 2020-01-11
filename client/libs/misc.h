#ifndef MISC_H
#define MISC_H

#define BUFF_SIZE 6071
#define MAX_PLAYER_CNT 8
#define MAX_UNAME_SIZE 16
#define MAX_MAP_HEIGHT 999
#define MAX_MAP_WIDTH 999
#define MAX_FOOD_CNT 999

struct Position {
    int y;
    int x;
};

struct Player {
    char uname[MAX_UNAME_SIZE + 1];
    int points;
    struct Position pos;
};

struct Food {
    struct Position pos;
};

#endif /* MISC_H */
