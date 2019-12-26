#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define CONFIG_FILENAME "client.cfg"
#define NICKNAME_SIZE (16 + 1)
#define BUFF_SIZE 1024

/* Configuration variables */
char serverIp[16];
int serverPort;

char *getLine(char *buffer, int bufferMaxSize, FILE *stream) {
    char *result;
    int bufferSize;

    result = fgets(buffer, bufferMaxSize, stream);

    if (result == NULL) {
        return result;
    }

    bufferSize = strlen(buffer);
    if (buffer[bufferSize - 1] == '\n') {
        buffer[bufferSize - 1] = 0;

        if (buffer[bufferSize - 2] == '\r') {
            buffer[bufferSize - 2] = 0;
        }
    }

    return buffer;
}

void readConfig() {
    char buffer[BUFF_SIZE];
    char keyBuffer[BUFF_SIZE];
    char valueBuffer[BUFF_SIZE];
    FILE *configFile;

    configFile = fopen(CONFIG_FILENAME, "r");
    if (configFile == NULL) {
        printf("Could not open configuration file '%s'", CONFIG_FILENAME);
        perror("");
        exit(1);
    }

    while (getLine(buffer, 1024, configFile) != NULL) {
        sscanf(buffer, "%s = %s", keyBuffer, valueBuffer);
        if (strcmp(keyBuffer, "server_ip") == 0) {
            strcpy(serverIp, valueBuffer);
        } else if (strcmp(keyBuffer, "server_port") == 0) {
            sscanf(valueBuffer, "%d", &serverPort);
        }
    }

    printf("Client's configuration:\n");
    printf("\tserver_ip = %s\n", serverIp);
    printf("\tserver_port = %d\n", serverPort);
}

int socketCreate() {
    return socket(AF_INET, SOCK_STREAM, 0);
}

void socketConnect(int socket, char *ipAddress, int port) {
    struct sockaddr_in remote;
    remote.sin_addr.s_addr = inet_addr(ipAddress);
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    printf("Attempting to connect to the server...\n");
    if (connect(socket, (struct sockaddr *) &remote, sizeof(struct sockaddr_in)) < 0) {
        perror("Error connecting to server");
        exit(1);
    }
    printf("Connection established!\n");
}

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

void setJoinGameMessage(char *buff, char *nickname) {
    sprintf(buff, "0%s", nickname);
}

int main(int argc, char** argv) {
    int netSock;
    char nickname[NICKNAME_SIZE];
    char buff[BUFF_SIZE];

    readConfig();

    netSock = socketCreate();
    socketConnect(netSock, serverIp, serverPort);

    printf("Please enter a nickname: ");
    fgets(nickname, sizeof(nickname), stdin);
    setJoinGameMessage(buff, nickname);

    printf("Attempting to join game...\n");
    socketSend(netSock, buff, sizeof(buff), 20);

    strcpy(buff, "");
    recv(netSock, &buff, sizeof(buff), 0);
    printf("Message from server: %s\n", buff);

    close(netSock);

    return 0;
}
