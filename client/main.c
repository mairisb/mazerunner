#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "libs/clientcfg.h"
#include "libs/conn.h"
#include "libs/gui.h"
#include "libs/log.h"
#include "libs/misc.h"
#include "libs/utility.h"

char buff[BUFF_SIZE];
char uname[MAX_UNAME_SIZE + 1];
int playerCnt;
struct Player players[MAX_PLAYER_CNT];
struct Player playersOld[MAX_PLAYER_CNT];
int mapHeight, mapWidth;
char map[MAX_MAP_HEIGHT][MAX_MAP_WIDTH + 1];
int foodCnt;
struct Food food[MAX_FOOD_CNT];
int foodCntOld;
struct Food foodOld[MAX_FOOD_CNT];
int winStatus;
pthread_t threadGetDir;

int parsePlayerInfo(char *msg) {
    int bytesParsed = 0;

    /* get player count */
    playerCnt = msg[0] - '0';
    bytesParsed++;
    /* get players' usernames */
    for (int i = 0; i < playerCnt; i++) {
        strncpy(players[i].uname, (msg + bytesParsed), MAX_UNAME_SIZE);
        bytesParsed += MAX_UNAME_SIZE;
    }

    return bytesParsed;
}

int loadLobbyInfo() {
    int bytesParsed = 1;

    /* get player count and usernames */
    bytesParsed += parsePlayerInfo(buff + bytesParsed);

    return bytesParsed;
}

int loadStartGameInfo() {
    int bytesParsed = 1;
    char mapHeightStr[3];
    char mapWidthStr[3];

    /* get player count and usernames */
    bytesParsed += parsePlayerInfo(buff + bytesParsed);
    /* get map dimensions */
    strncpy(mapWidthStr, (buff + bytesParsed), 3);
    bytesParsed += 3;
    mapWidth = strToInt(mapWidthStr, 3);
    strncpy(mapHeightStr, (buff + bytesParsed), 3);
    bytesParsed += 3;
    mapHeight = strToInt(mapHeightStr, 3);

    return bytesParsed;
}

int loadMapRow() {
    int bytesParsed = 1;
    char mapRowNumStr[3];
    int mapRowNum;

    /* get row number */
    strncpy(mapRowNumStr, (buff + bytesParsed), 3);
    bytesParsed += 3;
    mapRowNum = strToInt(mapRowNumStr, 3);
    /* get map row */
    strncpy(map[mapRowNum - 1], (buff + bytesParsed), mapWidth);
    bytesParsed += mapWidth;

    return bytesParsed;
}

int loadGameUpdateInfo() {
    int bytesParsed = 1;
    char strNum[3];

    /* set old player and food positions for efficient frame redrawing */
    for (int i = 0; i < playerCnt; i++) {
        playersOld[i].pos.x = players[i].pos.x;
        playersOld[i].pos.y = players[i].pos.y;
    }
    foodCntOld = foodCnt; /* food count can change on subsequent updates */
    for (int i = 0; i < foodCntOld; i++) {
        foodOld[i].pos.x = food[i].pos.x;
        foodOld[i].pos.y = food[i].pos.y;
    }

    /* get player count */
    playerCnt = buff[1] - '0';
    bytesParsed++;
    /* get player info */
    for (int i = 0; i < playerCnt; i++) {
        /* get player y position */
        strncpy(strNum, (buff + bytesParsed), 3);
        bytesParsed += 3;
        players[i].pos.x = strToInt(strNum, 3);
        /* get player x position */
        strncpy(strNum, (buff + bytesParsed), 3);
        bytesParsed += 3;
        players[i].pos.y = strToInt(strNum, 3);
        /* get player points */
        strncpy(strNum, (buff + bytesParsed), 3);
        bytesParsed += 3;
        players[i].points = strToInt(strNum, 3);
    }
    /* get food count */
    strncpy(strNum, (buff + bytesParsed), 3);
    bytesParsed += 3;
    foodCnt = strToInt(strNum, 3);
    for (int i = 0; i < foodCnt; i++) {
        /* get food y position */
        strncpy(strNum, (buff + bytesParsed), 3);
        bytesParsed += 3;
        food[i].pos.x = strToInt(strNum, 3);
        /* get food x position */
        strncpy(strNum, (buff + bytesParsed), 3);
        bytesParsed += 3;
        food[i].pos.y = strToInt(strNum, 3);
    }

    return bytesParsed;
}

int loadGameEndInfo() {
    int bytesParsed = 1;
    char strNum[3];
    int winPoints;
    int myPoints;
    int draw;

    /* get player count */
    playerCnt = buff[1] - '0';
    bytesParsed++;
    /* get player info */
    for (int i = 0; i < playerCnt; i++) {
        /* get player username */
        strncpy(players[i].uname, (buff + bytesParsed), MAX_UNAME_SIZE);
        bytesParsed += MAX_UNAME_SIZE;
        /* get player points */
        strncpy(strNum, (buff + bytesParsed), 3);
        bytesParsed += 3;
        players[i].points = strToInt(strNum, 3);
    }

    /* get winner points */
    winPoints = -1;
    draw = 0;
    for (int i = 0; i < playerCnt; i++) {
        logOut("[DEBUG]\tPlayer points: %d\n", players[i].points);
        if (players[i].points > winPoints) {
            winPoints = players[i].points;
            draw = 0;
        } else if (players[i].points == winPoints) {
            draw = 1;
        }
    }

    /* get my points */
    for (int i = 0; i < playerCnt; i++) {
        if (strcmp(uname, players[i].uname) == 0) {
            myPoints = players[i].points;
        }
    }
    logOut("[DEBUG]\tMy %s points: %d\n", uname, myPoints);

    /* determine win/lose/draw */
    if (myPoints == winPoints) {
        if (draw) {
            winStatus = 0;
        } else {
            winStatus = 1;
        }
    } else {
        winStatus = -1;
    }

    return bytesParsed;
}

void *getDir(void *vargp) {
    while (true) {
        /* get keyboard input */
        char c = getch();
        switch(c) {
            case 'w':
            case 'W':
            case 'i':
            case 'I':
                sockSendMove(UP);
                break;
            case 'a':
            case 'A':
            case 'j':
            case 'J':
                sockSendMove(LEFT);
                break;
            case 's':
            case 'S':
            case 'k':
            case 'K':
                sockSendMove(DOWN);
                break;
            case 'd':
            case 'D':
            case 'L':
            case 'l':
                sockSendMove(RIGHT);
                break;
        }
    }
}

int main() {
    int connRes;
    enum MsgType msgType;

    /* load initial configuration */
    loadCfg();
    /* start ncurses */
    guiStart();

    displayMainScreen();

    /* get username */
    displayUnamePrompt();
    getUname(uname);

    /* create socket */
    sockCreate();

    /* connect to the server */
    do {
        displayStr("Connecting to server...");
        sleep(1);
        connRes = sockConn(cfg.serverIp, cfg.serverPort);
        if (connRes < 0) {
            displayConnError();
        }
    } while (connRes < 0);
    displayStr("Connection established");
    sleep(1);

    /* join lobby */
    msgType = NO_MESSAGE;
    do {
        if (msgType == GAME_IN_PROGRESS) {
            sleep(3);
        }
        displayStr("Joining game...");
        sleep(1);
        sockSendJoinGame(uname);
        sockRecvJoinGameResp(buff);
        msgType = getMsgType(buff);
        switch (msgType) {
            case LOBBY_INFO:
                displayStr("Joined game");
                sleep(1);
                break;
            case GAME_IN_PROGRESS:
                displayGameInProgress();
                break;
            case USERNAME_TAKEN:
                displayUnameTaken();
                getUname(uname);
                break;
            default:
                break;
        }
    } while (msgType != LOBBY_INFO);

    /* wait for other players and the game to start */
    do {
        loadLobbyInfo();
        displayLobbyInfo(playerCnt, players);
        sockRecvLobbyInfo(buff);
        msgType = getMsgType(buff);
    } while (msgType == LOBBY_INFO);

    /* game starts */
    loadStartGameInfo();

    /* load map */
    for (int i = 0; i < mapHeight; i++) {
        sockRecvMapRow(buff, mapWidth);
        loadMapRow();
    }
    displayMap(map, mapHeight, mapWidth, players, playerCnt);

    /* get initial player and food positions */
    sockRecvGameUpdate(buff);
    loadGameUpdateInfo();

    /* initialize old player and food positions */
    for (int i = 0; i < playerCnt; i++) {
        playersOld[i].pos.x = players[i].pos.x;
        playersOld[i].pos.y = players[i].pos.y;
    }
    foodCntOld = foodCnt;
    for (int i = 0; i < foodCntOld; i++) {
        foodOld[i].pos.x = food[i].pos.x;
        foodOld[i].pos.y = food[i].pos.y;
    }

    /* display initial player and food positions */
    updateMap(players, playersOld, playerCnt, food, foodOld, foodCnt, foodCntOld);

    /* start reading user keyboard input */
    pthread_create(&threadGetDir, NULL, getDir, NULL);

    /* game in progress */
    do {
        /* receive updated player and food info */
        sockRecvGameUpdate(buff);
        msgType = getMsgType(buff);
        if (msgType != GAME_UPDATE) {
            break;
        }
        loadGameUpdateInfo();
        updateMap(players, playersOld, playerCnt, food, foodOld, foodCnt, foodCntOld);
    } while (true);

    /* stop reading user keyboard input */
    pthread_cancel(threadGetDir);
    pthread_join(threadGetDir, NULL);

    /* player was killed */
    if (msgType == PLAYER_DEAD) {
        /* receive final frame */
        sockRecvGameUpdate(buff);
        msgType = getMsgType(buff);
    }

    while (msgType != GAME_END) {
        loadGameUpdateInfo();
        sockRecvGameUpdate(buff);
        msgType = getMsgType(buff);
    }

    loadGameEndInfo();
    updateMap(players, playersOld, playerCnt, food, foodOld, foodCnt, foodCntOld);
    displayGameOver(winStatus);
    displayScoreBoard(players, playerCnt);

    guiEnd();
    close(netSock);

    return 0;
}