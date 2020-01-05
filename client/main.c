#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libs/clientcfg.h"
#include "libs/utility.h"
#include "libs/conn.h"
#include "libs/gui.h"

#define MAX_UNAME_SIZE 16
#define BUFF_SIZE 1024
#define MAX_PLAYER_CNT 8
#define DISPLAY_TXT_MAX_SIZE 128
#define MAX_MAP_HEIGHT 999
#define MAX_MAP_WIDTH 999

struct PlayerInfo {
    int playerCnt;
    char players[MAX_PLAYER_CNT][MAX_UNAME_SIZE + 1];
};

int getPlayerInfo(char *);
int getCoordsFromStr(char *);

struct PlayerInfo playerInfo;

char mapState[MAX_MAP_HEIGHT][MAX_MAP_WIDTH + 1];

int main(int argc, char** argv) {
    char uname[MAX_UNAME_SIZE + 1];
    char buff[BUFF_SIZE];
    enum MsgType msgType;
    char displayTxt[DISPLAY_TXT_MAX_SIZE];
    int connRes;
    int lastPlayerInfoByte;
    char mapRowsStr[3], mapColsStr[3];
    int mapRows, mapCols;
    char rowNumStr[3];
    int rowNum;

    /* Make sure map rows are null terminated */
    for (int i = 0; i < MAX_MAP_HEIGHT; i++) {
        memset(mapState[i], 0, MAX_MAP_WIDTH+1);
    }

    getCfg(); /* read and set client configuration */
    initGui(); /* start curses mode */

    displayTitle();

    displayUnamePrompt();
    getUname(uname, sizeof(uname));

    msgType = NO_MESSAGE;
    do { /* Connect to the server */
        displayStr("Connecting to server...");
        sleep(1);
        connRes = sockCreateConn(cfg.serverIp, cfg.serverPort);
        if (connRes < 0) {
            displayConnError();
        }
    } while (connRes < 0);

    displayStr("Connection established");
    sleep(1);

    do { /* Join game */
        if (msgType == GAME_IN_PROGRESS) {
            sleep(3);
        }

        displayStr("Joining game...");
        sleep(1);
        sockSendJoinGame(uname);

        sockRecv(buff, sizeof(buff));

        msgType = getMsgType(buff);
        switch(msgType) {
            case LOBBY_INFO:
                displayStr("Joined game");
                sleep(1);
                break;
            case GAME_IN_PROGRESS:
                displayGameInProgress();
                break;
            case USERNAME_TAKEN:
                displayUnameTaken();
                getUname(uname, sizeof(uname));
                break;
            default:
                endGui();
                printf("Error: received unexpected message\n");
                exit(1);
        }
    } while (msgType != LOBBY_INFO);

    do { /* Wait for other players and the game to start */
        getPlayerInfo(buff);
        displayLobbyInfo(playerInfo.playerCnt, playerInfo.players);

        sockRecv(buff, sizeof(buff));

        msgType = getMsgType(buff);
    } while (msgType == LOBBY_INFO);

    /* Start game */
    if (msgType == GAME_START) {
        lastPlayerInfoByte = getPlayerInfo(buff);
        strncpy(mapColsStr, buff+lastPlayerInfoByte, 3);
        strncpy(mapRowsStr, buff+lastPlayerInfoByte+3, 3);
        mapCols = getCoordsFromStr(mapColsStr);
        mapRows = getCoordsFromStr(mapRowsStr);
    } else {
        endGui();
        printf("Error: received unexpected message\n");
        exit(1);
    }

    /* Receive map */
    for (int i = 0; i < mapRows; i++) {
        sockRecv(buff, sizeof(buff));
        strncpy(rowNumStr, buff+lastPlayerInfoByte, 3);
        rowNum = getCoordsFromStr(rowNumStr);
        strncpy(mapState[rowNum], buff+4, mapCols);
        mapState[rowNum][mapCols] = '\0';
    }

    displayMap(mapRows, mapCols, mapState);
    getch();

    endGui();
    close(netSock);

    return 0;
}

int getPlayerInfo(char *msg) {
    /* Get player count */
    playerInfo.playerCnt = msg[1] - '0';

    /* Get players' usernames */
    memset(playerInfo.players, 0, sizeof(playerInfo.players[0][0]) * MAX_PLAYER_CNT * (MAX_UNAME_SIZE + 1));
    int i, j, k;
    for (i = 0, j = 0, k = 2; i < playerInfo.playerCnt; k++) {
        if (msg[k] == '\0' || j == 16) {
            i++;
            j = 0;
            continue;
        }
        playerInfo.players[i][j] = msg[k];
        j++;
    }
    return k; /* index of last buff byte read */
}

int getCoordsFromStr(char *str) {
    return (str[0]-'0') * 100 + (str[1]-'0') * 10 + (str[2]-'0');
}