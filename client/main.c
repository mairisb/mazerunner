#include <curses.h>
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

struct LobbyInfo {
    int playerCnt;
    char players[MAX_PLAYER_CNT][MAX_UNAME_SIZE + 1];
};

void getLobbyInfo(char *msgLobbyInfo);

struct LobbyInfo lobbyInfo;

int main(int argc, char** argv) {
    char uname[MAX_UNAME_SIZE + 1];
    char buff[BUFF_SIZE];
    enum MsgType msgType;

    /* Read and set client configuration */
    getCfg();

    /* Connect to the server */
    sockCreateConn(cfg.serverIp, cfg.serverPort);

    printf("Enter username: ");
    getLine(uname, sizeof(uname), stdin);

    do { /* Join game */
        if (msgType == GAME_IN_PROGRESS) {
            sleep(3);
        }

        printf("Attempting to join game...\n");
        sockSendJoinGame(uname);

        sockRecv(buff, sizeof(buff));

        msgType = getMsgType(buff);
        switch(msgType) {
            case LOBBY_INFO:
                printf("Joined game.\n");
                break;
            case GAME_IN_PROGRESS:
                printf("Game already in progress. Will try to join again.\n");
                break;
            case USERNAME_TAKEN:
                printf("Username %s already taken.\n", uname);
                printf("Enter another username to join again: ");
                getLine(uname, sizeof(uname), stdin);
                break;
            default:
                printf("Error: received unexpected message\n");
                exit(1);
        }
    } while (msgType != LOBBY_INFO);

    do { /* Wait for other players and the game to start */
        getLobbyInfo(buff);

        printf("Players: %d/%d\n", lobbyInfo.playerCnt, MAX_PLAYER_CNT);
        for (int i = 0; i < lobbyInfo.playerCnt; i++) {
            printf("%s\n", lobbyInfo.players[i]);
        }

        sockRecv(buff, sizeof(buff));

        if (getMsgType(buff) != LOBBY_INFO && getMsgType(buff) != GAME_START) {
            printf("Error: received unexpected message\n");
            exit(1);
        }
    } while (getMsgType(buff) != GAME_START);


    printf("Exit");
    close(netSock);

    return 0;
}

void getLobbyInfo(char *msgLobbyInfo) {
    /* Get player count */
    lobbyInfo.playerCnt = msgLobbyInfo[1] - '0';

    /* Get players' usernames */
    memset(lobbyInfo.players, 0, sizeof(lobbyInfo.players[0][0]) * MAX_PLAYER_CNT * (MAX_UNAME_SIZE + 1));
    int i, j, k;
    for (i = 0, j = 0, k = 2; i < lobbyInfo.playerCnt; k++) {
        if (msgLobbyInfo[k] == '\0' || j == 16) {
            i++;
            j = 0;
            continue;
        }
        lobbyInfo.players[i][j] = msgLobbyInfo[k];
        j++;
    }
}