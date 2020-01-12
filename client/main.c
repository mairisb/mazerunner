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
int mapHeight, mapWidth;
char map[MAX_MAP_HEIGHT][MAX_MAP_WIDTH + 1];
int foodCnt;
struct Food food[MAX_FOOD_CNT];

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
        logOut("[DEBUG]\t%s %d\n", players[i].uname, players[i].points);
    }

    return bytesParsed;
}

void *getDirection(void *vargp) {
    while (true) {
        char c = getch();
        switch(c) {
            case 'w':
            case 'W':
                sockSendMove(UP);
                break;
            case 'a':
            case 'A':
                sockSendMove(LEFT);
                break;
            case 's':
            case 'S':
                sockSendMove(DOWN);
                break;
            case 'd':
            case 'D':
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

    displayMap(mapHeight, mapWidth, map);

    sockRecvGameUpdate(buff);
    loadGameUpdateInfo();

    updateMap(players, playerCnt, food, foodCnt);

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, getDirection, NULL);

    do {
        sockRecvGameUpdate(buff);
        if (getMsgType(buff) != GAME_UPDATE) {
            break;
        }
        loadGameUpdateInfo();
        displayMap(mapHeight, mapWidth, map);
        updateMap(players, playerCnt, food, foodCnt);
    } while (true);

    pthread_cancel(thread_id);
    pthread_join(thread_id, NULL);

    if (getMsgType(buff) == PLAYER_DEAD) {
        sockRecvGameUpdate(buff);
    }

    loadGameEndInfo();


    displayScoreBoard(players, playerCnt);

    guiEnd();
    close(netSock);

    return 0;
}