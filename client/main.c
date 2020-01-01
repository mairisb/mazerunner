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
    char displayTxt[DISPLAY_TXT_MAX_SIZE];

    getCfg(); /* read and set client configuration */
    initGui(); /* start curses mode */

    displayTitle();

    erase();
    strcpy(displayTxt, "Enter username: ");
    mvprintw(row / 2, (col - strlen(displayTxt) - sizeof(uname)) / 2, "%s", displayTxt);
    refresh();
    getUname(uname, sizeof(uname));

    sockCreateConn(cfg.serverIp, cfg.serverPort); /* connect to the server */

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
                erase();
                strcpy(displayTxt, "Game already in progress.");
                mvprintw(row / 2, (col - strlen(displayTxt)) / 2, "%s", displayTxt);
                strcpy(displayTxt, "<Press any button to join again>");
                mvprintw(row / 2 + 1, (col - strlen(displayTxt)) / 2, "%s", displayTxt);
                refresh();
                getch();
                break;
            case USERNAME_TAKEN:
                erase();
                strcpy(displayTxt, "Username already taken");
                mvprintw(row / 2, (col - strlen(displayTxt)) / 2, "%s", displayTxt);
                strcpy(displayTxt, "Enter new username: ");
                mvprintw(row / 2 + 2, (col - strlen(displayTxt) - sizeof(uname)) / 2, "%s", displayTxt);
                refresh();
                getUname(uname, sizeof(uname));
                break;
            default:
                endGui();
                printf("Error: received unexpected message\n");
                exit(1);
        }
    } while (msgType != LOBBY_INFO);

    do { /* Wait for other players and the game to start */
        getLobbyInfo(buff);

        erase();
        sprintf(displayTxt, "Players: %d/%d\n", lobbyInfo.playerCnt, MAX_PLAYER_CNT);
        mvprintw(row / 2, (col - strlen(displayTxt)) / 2, "%s", displayTxt);
        for (int i = 0; i < lobbyInfo.playerCnt; i++) {
            mvprintw(row / 2 + 2 + i, (col - strlen(lobbyInfo.players[i]) - sizeof(uname)) / 2, "%s", lobbyInfo.players[i]);
            // printf("%s\n", lobbyInfo.players[i]);
        }
        refresh();

        sockRecv(buff, sizeof(buff));

        msgType = getMsgType(buff);
        if (msgType != LOBBY_INFO && msgType != GAME_START) {
            endGui();
            printf("Error: received unexpected message\n");
            exit(1);
        }
    } while (getMsgType(buff) != GAME_START);

    endGui();
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