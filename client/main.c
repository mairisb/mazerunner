#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_TYPE_SIZE 1
#define MAX_PLAYER_COUNT 8
#define MAX_USERNAME_SIZE 16
#define CONFIG_FILENAME "client.cfg"
#define BUFF_SIZE 1024

enum MsgType {
    NO_MESSAGE = 'N',
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

char getMesssageType(char *message) {
    return message[0];
}

/* Configuration variables */
char serverIp[16];
int serverPort;

void printBytes(char *buff, int size) {
    int i;
    for (i = 0; i < size; i++) {
        if (buff[i] == '\0') {
            printf("\\0");
        } else {
            printf("%c", buff[i]);
        }
    }
    printf("\n");
}

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

int socketSend(int socket, char* request) {
    return send(socket, request, strlen(request), 0);
}

int socketReceive(int socket, char* buffer, short bufferSize) {
    strcpy(buffer, "");
    return recv(socket, buffer, bufferSize, 0);
}

int socketSendJoinGame(int socket, char *username) {
    char message[18] = "";
    sprintf(message, "%c%s", JOIN_GAME, username);
    return socketSend(socket, message);
}

int main(int argc, char** argv) {
    int netSock;
    char username[MAX_USERNAME_SIZE + 1];
    char buff[BUFF_SIZE];
    enum MsgType msgType;

    readConfig();

    netSock = socketCreate();
    socketConnect(netSock, serverIp, serverPort);

    printf("Enter username: ");
    getLine(username, sizeof(username), stdin);

    do {
        if (msgType == GAME_IN_PROGRESS) {
            sleep(3);
        }

        printf("Attempting to join game...\n");
        socketSendJoinGame(netSock, username);

        socketReceive(netSock, buff, sizeof(buff));

        msgType = getMesssageType(buff);
        switch(msgType) {
            case LOBBY_INFO:
                printf("Joined game.\n");
                break;
            case GAME_IN_PROGRESS:
                printf("Game already in progress. Will try to join again.\n");
                break;
            case USERNAME_TAKEN:
                printf("Username %s already taken.\nEnter new username to join again: ", username);
                getLine(username, sizeof(username), stdin);
                break;
            default:
                printf("Error: received unexpected message\n");
                exit(1);
        }
    } while (msgType != LOBBY_INFO);

    /* Handle incoming LOBBY_INFO messages */

    printf("Exit");

    close(netSock);

    return 0;
}
