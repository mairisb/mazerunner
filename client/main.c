/**
 * Command for compiling and running client:
 */
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <arpa/inet.h>

#include <string.h>

#define NICKNAME_SIZE (16 + 1)
#define BUFF_SIZE (1024)

/**
 * Create a Socket for server communication
 */
int socketCreate() {
    return socket(AF_INET, SOCK_STREAM, 0);
}

/**
 * Connect socket to the server
 */
void socketConnect(int socket, char *ipAddress, int port) {
    struct sockaddr_in remote;
    remote.sin_addr.s_addr = inet_addr(ipAddress);
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    printf("Attempting to connect...\n");
    if (connect(socket, (struct sockaddr *) &remote, sizeof(struct sockaddr_in)) < 0) {
        perror("Error connecting to server");
        exit(1);
    }
    printf("Connection established!\n");
}

/**
 * Send data to the server
 */
int socketSend(int socket, char* request, short requestLen, long timeoutSec) {
    int retVal;
    struct timeval tv;
    tv.tv_sec = timeoutSec;
    tv.tv_usec = 0;
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (char *) &tv, sizeof(tv)) < 0) {
        perror("Error setting 'send timeout' socket option");
        exit(1);
    }
    retVal = send(socket, request, requestLen, 0);
    return retVal;
}

/**
 * Receive data from the server
 */
int socketReceive(int socket, char* response, short responseLen, long timeoutSec) {
    int retVal;
    struct timeval tv;
    tv.tv_sec = timeoutSec;
    tv.tv_usec = 0;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(tv)) < 0) {
        printf("Time Out\n");
        return -1;
        perror("Error setting 'receive timeout' socket option");
        exit(1);
    }
    retVal = recv(socket, response, responseLen, 0);
    return retVal;
}

int main(int argc, char** argv) {
    int netSock;
    char buff[BUFF_SIZE] = "11porkchop\0";

    netSock = socketCreate();
    socketConnect(netSock, "109.110.2.39", 7443);

    printf("Attempting to join game...\n");
    /*
    send(netSock, buff, sizeof(buff), 0);
    */
    socketSend(netSock, buff, sizeof(buff), 20);

    strcpy(buff, "");
    recv(netSock, &buff, sizeof(buff), 0);
    printf("Message from server: %s\n", buff);

    close(netSock);

    return 0;
}
