#define _XOPEN_SOURCE 500
#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

#include "libs/servercfg.h"
#include "libs/utility.h"

#define TYPE_SIZE 1
#define USERNAME_SIZE 16
#define MAX_PLAYER_COUNT 8
#define MAX_MAP_HEIGHT 999
#define MAX_MAP_WIDTH 999
#define MAP_HEIGHT_SIZE 3
#define MAP_WIDTH_SIZE 3
#define X_COORD_SIZE 3
#define Y_COORD_SIZE 3
#define MOVE_SIZE 1

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
#define MOVE_MSG_SIZE (TYPE_SIZE + MOVE_SIZE + 1)

int netSocket;
int connectedPlayerCount = 0;
int players[MAX_PLAYER_COUNT];
char usernames[MAX_PLAYER_COUNT][USERNAME_SIZE + 1];
int startRowPositions[MAX_PLAYER_COUNT];
int startColumnPositions[MAX_PLAYER_COUNT];

int mapWidth = 0;
int mapHeight = 0;
char mapState[MAX_MAP_HEIGHT][MAX_MAP_WIDTH + 2];

int gameStarted = 0;
pthread_t moveResolverThread;
pthread_t moveSetterThread;

void exitHandler(int sig) {
    int i;
    printf("Entered exit handler\n");
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i] != 0) {
            close(players[i]);
        }
    }
    close(netSocket);

    exit(sig);
}

void printMap() {
    int i;
    printf("Map height: %d\n", mapHeight);
    printf("Map width: %d\n", mapWidth);
    for (i = 0; i < mapHeight; i++) {
        printf("%s\n", mapState[i]);
    }
}

void checkAndSetSpawnPosition(char *mapRow, int rowLength, int rowPosition) {
    int i;
    for (i = 0; i < rowLength; i++) {
        int cellValue = (int) mapRow[i];
        if (cellValue >= 65 && cellValue <= 72) {
            int index = cellValue - 65;
            startRowPositions[index] = rowPosition;
            startColumnPositions[index] = i;
            mapRow[i] = ' ';
        }
    }
}

void loadMap() {
    int i;
    FILE *mapFile = NULL;

    mapFile = fopen(cfg.mapFilename, "r");
    if (mapFile == NULL) {
        fprintf(stderr, "Could not open map file '%s'", cfg.mapFilename);
        perror("");
        fclose(mapFile);
        exitHandler(1);
    }

    for (i = 0; i < MAX_MAP_HEIGHT; i++) {
        int rowLength;
        if (getLine(mapState[i], sizeof(mapState[i]), mapFile) == NULL) {
            if (mapWidth == 0) {
                printf("A map can not be empty\n");
                fclose(mapFile);
                exitHandler(1);
            }
            mapHeight = i;
            break;
        }

        rowLength = strlen(mapState[i]);
        if (mapWidth == 0) {
            mapWidth = rowLength;
        } else if (rowLength != mapWidth) {
            printf("A map can not contain different width rows\n");
            fclose(mapFile);
            exitHandler(1);
        }

        checkAndSetSpawnPosition(mapState[i], rowLength, i);
    }

    fclose(mapFile);
    printMap();
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

    close(socket);
}

int socketSend(int socket, char *message, int messageSize) {
    int sentBytes = 0;
    int ret;
    while (sentBytes < messageSize) {
        ret = send(socket, message, messageSize, MSG_NOSIGNAL);
        if (ret <= 0) {
            printf("Client disconnected, removing player\n");
            resetPlayer(socket);
            errno = 0;
        } else {
            sentBytes += ret;
            printf("Sent %d bytes\n", ret);
        }
    }
    printf("Message sent ");
    printBytes(message, messageSize);

    return ret;
}

int socketReceive(int socket, char *message, int messageSize) {
    int receivedBytes = 0;
    int ret;
    while (receivedBytes < messageSize) {
        ret = recv(socket, message, messageSize, 0);
        if (ret < 0) {
            printf("Client disconnected, removing player\n");
            resetPlayer(socket);
            errno = 0;
        } else if (ret == 0) {
            printf("Message was shorter than passed size\n");
            break;
        } else {
            receivedBytes += ret;
            printf("Received %d bytes\n", ret);
        }
    }
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

int getRandomFreePosition() {
    int freePositions[MAX_PLAYER_COUNT];
    int freePositionCount = 0;
    int randomPosition;
    int lastIndex = 0;
    int i;

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i] == 0) {
            freePositions[lastIndex] = i;
            lastIndex++;
            freePositionCount++;
        }
    }

    if (freePositionCount == 0) {
        return -1;
    }

    randomPosition = rand() % freePositionCount;
    return freePositions[randomPosition];
}

char *addClientToGame(int clientSocket) {
    char joinGameRequest[JOIN_GAME_MSG_SIZE] = "";
    char requestType[2] = "";

    int playerPosition = getRandomFreePosition();
    if (playerPosition < 0) {
        printf("Cannot add player to game - game full\n");
        return E_GAME_IN_PROGRESS;
    }

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

    strcpy(usernames[playerPosition], &joinGameRequest[1]);
    players[playerPosition] = clientSocket;
    connectedPlayerCount++;

    printf("Player successfully added to game\n");
    return NULL;
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

void *setIncomingMoves(void *args) {
    printf("Not implemented yet\n");

    return NULL;
}

void resolveIncomingMoves() {
    while(1) {
        usleep(100000);
        printf("Resolving moves\n");
        printf("Not implemented yet\n");
    }
}

void *handleGameStart(void *args) {
    int ret;
    sendGameStartMessage();
    sendMap();

    ret = pthread_create(&moveSetterThread, NULL, setIncomingMoves, NULL);
    if (ret != 0) {
        perror("Failed to create moveSetterThread");
        exitHandler(1);
    }

    resolveIncomingMoves();
    return NULL;
}

void startGame() {
    int ret;
    gameStarted = 1;

    ret = pthread_create(&moveResolverThread, NULL, handleGameStart, NULL);
    if (ret != 0) {
        fprintf(stderr, "Failed to create moveResolverThread");
        perror("");
        exitHandler(1);
    }
}

int main() {
    struct sockaddr_in serverAddress;

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = exitHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    srand(time(NULL)); /* Initialization for setting players random positions later. Should only be called once. */

    loadCfg();
    loadMap();

    netSocket = socket(AF_INET, SOCK_STREAM, 0);

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(cfg.serverPort);
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
            close(clientSocket);
        }
    }

    close(netSocket);

    return 0;
}
