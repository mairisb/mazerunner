#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "clientcfg.h"
#include "utility.h"

#define MAX_UNAME_SIZE 16
#define BUFF_SIZE 1024
#define MAX_PLAYER_CNT 8

enum MsgType {
    JOIN_GAME = '0',
    MOVE = '1',
    LOBBY_INFO = '2',
    GAME_IN_PROGRESS = '3',
    USERNAME_TAKEN = '4',
    GAME_START = '5',
    MAP_ROW = '6',
    GAME_UPDATE = '7',
    PLAYER_DEAD = '8',
    GAME_END = '9'
};

char getMsgType(char *msg) {
    return msg[0];
}

int sockCreate() {
    return socket(AF_INET, SOCK_STREAM, 0);
}

void sockConn(int sock, char *ip, int port) {
    struct sockaddr_in remote;
    remote.sin_addr.s_addr = inet_addr(ip);
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    printf("Attempting to connect to the server...\n");
    if (connect(sock, (struct sockaddr *) &remote, sizeof(struct sockaddr_in)) < 0) {
        perror("Error connecting to server");
        exit(1);
    }
    printf("Connection established!\n");
}

int sockSend(int sock, char* req) {
    return send(sock, req, strlen(req), 0);
}

int sockRecv(int sock, char* buff, short buffSize) {
    strcpy(buff, "");
    return recv(sock, buff, buffSize, 0);
}

int sockSendJoinGame(int sock, char *uname) {
    char msg[18] = "";
    sprintf(msg, "%c%s", JOIN_GAME, uname);
    return sockSend(sock, msg);
}

int main(int argc, char** argv) {
    struct ClientCfg clientCfg;
    int netSock;
    char uname[MAX_UNAME_SIZE + 1];
    char buff[BUFF_SIZE];
    enum MsgType msgType;
    int playerCnt;
    char players[MAX_PLAYER_CNT][MAX_UNAME_SIZE + 1];
    memset(players, 0, sizeof(players[0][0]) * MAX_PLAYER_CNT * (MAX_UNAME_SIZE + 1));

    /* Read and set client configuration */
    setCfg(&clientCfg);

    /* Create and connect socket to server */
    netSock = sockCreate();
    sockConn(netSock, clientCfg.serverIp, clientCfg.serverPort);

    printf("Enter username: ");
    getLine(uname, sizeof(uname), stdin);

    do {
        if (msgType == GAME_IN_PROGRESS) {
            sleep(3);
        }

        printf("Attempting to join game...\n");
        sockSendJoinGame(netSock, uname);

        sockRecv(netSock, buff, sizeof(buff));

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

    do {
        playerCnt = buff[1] - '0';

        int i, j, k;
        for (i = 0, j = 0, k = 2; i < playerCnt; k++) {
            if (buff[k] == '\0' || j == 16) {
                i++;
                j = 0;
                continue;
            }
            players[i][j] = buff[k];
            j++;
        }
        for (int i = 0; i < (clientCfg.screenHeight - playerCnt - 1); i++) {
            printf("\n");
        }
        printf("Players: %d/%d\n", playerCnt, MAX_PLAYER_CNT);
        for (int i = 0; i < playerCnt; i++) {
            printf("%s\n", players[i]);
        }

        sockRecv(netSock, buff, sizeof(buff));

        if (getMsgType(buff) != LOBBY_INFO && getMsgType(buff) != GAME_START) {
            printf("Error: received unexpected message\n");
            exit(1);
        }
    } while (getMsgType(buff) != GAME_START);


    printf("Exit");
    close(netSock);

    return 0;
}
