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