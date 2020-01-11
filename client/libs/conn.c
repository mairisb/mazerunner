#include "conn.h"

#include <arpa/inet.h>
#include <ncurses.h>
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
#include "misc.h"
#include "utility.h"

enum MsgType getMsgType(char *msg) {
    return (enum MsgType)msg[0];
}

char *getMsgTypeStr(enum MsgType msgType) {
    switch (msgType) {
        JOIN_GAME:
            return "JOIN_GAME";
        MOVE:
            return "MOVE";
        LOBBY_INFO:
            return "LOBBY_INFO";
        GAME_IN_PROGRESS:
            return "GAME_IN_PROGRESS";
        USERNAME_TAKEN:
            return "USERNAME_TAKEN";
        GAME_START:
            return "GAME_START";
        MAP_ROW:
            return "MAP_ROW";
        GAME_UPDATE:
            return "GAME_UPDATE";
        PLAYER_DEAD:
            return "PLAYER_DEAD";
        GAME_END:
            return "GAME_END";
        default:
            return "UNKNOWN";
    }
}

void sockCreate() {
    /* Create socket */
    netSock = socket(AF_INET, SOCK_STREAM, 0);
    if (netSock < 0) {
        logOut("[ERROR]\tError creating socket: %s\n", strerror(errno));
        exit(1);
    }
}

int sockConn(char *ip, int port) {
    struct sockaddr_in server;
    int connRes;

    /* Set server connection parameters */
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    logOut("[INFO]\tConnecting to %s:%d\n", ip, port);
    connRes = connect(netSock, (struct sockaddr *) &server, sizeof(struct sockaddr_in));
    if (connRes < 0) {
        logOut("[ERROR]\tError connecting to the server: %s\n", strerror(errno));
    } else {
        logOut("[INFO]\tConnection established\n");
    }

    return connRes;
}

int sockSend(char *message, int messageSize) {
    int sentBytes = 0;
    int sentTotal = 0;

    while (sentTotal < messageSize) {
        sentBytes = send(netSock, &message[sentTotal], messageSize - sentTotal, 0);
        if (sentBytes < 0) {
            logOut("[ERROR]\tError sending to socket: %s\n", strerror(errno));
            exit(1);
        } else if (sentBytes == 0) {
            logOut("[ERROR]\tServer disconnected: %s\n", strerror(errno));
            exit(1);
        } else {
            sentTotal += sentBytes;
        }
    }

    return sentBytes;
}

int sockSendJoinGame(char *uname) {
    const int msgSize = 1 + MAX_UNAME_SIZE;
    char msg[msgSize];
    int retVal;

    memset(msg, 0, msgSize);
    sprintf(msg, "%c%s", JOIN_GAME, uname);
    retVal = sockSend(msg, msgSize);

    logOut("[INFO]\tMessage JOIN_GAME sent: ");
    logOutBytes(msg, msgSize);

    return retVal;
}

int sockSendMove(enum Direction direction) {
    const int msgSize = 2;
    char msg[msgSize];
    int retVal;

    sprintf(msg, "%c%c", MOVE, direction);
    retVal = sockSend(msg, msgSize);

    logOut("[INFO]\tMessage MOVE sent: %.*s\n", msgSize, msg);

    return retVal;
}

int sockRecv(char *buff, int messageSize) {
    int recBytes = 0;
    int recTotal = 0;

    while (recTotal < messageSize) {
        recBytes = recv(netSock, &buff[recTotal], messageSize - recTotal, 0);
        if (recBytes < 0) {
            logOut("[ERROR]\tError reading socket: %s\n", strerror(errno));
            exit(1);
        } else if (recBytes == 0) {
            logOut("[ERROR]\tServer disconnected: %s\n", strerror(errno));
            exit(1);
        } else {
            recTotal += recBytes;
        }
    }

    return recTotal;
}

int sockRecvPlayers(char *buff) {
    int msgSize = 0;
    int playerCnt;

    /* get player count */
    msgSize += sockRecv(buff, 1);
    playerCnt = buff[0] - '0';
    /* get players' usernames */
    msgSize += sockRecv((buff + msgSize), (MAX_UNAME_SIZE * playerCnt));

    return msgSize;
}

int sockRecvJoinGameResp(char *buff) {
    int msgSize = 0;
    enum MsgType msgType;

    /* get message type */
    msgSize += sockRecv(buff, 1);

    /* get one of four possible messages */
    msgType = getMsgType(buff);
    switch (msgType) {
        case LOBBY_INFO:
            /* get players' usernames */
            msgSize += sockRecvPlayers(buff + msgSize);
            logOut("[INFO]\tMessage LOBBY_INFO received: ");
            logOutBytes(buff, msgSize);
            return msgSize;
        case GAME_IN_PROGRESS:
            logOut("[INFO]\tMessage GAME_IN_PROGRESS received: %.*s\n", msgSize, buff);
            return msgSize;
        case USERNAME_TAKEN:
            logOut("[INFO]\tMessage USERNAME_TAKEN received: %.*s\n", msgSize, buff);
            return msgSize;
        default:
            logOut("[ERROR]\tUnexpected message received\n\tExpected one of: LOBBY_INFO GAME_IN_PROGRESS USERNAME_TAKEN\n\tReceived: %s\n", getMsgTypeStr(msgType));
            exit(1);
    }
}

int sockRecvLobbyInfo(char *buff) {
    int msgSize = 0;
    enum MsgType msgType;

    /* get message type */
    msgSize += sockRecv(buff, 1);

    /* get one of four possible messages */
    msgType = getMsgType(buff);
    switch (getMsgType(buff)) {
        case LOBBY_INFO:
            /* get players' usernames */
            msgSize += sockRecvPlayers(buff + msgSize);
            logOut("[INFO]\tMessage LOBBY_INFO received: ");
            logOutBytes(buff, msgSize);
            return msgSize;
        case GAME_START:
            /* get players' usernames */
            msgSize += sockRecvPlayers(buff + msgSize);
            /* get map dimensions */
            msgSize += sockRecv((buff + msgSize), 6);
            logOut("[INFO]\tMessage GAME_START received: ");
            logOutBytes(buff, msgSize);
            return msgSize;
        default:
            logOut("[ERROR]\tUnexpected message received\n\tExpected one of: LOBBY_INFO GAME_START\n\tReceived: %s\n", getMsgTypeStr(msgType));
            exit(1);
    }
}

int sockRecvMapRow(char *buff, int mapWidth) {
    int msgSize = 0;
    enum MsgType msgType;

    /* get message type */
    msgSize += sockRecv(buff, 1);
    msgType = getMsgType(buff);
    if (msgType != MAP_ROW) {
        logOut("[ERROR]\tUnexpected message received\n\tExpected: MAP_ROW\n\tReceived: %s\n", getMsgTypeStr(msgType));
        exit(1);
    }
    /* get row number and map row */
    msgSize += sockRecv((buff + msgSize), 3 + mapWidth);

    logOut("[INFO]\tMessage MAP_ROW received: %.*s\n", msgSize, buff);

    return 0;
}

int sockRecvGameUpdate(char *buff) {
    int msgSize = 0;
    enum MsgType msgType;
    int playerCnt;
    char foodCntStr[3];
    int foodCnt;

    /* get message type */
    msgSize += sockRecv(buff, 1);
    msgType = getMsgType(buff);
    if (msgType != GAME_UPDATE) {
        logOut("[ERROR]\tUnexpected message received\n\tExpected: GAME_UPDATE\n\tReceived: %s\n", getMsgTypeStr(msgType));
        exit(1);
    }

    /* get player count */
    msgSize += sockRecv((buff + msgSize), 1);
    playerCnt = buff[1] - '0';
    /* get player coords and points */
    msgSize += sockRecv((buff + msgSize), (9 * playerCnt));

    /* get food count */
    msgSize += sockRecv(foodCntStr, 3);
    foodCnt = strToInt(foodCntStr, 3);
    strncpy((buff + msgSize - 3), foodCntStr, 3);
    /* get food coords */
    msgSize += sockRecv((buff + msgSize), (6 * foodCnt));

    logOut("[INFO]\tMessage GAME_UPDATE received: %.*s\n", msgSize, buff);

    return msgSize;
}