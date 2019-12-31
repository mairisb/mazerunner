#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>

#include "servercfg.h"
#include "utility.h"

#define TYPE_SIZE 1
#define USERNAME_SIZE 16
#define MAX_PLAYER_COUNT 8
#define MAX_MAP_HEIGHT 999
#define MAX_MAP_WIDTH 999
#define MAP_HEIGHT_SIZE 3
#define MAP_WIDTH_SIZE 3

#define X_COORD_SIZE 3
#define Y_COORD_SIZE 3

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

#define JOIN_GAME_MSG_SIZE (TYPE_SIZE + USERNAME_SIZE + 1)
#define LOBBY_INFO_MSG_SIZE (TYPE_SIZE + 1 + MAX_PLAYER_COUNT * USERNAME_SIZE + 1)
#define GAME_START_MSG_SIZE (TYPE_SIZE + 1 + MAX_PLAYER_COUNT * USERNAME_SIZE + MAP_HEIGHT_SIZE + MAP_WIDTH_SIZE + 1)
#define MAP_MSG_SIZE (TYPE_SIZE + MAP_WIDTH_SIZE + MAX_MAP_WIDTH + 1)

int mapWidth = 0;
int mapHeight = 0;
char mapState[MAX_MAP_HEIGHT][MAX_MAP_WIDTH + 2];
int players[MAX_PLAYER_COUNT];
char usernames[MAX_PLAYER_COUNT][USERNAME_SIZE + 1];
int connectedPlayerCount = 0;
int gameStarted = 0;

void printMap(struct ServerCfg *serverCfg) {
    int i;
    for (i = 0; i < mapHeight; i++) {
        printf("%s\n", mapState[i]);
    }
}

void loadMap(struct ServerCfg *serverCfg) {
    int i;
    FILE *mapFile = NULL;

    mapFile = fopen(serverCfg->mapFile, "r");
    if (mapFile == NULL) {
        printf("Could not open map file '%s'", serverCfg->mapFile);
        perror("");
        exit(1);
    }

    for (i = 0; i < MAX_MAP_HEIGHT; i++) {
        int rowLength;
        if (getLine(mapState[i], sizeof(mapState[i]), mapFile) == NULL) {
            if (mapWidth == 0) {
                printf("A map can not be empty\n");
                exit(1);
            }
            mapHeight = i;
            break;
        }

        rowLength = strlen(mapState[i]);
        if (mapWidth == 0) {
            mapWidth = rowLength;
        } else if (rowLength != mapWidth) {
            printf("A map can not contain different width rows\n");
            exit(1);
        }

    }

    printMap(serverCfg);
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

void sendToAll(char *message, int size) {
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i] == 0) {
            continue;
        }
        socketSend(players[i], message, size);
    }
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

        if (usernameLength == USERNAME_SIZE) {
            size += usernameLength;
            ptr += usernameLength;
        } else {
            size += usernameLength + 1;
            ptr += usernameLength + 1;
        }
    }

    return size;
}

char *addClientToGame(int clientSocket) {
    int i;
    char joinGameRequest[JOIN_GAME_MSG_SIZE] = "";
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
    int actualSize = 2;
    char lobbyInfoMessage[LOBBY_INFO_MSG_SIZE] = "";
    sprintf(lobbyInfoMessage, "%s%d", S_LOBBY_INFO, connectedPlayerCount);

    actualSize += addUsernames(&lobbyInfoMessage[2]);

    sendToAll(lobbyInfoMessage, actualSize);
}

void sendGameStartMessage() {
    int actualSize = TYPE_SIZE + 1;
    char gameStartMessage[GAME_START_MSG_SIZE] = "";
    sprintf(gameStartMessage, "%s%d", S_GAME_START, connectedPlayerCount);

    actualSize += addUsernames(&gameStartMessage[2]);

    sprintf(&gameStartMessage[actualSize], "%03d%03d", mapWidth, mapHeight);
    actualSize += MAP_WIDTH_SIZE + MAP_HEIGHT_SIZE;

    sendToAll(gameStartMessage, actualSize);
}

void sendMap() {
    int i;
    int actualSize = TYPE_SIZE + MAP_WIDTH_SIZE + mapWidth;
    for (i = 0; i < mapHeight; i++) {
        char mapMessage[MAP_MSG_SIZE] = "";
        sprintf(mapMessage, "%s%03d%s", S_MAP_ROW, i + 1, mapState[i]);
        sendToAll(mapMessage, actualSize);
    }
}

void startGame() {
    gameStarted = 1;
    sendGameStartMessage();
    sendMap();
}

int main() {
    struct ServerCfg serverCfg;
    struct sockaddr_in serverAddress;
    int netSocket;

    getCfg(&serverCfg);
    loadMap(&serverCfg);

    netSocket = socket(AF_INET, SOCK_STREAM, 0);

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverCfg.serverPort);
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
            if (connectedPlayerCount == MAX_PLAYER_COUNT) {
                startGame();
            }
        } else {
            respondWithError(clientSocket, retErrorType);
        }
    }

    return 0;
}
