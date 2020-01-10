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
#include <sys/queue.h>

#include "libs/servercfg.h"
#include "libs/utility.h"
#include "libs/move_queue.h"

#define TYPE_SIZE 1
#define USERNAME_SIZE 16
#define MAX_PLAYER_COUNT 2
#define PLAYER_COUNT_SIZE 1
#define MAX_MAP_HEIGHT 999
#define MAX_MAP_WIDTH 999
#define MAP_HEIGHT_SIZE 3
#define MAP_WIDTH_SIZE 3
#define X_COORD_SIZE 3
#define Y_COORD_SIZE 3
#define MOVE_SIZE 1
#define FOOD_SIZE 3
#define MAX_FOOD_COUNT 999
#define POINT_SIZE 3

#define R_JOIN_GAME '0'
#define R_MOVE '1'
#define S_LOBBY_INFO '2'
#define S_GAME_START '5'
#define S_MAP_ROW '6'
#define S_GAME_UPDATE '7'
#define S_PLAYER_DEAD '8'
#define S_GAME_END '9'
#define E_GAME_IN_PROGRESS '3'
#define E_USERNAME_TAKEN '4'
#define E_TECHNICAL 'A'

#define JOIN_GAME_MSG_SIZE (TYPE_SIZE + USERNAME_SIZE + 1)
#define LOBBY_INFO_MSG_SIZE (TYPE_SIZE + 1 + MAX_PLAYER_COUNT * USERNAME_SIZE + 1)
#define GAME_START_MSG_SIZE (TYPE_SIZE + 1 + MAX_PLAYER_COUNT * USERNAME_SIZE + MAP_HEIGHT_SIZE + MAP_WIDTH_SIZE + 1)
#define MAP_MSG_SIZE (TYPE_SIZE + MAP_WIDTH_SIZE + MAX_MAP_WIDTH + 1)
#define MOVE_MSG_SIZE (TYPE_SIZE + MOVE_SIZE + 1)
#define GAME_UPDATE_MSG_SIZE (TYPE_SIZE + 1 + MAX_PLAYER_COUNT * 9 + 2 + MAX_FOOD_COUNT * 6 + 1)

#define THREAD_RUNNING 0
#define THREAD_COMPLETED 1
#define THREAD_ERRORED 2

struct Position {
    int rowPosition;
    int columnPosition;
};

struct PlayerData {
    int socket;
    char username[USERNAME_SIZE + 1];
    struct Position position;
    int points;
    int requestedMove;
};

struct MapData {
    char **map;
    int currentfoodCount;
    struct Position *foodPositions;
    struct Position *startPositions;
};

int netSocket;
struct MapData mapData = {};
struct PlayerData players[MAX_PLAYER_COUNT];
int connectedPlayerCount = 0;
int gameStarted = 0;

pthread_t moveResolverThread;
pthread_t moveSetterThread;
pthread_mutex_t lock;

void exitHandler(int sig);
void handleDisconnect(int socket);
int socketSend(int socket, char *message, int messageSize);
int socketReceive(int socket, char *buff, int messageSize);
void sendToAll(char *message, int size);
void respondWithError(int clientSocket, char errorType);
int gameEnded();
int addFoodData(char *buff);
int addPositionsAndPoints(char *buff);
void sendGameUpdateMessage();
void generateFood();
void resetOneFood(int targetRow, int targetCol);
void resolveIncomingMoves();
void readAndSetMove(int playerIndex);
void *setIncomingMoves(void *args); /* Thread function */
void *handleGameStart(void *args); /* Thread function */
void sendMap();
int addUsernames(char *buff);
void sendGameStartMessage();
void startGame();
int usernameTaken(char *username);
int getRandomFreePosition();
void sendLobbyInfoMessage();
int addClientToGame(int clientSocket);
void loadPlayerData();
void printMap();
void checkAndSetSpawnPositions(char *mapRow, int rowPosition);
void allocateMap();
void loadMapData();
void init();

void printStartPositions() {
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        printf("Player %d start pos:\n", i);
        printf("  x: %d\n", mapData.startPositions[i].columnPosition);
        printf("  y: %d\n", mapData.startPositions[i].rowPosition);
    }
}

void printPlayerPositions() {
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        printf("Player %d start pos:\n", i);
        printf("  x: %d\n", players[i].position.columnPosition);
        printf("  y: %d\n", players[i].position.rowPosition);
    }
}

void printSockets() {
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        printf("Player %d socket:\n", i);
        printf("  %d\n", players[i].socket);
    }
}

void exitHandler(int sig) {
    int i;
    printf("Entered exit handler\n");

    pthread_mutex_destroy(&lock);

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].socket > 0) {
            close(players[i].socket);
        }
    }

    freeMoveQueue();

    if (netSocket > 0) {
        close(netSocket);
    }

    /* Free map */
    if (mapData.map != NULL) {
        for (i = 0; i < cfg.mapHeight; i++) {
            if (mapData.map[i] != NULL) {
                free(mapData.map[i]);
            }
        }
        free(mapData.map);
    }
    if (mapData.foodPositions != NULL) {
        free(mapData.foodPositions);
    }
    if (mapData.startPositions != NULL) {
        free(mapData.startPositions);
    }

    exit(sig);
}

void handleDisconnect(int socket) {
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].socket == socket) {
            players[i].socket = 0;
            if (gameStarted == 0) {
                connectedPlayerCount--;
                players[i].username[0] = '\0';
            } else {
                players[i].requestedMove = 0;
            }
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
            perror("Error sending to socket, removing player: ");
            handleDisconnect(socket);
            errno = 0;
            return -1;
        } else if (sentBytes == 0) {
            printf("Client disconnected, removing player\n");
            handleDisconnect(socket);
            return -1;
        } else {
            sentTotal += sentBytes;
        }
    }

    /*printf("Message sent ");
    printBytes(message, messageSize);*/
    return 0;
}

int socketReceive(int socket, char *buff, int messageSize) {
    int recBytes = 0;
    int recTotal = 0;

    while (recTotal < messageSize) {
        recBytes = recv(socket, &buff[recTotal], messageSize - recTotal, 0);
        if (recBytes < 0) {
            perror("Error reading socket, removing player: ");
            handleDisconnect(socket);
            errno = 0;
            return -1;
        } else if (recBytes == 0) {
            printf("Client disconnected, removing player\n");
            handleDisconnect(socket);
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

void respondWithError(int clientSocket, char errorType) {
    if (socketSend(clientSocket, &errorType, sizeof(errorType)) == 0) {
        close(clientSocket);
    }
}

int gameEnded() {
    return 0;
}

int addFoodData(char *buff) {
    int size = 0;
    int i;
    char *ptr = buff;
    for (i = 0; i < cfg.foodCount; i++) {
        struct Position *position = &mapData.foodPositions[i];
        if (position->rowPosition == 0 && position->columnPosition == 0) {
            continue;
        }

        sprintf(ptr, "%03d%03d", position->columnPosition, position->rowPosition);
        ptr += X_COORD_SIZE + Y_COORD_SIZE;
        size += X_COORD_SIZE + Y_COORD_SIZE;
    }

    return size;
}

int addPositionsAndPoints(char *buff) {
    int size = 0;
    int i;
    char *ptr = buff;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        struct PlayerData *player = &players[i];
        if (player->socket == 0 && strlen(player->username) == 0) {
            continue;
        }

        sprintf(ptr, "%03d%03d%03d", player->position.columnPosition, player->position.rowPosition, player->points);
        ptr += X_COORD_SIZE + Y_COORD_SIZE + POINT_SIZE;
        size += X_COORD_SIZE + Y_COORD_SIZE + POINT_SIZE;
    }

    return size;
}

void sendGameUpdateMessage() {
    int actualSize = 0;
    char gameUpdateMessage[GAME_UPDATE_MSG_SIZE] = "";

    actualSize += sprintf(gameUpdateMessage, "%c%d", S_GAME_UPDATE, connectedPlayerCount);
    actualSize += addPositionsAndPoints(&gameUpdateMessage[actualSize]);
    actualSize += sprintf(&gameUpdateMessage[actualSize], "%03d", mapData.currentfoodCount);
    actualSize += addFoodData(&gameUpdateMessage[actualSize]);

    sendToAll(gameUpdateMessage, actualSize);
}

void generateFood() {
    int i;
    char **map = mapData.map;
    for (i = 0; i < cfg.foodCount; i++) {
        struct Position *position = &mapData.foodPositions[i];
        if (position->rowPosition == 0 && position->columnPosition == 0) {
            int randRowPos = 0;
            int randColPos = 0;
            do {
                randRowPos = rand() % (cfg.mapHeight - 2) + 1;
                randColPos = rand() % (cfg.mapWidth - 2) + 1;
            } while (map[randRowPos][randColPos] != ' ');

            map[randRowPos][randColPos] = '@';
            position->rowPosition = randRowPos;
            position->columnPosition = randColPos;
        }
    }

    mapData.currentfoodCount = cfg.foodCount;
}

void resetOneFood(int targetRow, int targetCol) {
    int i;
    for (i = 0; i < cfg.foodCount; i++) {
        struct Position *position = &mapData.foodPositions[i];
        if (position->rowPosition == targetRow && position->columnPosition == targetCol) {
            position->rowPosition = 0;
            position->columnPosition = 0;
            mapData.currentfoodCount--;
            return;
        }
    }
}

void resolveIncomingMoves(int *threadStatus) {
    char playerDeadMessage[2] = "";
    playerDeadMessage[0] = S_PLAYER_DEAD;

    while(1) {
        struct Node *moveNode = NULL;
        if (*threadStatus == THREAD_ERRORED || *threadStatus == THREAD_COMPLETED) {
            return;
        }

        sendGameUpdateMessage();
        usleep(100000);
        pthread_mutex_lock(&lock);

        if (moveQueue->head == NULL) {
            pthread_mutex_unlock(&lock);
            continue;
        }

        for (moveNode = moveQueue->start; moveNode != moveQueue->head->next; moveNode = moveNode->next) {
            char playerSymbol;
            int playerIndex = moveNode->data;
            struct PlayerData *player = &players[playerIndex];
            struct Position *playerPosition = &player->position;
            char targetSymbol;
            int targetRow = 0;
            int targetCol = 0;

            if (player->requestedMove == 0) {
                continue;
            }

            playerSymbol = 65 + playerIndex;
            printf("Player symbol: %c\n", playerSymbol);
            switch (player->requestedMove) {
                case 1:
                    printf("Request to move up\n");
                    if (playerPosition->rowPosition != 0) {
                        targetRow = playerPosition->rowPosition - 1;
                        targetCol = playerPosition->columnPosition;
                    }
                    break;
                case 2:
                    printf("Request to move down\n");
                    if (playerPosition->rowPosition != cfg.mapHeight - 1) {
                        targetRow = playerPosition->rowPosition + 1;
                        targetCol = playerPosition->columnPosition;
                    }
                    break;
                case 3:
                    printf("Request to move right\n");
                    if (playerPosition->columnPosition != cfg.mapWidth - 1) {
                        targetRow = playerPosition->rowPosition;
                        targetCol = playerPosition->columnPosition + 1;
                    }
                    break;
                case 4:
                    printf("Request to move left\n");
                    if (playerPosition->columnPosition != 0) {
                        targetRow = playerPosition->rowPosition;
                        targetCol = playerPosition->columnPosition - 1;
                    }
                    break;
            }

            if (targetRow != 0 && targetCol != 0) {
                char **map = mapData.map;
                targetSymbol = map[targetRow][targetCol];
                printf("Target symbol %c\n", targetSymbol);
                if (targetSymbol == ' ') {
                    map[playerPosition->rowPosition][playerPosition->columnPosition] = ' ';
                    map[targetRow][targetCol] = playerSymbol;
                    playerPosition->rowPosition = targetRow;
                    playerPosition->columnPosition = targetCol;
                } else if (targetSymbol == '@') {
                    map[playerPosition->rowPosition][playerPosition->columnPosition] = ' ';
                    map[targetRow][targetCol] = playerSymbol;
                    playerPosition->rowPosition = targetRow;
                    playerPosition->columnPosition = targetCol;
                    player->points++;
                    resetOneFood(targetRow, targetCol);
                } else if ((int) targetSymbol >= 65 && (int) targetSymbol <= 72) {
                    int targetPlayerIndex = (int) targetSymbol - 65;
                    struct PlayerData *targetPlayer = &players[targetPlayerIndex];
                    if (targetPlayer->points > player->points) {
                        targetPlayer->points += player->points;
                        player->points = 0;
                        socketSend(player->socket, playerDeadMessage, sizeof(S_PLAYER_DEAD));
                        map[playerPosition->rowPosition][playerPosition->columnPosition] = ' ';
                    } else if (targetPlayer->points < player->points) {
                        player->points += targetPlayer->points;
                        targetPlayer->points = 0;
                        socketSend(targetPlayer->socket, playerDeadMessage, sizeof(S_PLAYER_DEAD));
                        map[targetRow][targetCol] = playerSymbol;
                        map[playerPosition->rowPosition][playerPosition->columnPosition] = ' ';
                        playerPosition->rowPosition = targetRow;
                        playerPosition->columnPosition = targetCol;
                    } else {
                        printf("Player collision, both players have the same points, ignoring\n");
                    }
                } else {
                    printf("Tagret position is in wall, ignoring\n");
                }

                if (gameEnded()) {
                    *threadStatus = THREAD_COMPLETED;
                    break;
                }
                printf("Current food count: %d\n", mapData.currentfoodCount);
                if (mapData.currentfoodCount <= cfg.foodRespawnThreshold) {
                    printf("Regenerating food\n");
                    generateFood();
                }
            }

            player->requestedMove = 0;
        }

        moveQueue->head = NULL;
        pthread_mutex_unlock(&lock);
        if (*threadStatus == THREAD_COMPLETED) {
            return;
        }
        printMap();
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
    moveType = ((int) moveRequest[1]);

    if (requestType != R_MOVE) {
        printf("Request type %c was not expected\n", requestType);
        printf("Ignoring player move\n");
        respondWithError(player->socket, E_TECHNICAL);
        return;
    }

    pthread_mutex_lock(&lock);

    if (player->requestedMove == 0) {
        switch (moveType) {
            case 85:
                player->requestedMove = 1;
                addMove(playerIndex);
                break;
            case 68:
                player->requestedMove = 2;
                addMove(playerIndex);
                break;
            case 82:
                player->requestedMove = 3;
                addMove(playerIndex);
                break;
            case 76:
                player->requestedMove = 4;
                addMove(playerIndex);
                break;
            default:
                printf("Requested move is not defined\n");
                printf("Ignoring player move\n");
                respondWithError(player->socket, E_TECHNICAL);
                break;
        }
    }

    pthread_mutex_unlock(&lock);
}

void *setIncomingMoves(void *args) {
    struct pollfd pollList[MAX_PLAYER_COUNT];
    int ret;
    int i;
    int *threadStatus = args;
    int allClientsDisconnected;

    while(1) {
        if (*threadStatus == THREAD_ERRORED || *threadStatus == THREAD_COMPLETED) {
            return NULL;
        }

        allClientsDisconnected = 1;

        for (i = 0; i < MAX_PLAYER_COUNT; i++) {
            if (allClientsDisconnected == 1 && players[i].socket > 0) {
                allClientsDisconnected = 0;
            }
            pollList[i].fd = players[i].socket;
            pollList[i].events = POLLIN;
        }

        if (allClientsDisconnected == 1) {
            *threadStatus = THREAD_COMPLETED;
            return NULL;
        }

        ret = poll(pollList, MAX_PLAYER_COUNT, 10);
        if(ret < 0) {
            perror("Error starting polling: ");
            *threadStatus = THREAD_ERRORED;
            return NULL;
        } else if (ret == 0) {
            continue;
        }


        for (i = 0; i < MAX_PLAYER_COUNT; i++) {
            if ((pollList[i].revents & POLLHUP) == POLLHUP || (pollList[i].revents & POLLERR) == POLLERR ||
            (pollList[i].revents & POLLNVAL) == POLLNVAL) {
                printf("Error polling client\n");
                handleDisconnect(pollList[i].fd);
                pollList[i].fd = 0;
            }
            else if ((pollList[i].revents & POLLIN) == POLLIN) {
                readAndSetMove(i);
            }
        }
    }

    return NULL;
}

void sendMap() {
    int actualSize = 4 + cfg.mapWidth;
    int i;
    int j;
    for (i = 0; i < cfg.mapHeight; i++) {
        char mapMessage[MAP_MSG_SIZE] = "";
        char *mapRowPtr = NULL;
        sprintf(mapMessage, "%c%03d%s", S_MAP_ROW, i + 1, mapData.map[i]);
        mapRowPtr = &mapMessage[4];
        for (j = 0; j < cfg.mapWidth; j++) {
            if ((int) mapRowPtr[j] >= 65 && (int) mapRowPtr[j] <= 72) {
                mapRowPtr[j] = ' ';
            }
        }
        sendToAll(mapMessage, actualSize);
    }
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

void sendGameStartMessage() {
    int actualSize = 0;
    char gameStartMessage[GAME_START_MSG_SIZE] = "";

    actualSize += sprintf(gameStartMessage, "%c%d", S_GAME_START, connectedPlayerCount);
    actualSize += addUsernames(&gameStartMessage[actualSize]);
    actualSize += sprintf(&gameStartMessage[actualSize], "%03d%03d", cfg.mapWidth, cfg.mapHeight);

    sendToAll(gameStartMessage, actualSize);
}

void *handleGameStart(void *args) {
    int ret;
    int *threadStatus = args;

    sendGameStartMessage();
    sendMap();
    generateFood();

    printMap();

    ret = pthread_create(&moveSetterThread, NULL, setIncomingMoves, threadStatus);
    if (ret != 0) {
        perror("Failed to create moveSetterThread");
        *threadStatus = THREAD_ERRORED;
        return NULL;
    }

    resolveIncomingMoves(threadStatus);
    pthread_join(moveSetterThread, NULL);

    if (*threadStatus == THREAD_COMPLETED) {
        printf("THREAD_COMPLETED\n");
        /* Execute game end here */
        /* Reset game here */
    }

    return NULL;
}

void startGame() {
    int ret;
    gameStarted = 1;
    int threadStatus = THREAD_RUNNING;

    printMap();

    ret = pthread_create(&moveResolverThread, NULL, handleGameStart, &threadStatus);
    if (ret != 0) {
        fprintf(stderr, "Failed to create moveResolverThread");
        perror("");
        exitHandler(1);
    }

    pthread_join(moveResolverThread, NULL);
    if (threadStatus == THREAD_ERRORED) {
        fprintf(stderr, "Game handling threads returned with error\n");
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

void sendLobbyInfoMessage() {
    int actualSize = 0;
    char lobbyInfoMessage[LOBBY_INFO_MSG_SIZE] = "";

    actualSize += sprintf(lobbyInfoMessage, "%c%d", S_LOBBY_INFO, connectedPlayerCount);
    actualSize += addUsernames(&lobbyInfoMessage[actualSize]);

    sendToAll(lobbyInfoMessage, actualSize);
}

int addClientToGame(int clientSocket) {
    char joinGameRequest[JOIN_GAME_MSG_SIZE] = "";
    char requestType;
    int playerPosition;

    if (socketReceive(clientSocket, joinGameRequest, sizeof(joinGameRequest) - 1) < 0) {
        return -1;
    }

    if (strlen(joinGameRequest) < 2) {
        printf("Cannot add player to game - incorrect join game request size\n");
        respondWithError(clientSocket, E_TECHNICAL);
        close(clientSocket);
        return -1;
    }

    requestType = joinGameRequest[0];

    if (requestType != R_JOIN_GAME) {
        printf("Cannot add player to game - incorrect join game request type\n");
        respondWithError(clientSocket, E_TECHNICAL);
        close(clientSocket);
        return -1;
    }

    if (gameStarted) {
        printf("Cannot add player to game - game in progress\n");
        respondWithError(clientSocket, E_GAME_IN_PROGRESS);
        close(clientSocket);
        return -1;
    }

    playerPosition = getRandomFreePosition();
    if (playerPosition < 0) {
        printf("Cannot add player to game - game full\n");
        respondWithError(clientSocket, E_GAME_IN_PROGRESS);
        close(clientSocket);
        return -1;
    }

    if (usernameTaken(&joinGameRequest[1])) {
        printf("Cannot add player to game - username taken\n");
        respondWithError(clientSocket, E_USERNAME_TAKEN);
        close(clientSocket);
        return -1;
    }

    strcpy(players[playerPosition].username, &joinGameRequest[1]);
    players[playerPosition].socket = clientSocket;
    connectedPlayerCount++;

    printf("Player successfully added to game\n");
    return 0;
}

void loadPlayerData() {
    int i;

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        players[i].points = 1;
        players[i].position.rowPosition = mapData.startPositions[i].rowPosition;
        players[i].position.columnPosition = mapData.startPositions[i].columnPosition;
    }
}

void printMap() {
    int i;
    for (i = 0; i < cfg.mapHeight; i++) {
        printf("%s\n", mapData.map[i]);
    }
}

void checkAndSetSpawnPositions(char *mapRow, int rowPosition) {
    int i;
    for (i = 0; i < cfg.mapWidth; i++) {
        int cellValue = (int) mapRow[i];
        if (cellValue >= 65 && cellValue <= 72) {
            int index = cellValue - 65;
            mapData.startPositions[index].rowPosition = rowPosition;
            mapData.startPositions[index].columnPosition = i;
        }
    }
}

void allocateMap() {
    int i;

    /* Allocate map */
    mapData.map = malloc(cfg.mapHeight * sizeof(char *));
    if (mapData.map == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exitHandler(1);
    }
    for(i = 0; i < cfg.mapHeight; i++) {
        mapData.map[i] = malloc(cfg.mapWidth * sizeof(char) + 3);
        if (mapData.map[i] == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            exitHandler(1);
        }
        memset(mapData.map[i], 0, cfg.mapWidth * sizeof(char) + 2);
    }

    /* Allocate food positions */
    mapData.foodPositions = malloc(cfg.foodCount * sizeof(struct Position));
    if (mapData.foodPositions == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exitHandler(1);
    }
    memset(mapData.foodPositions, 0, cfg.foodCount * sizeof(struct Position));

    /* Allocate start positions */
    mapData.startPositions = malloc(MAX_PLAYER_COUNT * sizeof(struct Position));
    if (mapData.startPositions == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exitHandler(1);
    }
    memset(mapData.startPositions, 0, MAX_PLAYER_COUNT * sizeof(struct Position));
}

void loadMapData() {
    FILE *mapFile = NULL;
    int i;

    allocateMap();
    mapData.currentfoodCount = cfg.foodCount;

    mapFile = fopen(cfg.mapFilename, "r");
    if (mapFile == NULL) {
        fprintf(stderr, "Could not open map file '%s': ", cfg.mapFilename);
        perror("");
        exitHandler(1);
    }

    for (i = 0; i < cfg.mapHeight; i++) {
        int rowLength;
        if (getLine(mapData.map[i], cfg.mapWidth * sizeof(char) + 3, mapFile) == NULL) {
            if (i != cfg.mapHeight - 1) {
                fprintf(stderr, "Mismatching map height and configured height\n");
                fclose(mapFile);
                exitHandler(1);
            }
            break;
        }

        rowLength = strlen(mapData.map[i]);
        if (rowLength != cfg.mapWidth) {
            fprintf(stderr, "Mismatching map width and configured width\n");
            fclose(mapFile);
            exitHandler(1);
        }

        checkAndSetSpawnPositions(mapData.map[i], i);
    }

    fclose(mapFile);
    printMap();
}

void init() {
    int enable = 1;
    struct sockaddr_in serverAddress;
    struct sigaction sigIntHandler;

    /* load initial configuration */
    if (loadCfg() < 0) {
        fprintf(stderr, "Error loading configuration\n");
        exitHandler(1);
    }
    loadMapData();
    loadPlayerData();
    if (initMoveQueue(MAX_PLAYER_COUNT) < 0) {
        fprintf(stderr, "Error initializing move queue\n");
        exitHandler(1);
    }

    /* initialization for setting players random positions later. Should only be called once. */
    srand(time(NULL));

    /* set exitHandler on signals */
    sigIntHandler.sa_handler = exitHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    /* initialize lock */
    if (pthread_mutex_init(&lock, NULL) != 0) {
        fprintf(stderr, "Error initializing lock\n");
        exitHandler(1);
    }

    /* initialize main server socket */
    netSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (netSocket < 0) {
        perror("Error creating socket: ");
        exitHandler(1);
    }
    /* set main server socket as reusable so reboots work */
    if (setsockopt(netSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("Error on setting socket to SO_REUSEADDR: ");
        exitHandler(1);
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(cfg.serverPort);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(netSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        perror("Error binding server socket: ");
        exitHandler(1);
    }

    if (listen(netSocket, 30) < 0) {
        perror("Error listening on server socket: ");
        exitHandler(1);
    }
    printf("Listening...\n");
}

int main() {
    init();
    printStartPositions();
    printPlayerPositions();

    /* Main connection accepting loop */
    while (1) {
        int clientSocket = accept(netSocket, NULL, NULL);
        if (clientSocket < 0) {
            perror("Error accepting client: ");
            continue;
        }
        printf("Client connected\n");

        if (addClientToGame(clientSocket) == 0) {
            sendLobbyInfoMessage();
            if (connectedPlayerCount == MAX_PLAYER_COUNT) {
                startGame();
            }
        }
    }

    close(netSocket);

    return 0;
}
