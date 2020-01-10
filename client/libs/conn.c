#include "conn.h"

#include <arpa/inet.h>
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "utility.h"

char getMsgType(char *msg) {
    return msg[0];
}

void sockCreate() {
    netSock = socket(AF_INET, SOCK_STREAM, 0);
}

int sockConn(char *ip, int port) {
    struct sockaddr_in remote;
    remote.sin_addr.s_addr = inet_addr(ip);
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    return connect(netSock, (struct sockaddr *) &remote, sizeof(struct sockaddr_in));
}

int sockCreateConn(char *ip, int port) {
    sockCreate();
    return sockConn(ip, port);
}

int sockSend(char *message, int messageSize) {
    int sentBytes = 0;
    int sentTotal = 0;

    while (sentTotal < messageSize) {
        sentBytes = send(netSock, &message[sentTotal], messageSize - sentTotal, 0);
        if (sentBytes < 0) {
            logOut("[ERROR] Sending to socket failed: %s\n", strerror(errno));
            exit(1);
        } else if (sentBytes == 0) {
            logOut("[ERROR] Server disconnected: %s\n", strerror(errno));
            exit(1);
            return -1;
        } else {
            sentTotal += sentBytes;
        }
    }

    return 0;
}

int sockSendJoinGame(char *uname) {
    const int msgSize = 1 + MAX_UNAME_SIZE;
    char msg[1 + msgSize];
    memset(msg, 0, msgSize);
    sprintf(msg, "%c%s", JOIN_GAME, uname);
    return sockSend(msg, msgSize);
}

void sockSendMove(char c) {
    char msg[3];
    sprintf(msg, "%c%c", MOVE, c);
    sockSend(msg, 2);
    logOut("[INFO]\tMessage MOVE sent: %.*s\n", 2, msg);
}

int sockRecv(char *buff, int messageSize) {
    int recBytes = 0;
    int recTotal = 0;

    while (recTotal < messageSize) {
        recBytes = recv(netSock, &buff[recTotal], messageSize - recTotal, 0);
        if (recBytes < 0) {
            // perror("Error reading socket: ");
            // printf("Removing player\n");
            // resetPlayer(netSock);
            // errno = 0;
            return -1;
        } else if (recBytes == 0) {
            // printf("Client disconnected, removing player\n");
            // resetPlayer(netSock);
            return -1;
        } else {
            recTotal += recBytes;
        }
    }

    // printf("Message received %s\n", buff);
    return 0;
}

int sockRecvPlayers(char *buff) {
    int playerCnt;

    /* get player count */
    if (sockRecv(buff+1, 1) < 0) {
        return -1;
    }
    playerCnt = buff[1] - '0';
    /* get usernames */
    if (sockRecv((buff + 2), (MAX_UNAME_SIZE * playerCnt)) < 0) {
        return -1;
    }

    return playerCnt;
}

int sockRecvGameStart(char *buff) {
    int playerCnt = -1;

    /* get player count and usernames */
    playerCnt = sockRecvPlayers(buff);
    if (playerCnt < 0) {
        return -1;
    }
    /* get map dimensions */
    if (sockRecv((buff + 2 + (MAX_UNAME_SIZE * playerCnt)), 6) < 0) {
        return -1;
    }

    return playerCnt;
}

int sockRecvLobbyInfo(char *buff) {
    /* get message type */
    if (sockRecv(buff, 1) < 0) {
        return -1;
    }

    /* get one of four possible messages */
    switch (getMsgType(buff)) {
        case LOBBY_INFO:
            return sockRecvPlayers(buff);
        case GAME_IN_PROGRESS:
            return 0;
        case USERNAME_TAKEN:
            return 0;
        case GAME_START:
            return sockRecvGameStart(buff);
        default:
            return -1;
    }
}

int sockRecvMapRow(char *buff, int mapCols) {
    /* get message type */
    if (sockRecv(buff, 1) < 0) {
        return -1;
    }

    if (getMsgType(buff) != MAP_ROW) {
        return -1;
    }

    /* get row num and row */
    if (sockRecv((buff + 1), 3 + mapCols) < 0) {
        return -1;
    }

    logOut("[INFO]\tMessage MAP_ROW received: %.*s\n", 4+mapCols, buff);

    return 0;
}

int sockRecvGameUpdate(char *buff) {
    int playerCnt;
    int foodCnt;

    /* get message type */
    if (sockRecv(buff, 1) < 0) {
        return -1;
    }

    if (getMsgType(buff) != GAME_UPDATE) {
        return -1;
    }

    /* get player count */
    if (sockRecv((buff + 1), 1) < 0) {
        return -1;
    }
    playerCnt = buff[1] - '0';
    /* get player coords and points */
    if (sockRecv((buff + 2), (9 * playerCnt)) < 0) {
        return -1;
    }

    /* get food count */
    if (sockRecv((buff + 2 + (9 * playerCnt)), 3) < 0) {
        return -1;
    }
    foodCnt = (int)(buff[2 + (9 * playerCnt)] - '0') * 100 + (int)(buff[2 + (9 * playerCnt) + 1] - '0') * 10 + (int)(buff[2 + (9 * playerCnt) + 2] - '0');
    /* get food coords */
    if (sockRecv((buff + 2 + (9 * playerCnt) + 3), (6 * foodCnt)) < 0) {
        return -1;
    }

    logOut("[INFO]\tMessage GAME_UPDATE received: %.*s\n", (2 + (9 * playerCnt) + 3 + (6 * foodCnt)), buff);

    return 0;
}