#include "conn.h"

#include <arpa/inet.h>
#include <curses.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

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

int sockSend(char* req) {
    return send(netSock, req, strlen(req), 0);
}

int sockRecv(char* buff, short buffSize) {
    strcpy(buff, "");
    return recv(netSock, buff, buffSize, 0);
}

int sockSendJoinGame(char *uname) {
    char msg[18] = "";
    sprintf(msg, "%c%s", JOIN_GAME, uname);
    return sockSend(msg);
}