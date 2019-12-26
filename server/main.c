#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#define MAX_TYPE_SIZE 1
#define MAX_USERNAME_SIZE 16
#define MAX_PLAYER_COUNT 8

#define R_JOIN_GAME "0"
#define R_MOVE "1"
#define S_LOBBY_INFO "2"
#define S_GAME_START "5"
#define S_MAP_ROW "6"
#define S_GAME_UPDATE "7"
#define S_PLAYER_DEAD "8"
#define S_GAME_END "9"
#define E_GAME_IN_PROGRESS "3"
#define E_USERNAME_TAKEN "4"
#define E_TECHNICAL "A"

int players[MAX_PLAYER_COUNT];
char usernames[MAX_PLAYER_COUNT][MAX_USERNAME_SIZE + 1];
int connectedPlayerCount = 0;

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

int usernameTaken(char *username) {
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i] != 0) {
            if (strcmp(username, usernames[i]) == 0) {
                return 1;
            }
        }
    }

    return 0;
}

void resetPlayer(int socket) {
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i] == socket) {
            players[i] = 0;
            usernames[i][0] = '\0';
            return;
        }
    }
}

int socketSend(int socket, char *message, int messageSize) {
    int ret = send(socket, message, messageSize, MSG_NOSIGNAL);
    if (ret < 0) {
        printf("Client disconnected, removing player\n");
        resetPlayer(socket);
        errno = 0;
    }
    else {
        printf("Message sent ");
        printBytes(message, messageSize);
    }

    return ret;
}

int socketReceive(int socket, char *message, short messageSize) {
    int ret = recv(socket, message, messageSize, 0);
    printf("Message received %s\n", message);
    return ret;
}

void respondWithError(int clientSocket, char *errorType) {
    socketSend(clientSocket, errorType, sizeof(errorType));
    close(clientSocket);
}

int addUsernames(char *buff) {
    int i;
    int size = 0;
    char *ptr = buff;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i] == 0) {
            continue;
        }

        int usernameLength = strlen(usernames[i]);
        strncpy(ptr, usernames[i], usernameLength);

        if (usernameLength == MAX_USERNAME_SIZE) {
            size += usernameLength;
            ptr += usernameLength;
        } else {
            size += usernameLength + 1;
            ptr += usernameLength + 1;
        }
    }

    if (buff[size - 1] == '\0') {
        size -= 1;
    }

    return size;
}

char *addClientToGame(int clientSocket) {
    int i;
    char joinGameRequest[MAX_TYPE_SIZE + MAX_USERNAME_SIZE + 1] = "";
    char requestType[2] = "";

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i] == 0){
            socketReceive(clientSocket, joinGameRequest, sizeof(joinGameRequest));

            if (strlen(joinGameRequest) < 2) {
                printf("Request is not in the correct format\n");
                respondWithError(clientSocket, E_TECHNICAL);
                printf("Cannot add player to game - incorrect join game request size\n");
                return E_TECHNICAL;
            }

            strncpy(requestType, joinGameRequest, 1);

            if (strcmp(requestType, R_JOIN_GAME) != 0) {
                printf("Expected request type %s but was %s\n", R_JOIN_GAME, requestType);
                respondWithError(clientSocket, E_TECHNICAL);
                printf("Cannot add player to game - incorrect join game request type\n");
                return E_TECHNICAL;
            }

            if (usernameTaken(&joinGameRequest[1])) {
                printf("Cannot add player to game - username taken\n");
                return E_USERNAME_TAKEN;
            }

            strcpy(usernames[i], &joinGameRequest[1]);
            players[i] = clientSocket;
            connectedPlayerCount++;

            printf("Player successfully added to game\n");
            return NULL;
        }
    }

    printf("Cannot add player to game - game full\n");
    return E_GAME_IN_PROGRESS;
}

void sendLobbyInfoToAll() {
    int i;
    int actualSize = 2;
    char lobbyInfoMessage[MAX_TYPE_SIZE + 1 + MAX_PLAYER_COUNT * MAX_USERNAME_SIZE + 1] = "";
    sprintf(lobbyInfoMessage, "%s%d", S_LOBBY_INFO, connectedPlayerCount);

    actualSize += addUsernames(&lobbyInfoMessage[2]);

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i] == 0) {
            continue;
        }
        socketSend(players[i], lobbyInfoMessage, actualSize);
    }
}

int main() {
    int netSocket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(7443);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    bind(netSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    listen(netSocket, 30);
    printf("Listening...\n");

    while (1) {
        char *retErrorType = NULL;
        int clientSocket = accept(netSocket, NULL, NULL);
        printf("Client connected\n");

        retErrorType = addClientToGame(clientSocket);
        if (retErrorType == NULL) {
            sendLobbyInfoToAll();
        } else {
            respondWithError(clientSocket, retErrorType);
        }
    }

    return 0;
}
