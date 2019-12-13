#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>

#define MAX_CLIENT_COUNT 12
#define MAX_TYPE_SIZE 2
#define MAX_USERNAME_SIZE 16
#define MAX_PLAYER_COUNT 8

#define R_JOIN_GAME "11"
#define R_MOVE "12"
#define S_LOBBY_INFO "21"
#define S_GAME_START "22"
#define S_GAME_UPDATE "23"
#define S_PLAYER_DEAD "24"
#define S_GAME_END "25"
#define E_GAME_IN_PROGRESS "31"
#define E_USERNAME_TAKEN "32"
#define E_TECHNICAL "33"

int clients[MAX_CLIENT_COUNT];
char usernames[MAX_CLIENT_COUNT][MAX_USERNAME_SIZE + 1];
int connectedClientCount = 0;

int socketSend(int socket, char *message, int messageSize) {
    int ret = send(socket, message, messageSize, 0);
    return ret;
}

int socketReceive(int socket, char *message, short messageSize) {
    int ret = recv(socket, message, messageSize, 0);
    printf("Message %s\n", message);
    return ret;
}

void respondWithError(int clientSocket, char *errorType) {
    socketSend(clientSocket, errorType, sizeof(errorType));
    close(clientSocket);
}

int addClientToList(int clientSocket) {
    int i;
    char joinGameRequest[MAX_TYPE_SIZE + MAX_USERNAME_SIZE + 1] = "";
    char requestType[3] = "";

    for (i = 0; i < MAX_CLIENT_COUNT; i++) {
        if (clients[i] == 0){
            socketReceive(clientSocket, joinGameRequest, sizeof(joinGameRequest));

            if (strlen(joinGameRequest) < 3) {
                printf("Request is not in the correct format\n");
                respondWithError(clientSocket, E_TECHNICAL);
                return 0;
            }

            strncpy(requestType, joinGameRequest, 2);

            if (strcmp(requestType, R_JOIN_GAME) != 0) {
                printf("Expected request type 11 but was %s\n", requestType);
                respondWithError(clientSocket, E_TECHNICAL);
                return 0;
            }

            strcpy(usernames[i], &joinGameRequest[2]);
            clients[i] = clientSocket;
            connectedClientCount++;

            return 1;
        }
    }

    return 0;
}

void notifyLobbyInfo() {
    int i;
    char lobbyInfoMessage[MAX_TYPE_SIZE + 1 + MAX_PLAYER_COUNT * MAX_USERNAME_SIZE + 1] = "";
    sprintf(lobbyInfoMessage, "%s%d", S_LOBBY_INFO, connectedClientCount);

    for (i = 0; i < connectedClientCount; i++) {
        strcat(lobbyInfoMessage, usernames[i]);
    }

    for (i = 0; i < MAX_CLIENT_COUNT; i++) {
        if (clients[i] == 0) {
            return;
        }

        socketSend(clients[i], lobbyInfoMessage, strlen(lobbyInfoMessage));
    }
}

int main() {
    int netSocket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(7443);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    bind(netSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    listen(netSocket, 12);
    printf("Listening...\n");

    while (1) {
        int clientSocket = accept(netSocket, NULL, NULL);
        printf("Client connected\n");

        if (addClientToList(clientSocket)) {
            printf("Client successfully added to list\n");
            notifyLobbyInfo();
        } else {
            printf("Unable to add client, list full\n");
            respondWithError(clientSocket, E_GAME_IN_PROGRESS);
        }
    }

    return 0;
}
