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
#include <poll.h>

#include "libs/servercfg.h"
#include "libs/utility.h"

#define TYPE_SIZE 1
#define USERNAME_SIZE 16
#define MAX_PLAYER_COUNT 8
#define PLAYER_COUNT_SIZE 1
#define MAX_MAP_HEIGHT 999
#define MAX_MAP_WIDTH 999
#define MAP_HEIGHT_SIZE 3
#define MAP_WIDTH_SIZE 3
#define X_COORD_SIZE 3
#define Y_COORD_SIZE 3
#define MOVE_SIZE 1
#define MAX_FOOD_COUNT 999
#define FOOD_SIZE 3

#define R_JOIN_GAME '0'
#define R_MOVE '1'
#define S_LOBBY_INFO '2'
#define S_GAME_START '5'
#define S_MAP_ROW '6'
#define S_GAME_UPDATE '7'
#define S_PLAYER_DEAD '8'
#define S_GAME_END '9'

#define E_GAME_IN_PROGRESS "3"
#define E_USERNAME_TAKEN "4"
#define E_TECHNICAL "A"

#define JOIN_GAME_MSG_SIZE (TYPE_SIZE + USERNAME_SIZE + 1)
#define LOBBY_INFO_MSG_SIZE (TYPE_SIZE + 1 + MAX_PLAYER_COUNT * USERNAME_SIZE + 1)
#define GAME_START_MSG_SIZE (TYPE_SIZE + 1 + MAX_PLAYER_COUNT * USERNAME_SIZE + MAP_HEIGHT_SIZE + MAP_WIDTH_SIZE + 1)
#define MAP_MSG_SIZE (TYPE_SIZE + MAP_WIDTH_SIZE + MAX_MAP_WIDTH + 1)
#define MOVE_MSG_SIZE (TYPE_SIZE + MOVE_SIZE + 1)
#define GAME_UPDATE_MSG_SIZE (TYPE_SIZE + 1 + MAX_PLAYER_COUNT * 9 + 2 + MAX_FOOD_COUNT * 6 + 1)

struct Position {
    int rowPosition;
    int columnPosition;
};

struct PlayerData {
    int socket;
    char username[USERNAME_SIZE + 1];
    struct Position position;
    int points;
    int deathStatus;
    int requestedMove;
};

int netSocket;

int connectedPlayerCount = 0;
struct Position startPositions[MAX_PLAYER_COUNT];
struct PlayerData players[MAX_PLAYER_COUNT];

int mapWidth = 0;
int mapHeight = 0;
char mapState[MAX_MAP_HEIGHT][MAX_MAP_WIDTH + 2];

int gameStarted = 0;
pthread_t moveResolverThread;
pthread_t moveSetterThread;
pthread_mutex_t lock;

void exitHandler(int sig);
void resetPlayer(int socket);
int socketSend(int socket, char *message, int messageSize);
int socketReceive(int socket, char *buff, int messageSize);
void sendToAll(char *message, int size);
void respondWithError(int clientSocket, char *errorType);
int addUsernames(char *buff);
void sendLobbyInfoMessage();
void sendGameStartMessage();
void sendMap();
int addFoodData(char *buff);
int addPositionsAndPoints(char *buff);
void sendGameUpdateMessage();
void resolveIncomingMoves();
void readAndSetMove(int playerIndex);
void *setIncomingMoves(void *args); /* Thread function */
void *handleGameStart(void *args); /* Thread function */
void startGame();
int usernameTaken(char *username);
int getRandomFreePosition();
char *addClientToGame(int clientSocket);
void checkAndSetSpawnPosition(char *mapRow, int rowLength, int rowPosition);
void printMap();
void loadMap();

void exitHandler(int sig) {
    int i;
    printf("Entered exit handler\n");
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].socket != 0) {
            close(players[i].socket);
        }
    }
    close(netSocket);

    pthread_mutex_destroy(&lock);

    exit(sig);
}

void resetPlayer(int socket) {
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].socket == socket) {
            players[i].socket = 0;
            players[i].username[0] = '\0';
            return;
        }
    }

    shutdown(socket, SHUT_RDWR);
    close(socket);
}

int socketSend(int socket, char *message, int messageSize) {
    int sentBytes = 0;
    int sentTotal = 0;

    while (sentTotal < messageSize) {
        sentBytes = send(socket, &message[sentTotal], messageSize - sentTotal, MSG_NOSIGNAL);
        if (sentBytes < 0) {
            perror("Error sending to socket: ");
            printf("Removing player\n");
            resetPlayer(socket);
            errno = 0;
            return -1;
        } else if (sentBytes == 0) {
            printf("Client disconnected, removing player\n");
            resetPlayer(socket);
            return -1;
        } else {
            sentTotal += sentBytes;
        }
    }

    printf("Message sent ");
    printBytes(message, messageSize);
    return 0;
}

int socketReceive(int socket, char *buff, int messageSize) {
    int recBytes = 0;
    int recTotal = 0;

    while (recTotal < messageSize) {
        recBytes = recv(socket, &buff[recTotal], messageSize - recTotal, 0);
        if (recBytes < 0) {
            perror("Error reading socket: ");
            printf("Removing player\n");
            resetPlayer(socket);
            errno = 0;
            return -1;
        } else if (recBytes == 0) {
            printf("Client disconnected, removing player\n");
            resetPlayer(socket);
            return -1;
        } else {
            recTotal += recBytes;
        }
    }

    printf("Message received %s\n", buff);
    return 0;
}

void sendToAll(char *message, int size) {
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].socket == 0) {
            continue;
        }
        socketSend(players[i].socket, message, size);
    }
}

void respondWithError(int clientSocket, char *errorType) {
    socketSend(clientSocket, errorType, sizeof(errorType));
    close(clientSocket);
}

int addUsernames(char *buff) {
    int size = 0;
    char *ptr = buff;
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].socket == 0) {
            continue;
        }

        strcat(ptr, players[i].username);
        ptr += USERNAME_SIZE;
        size += USERNAME_SIZE;
    }

    return size;
}

void sendLobbyInfoMessage() {
    int actualSize = 0;
    char lobbyInfoMessage[LOBBY_INFO_MSG_SIZE] = "";

    actualSize += sprintf(lobbyInfoMessage, "%c%d", S_LOBBY_INFO, connectedPlayerCount);
    actualSize += addUsernames(&lobbyInfoMessage[actualSize]);

    sendToAll(lobbyInfoMessage, actualSize);
}

void sendGameStartMessage() {
    int actualSize = 0;
    char gameStartMessage[GAME_START_MSG_SIZE] = "";

    actualSize += sprintf(gameStartMessage, "%c%d", S_GAME_START, connectedPlayerCount);
    actualSize += addUsernames(&gameStartMessage[actualSize]);
    actualSize += sprintf(&gameStartMessage[actualSize], "%03d%03d", mapWidth, mapHeight);

    sendToAll(gameStartMessage, actualSize);
}

void sendMap() {
    int actualSize = 4 + mapWidth;
    int i;
    for (i = 0; i < mapHeight; i++) {
        char mapMessage[MAP_MSG_SIZE] = "";
        sprintf(mapMessage, "%c%03d%s", S_MAP_ROW, i + 1, mapState[i]);
        sendToAll(mapMessage, actualSize);
    }
}

int addFoodData(char *buff) {
    sprintf(buff, "%03d", 0);

    return 1;
}

int addPositionsAndPoints(char *buff) {
    int size = 0;
    int i;
    char *ptr = buff;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        struct PlayerData *player = &players[i];
        if (player->socket == 0) {
            continue;
        }

        sprintf(ptr, "%03d%03d%03d", player->position.columnPosition, player->position.rowPosition, player->points);
        ptr += 9;
        size += 9;
    }

    return size;
}

void sendGameUpdateMessage() {
    int actualSize = 0;
    char gameUpdateMessage[GAME_UPDATE_MSG_SIZE] = "";

    actualSize += sprintf(gameUpdateMessage, "%c%d", S_GAME_UPDATE, connectedPlayerCount);
    actualSize += addPositionsAndPoints(&gameUpdateMessage[actualSize]);
    actualSize += addFoodData(&gameUpdateMessage[actualSize]);

    sendToAll(gameUpdateMessage, actualSize);
}

void resolveIncomingMoves() {
    int i;
    while(1) {
        usleep(100000);
        pthread_mutex_lock(&lock);

        for (i = 0; i < MAX_PLAYER_COUNT; i++) {
            struct PlayerData *player = &players[i];
            struct Position *playerPosition = &player->position;

            if (player->requestedMove == 0) {
                continue;
            }

            switch (player->requestedMove) {
                case 1:
                    if (playerPosition->rowPosition != 0) {
                        char targetPosition = mapState[playerPosition->rowPosition - 1][playerPosition->columnPosition];
                        if (targetPosition == ' ') {
                            playerPosition->rowPosition -= 1;
                        } else {
                            printf("Tagret position is in wall, ignore\n");
                        }
                    }
                    break;
                case 2:
                    if (playerPosition->rowPosition != mapHeight - 1) {
                        char targetPosition = mapState[playerPosition->rowPosition + 1][playerPosition->columnPosition];
                        if (targetPosition == ' ') {
                            playerPosition->rowPosition += 1;
                        } else {
                            printf("Tagret position is in wall, ignore\n");
                        }
                    }
                    break;
                case 3:
                    if (playerPosition->columnPosition != mapWidth - 1) {
                        char targetPosition = mapState[playerPosition->rowPosition][playerPosition->columnPosition + 1];
                        if (targetPosition == ' ') {
                            playerPosition->columnPosition += 1;
                        } else {
                            printf("Tagret position is in wall, ignore\n");
                        }
                    }
                    break;
                case 4:
                    if (playerPosition->columnPosition != 0) {
                        char targetPosition = mapState[playerPosition->rowPosition][playerPosition->columnPosition - 1];
                        if (targetPosition == ' ') {
                            playerPosition->columnPosition -= 1;
                        } else {
                            printf("Tagret position is in wall, ignore\n");
                        }
                    }
                    break;
            }
        }

        pthread_mutex_unlock(&lock);
        sendGameUpdateMessage();
    }
}

void readAndSetMove(int playerIndex) {
    char moveRequest[MOVE_MSG_SIZE] = "";
    char requestType;
    int moveType;
    struct PlayerData *player = &players[playerIndex];

    socketReceive(player->socket, moveRequest, sizeof(moveRequest) - 1);
    if (strlen(moveRequest) != 2) {
        printf("Request is not in the correct length\n");
        printf("Ignoring player move\n");
        respondWithError(player->socket, E_TECHNICAL);
        return;
    }

    requestType = moveRequest[0];
    moveType = ((int) moveRequest[1]) - 64;

    if (requestType != R_MOVE) {
        printf("Request type %c was not expected\n", requestType);
        printf("Ignoring player move\n");
        respondWithError(player->socket, E_TECHNICAL);
        return;
    }

    pthread_mutex_lock(&lock);

    if (moveType >= 0 && moveType < 4) {
        player->requestedMove = moveType;
    } else {
        printf("Requested move is not defined\n");
        printf("Ignoring player move\n");
        respondWithError(player->socket, E_TECHNICAL);
    }

    pthread_mutex_unlock(&lock);
}

void *setIncomingMoves(void *args) {
    struct pollfd pollList[MAX_PLAYER_COUNT];
    int ret;
    int i;

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        pollList[i].fd = players[i].socket;
        pollList[i].events = POLLIN;
    }

    while(1) {
        ret = poll(pollList, MAX_PLAYER_COUNT, -1);

        if(ret < 0) {
            fprintf(stderr,"Error while polling: %s\n", strerror(errno));
            return NULL;
        }

        /*if (((pollList[0].revents & POLLHUP) == POLLHUP) || ((pollList[0].revents&POLLERR) == POLLERR) ||
           ((pollList[0].revents&POLLNVAL) == POLLNVAL) || ((pollList[1].revents&POLLHUP) == POLLHUP) ||
           ((pollList[1].revents&POLLERR) == POLLERR) || ((pollList[1].revents&POLLNVAL) == POLLNVAL)) {

            return 0;
        }*/

        for (i = 0; i < MAX_PLAYER_COUNT; i++) {
            if ((pollList[i].revents & POLLIN) == POLLIN) {
                readAndSetMove(i);
            }
        }
    }

    return NULL;
}

void *handleGameStart(void *args) {
    int ret;
    int i;

    /* Set the initial positions for all players */
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        players[i].position.rowPosition = startPositions[i].rowPosition;
        players[i].position.columnPosition = startPositions[i].columnPosition;
    }

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

int usernameTaken(char *username) {
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].socket != 0) {
            if (strcmp(username, players[i].username) == 0) {
                return 1;
            }
        }
    }

    return 0;
}

int getRandomFreePosition() {
    int freePositions[MAX_PLAYER_COUNT];
    int freePositionCount = 0;
    int randomPosition;
    int lastIndex = 0;
    int i;

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].socket == 0) {
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
    char requestType;

    int playerPosition = getRandomFreePosition();
    if (playerPosition < 0) {
        printf("Cannot add player to game - game full\n");
        return E_GAME_IN_PROGRESS;
    }

    socketReceive(clientSocket, joinGameRequest, sizeof(joinGameRequest) - 1);

    if (strlen(joinGameRequest) < 2) {
        printf("Request is not in the correct format\n");
        respondWithError(clientSocket, E_TECHNICAL);
        printf("Cannot add player to game - incorrect join game request size\n");
        return E_TECHNICAL;
    }

    requestType = joinGameRequest[0];

    if (requestType != R_JOIN_GAME) {
        printf("Expected request type %c but was %c\n", R_JOIN_GAME, requestType);
        respondWithError(clientSocket, E_TECHNICAL);
        printf("Cannot add player to game - incorrect join game request type\n");
        return E_TECHNICAL;
    }

    if (usernameTaken(&joinGameRequest[1])) {
        printf("Cannot add player to game - username taken\n");
        return E_USERNAME_TAKEN;
    }

    strcpy(players[playerPosition].username, &joinGameRequest[1]);
    players[playerPosition].socket = clientSocket;
    connectedPlayerCount++;

    printf("Player successfully added to game\n");
    return NULL;
}

void checkAndSetSpawnPosition(char *mapRow, int rowLength, int rowPosition) {
    int i;
    for (i = 0; i < rowLength; i++) {
        int cellValue = (int) mapRow[i];
        if (cellValue >= 65 && cellValue <= 72) {
            int index = cellValue - 65;
            startPositions[index].rowPosition = rowPosition;
            startPositions[index].columnPosition = i;
            mapRow[i] = ' ';
        }
    }
}

void printMap() {
    int i;
    printf("Map height: %d\n", mapHeight);
    printf("Map width: %d\n", mapWidth);
    for (i = 0; i < mapHeight; i++) {
        printf("%s\n", mapState[i]);
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

int main() {
    struct sockaddr_in serverAddress;

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = exitHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    int enable = 1;

    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("mutex init has failed\n");
        exit(1);
    }
    srand(time(NULL)); /* Initialization for setting players random positions later. Should only be called once. */

    loadCfg();
    loadMap();

    netSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (netSocket < 0) {
        perror("Error opening socket: ");
        exit(1);
    }
    if (setsockopt(netSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("Error on setsockopt(SO_REUSEADDR): ");
        exit(1);
    }

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
            sendLobbyInfoMessage();
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
