#define _XOPEN_SOURCE 500 /* Necessary to set signal handler for SIGINT */

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

/* Custom made libraries */
#include "libs/servercfg.h"
#include "libs/utility.h"
#include "libs/move_queue.h"

/* Sizes of each message parameter */
#define TYPE_SIZE 1
#define USERNAME_SIZE 16
#define PLAYER_COUNT_SIZE 1
#define POINT_SIZE 3
#define MAP_HEIGHT_SIZE 3
#define MAP_WIDTH_SIZE 3
#define X_COORD_SIZE 3
#define Y_COORD_SIZE 3
#define MOVE_SIZE 1
#define FOOD_COUNT_SIZE 3

/* Maximum amounts */
#define MAX_PLAYER_COUNT 8
#define MAX_MAP_HEIGHT 999
#define MAX_MAP_WIDTH 999
#define MAX_FOOD_COUNT 999

/* Message type symbols */
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

/* Maximum sizes for all defined messages */
#define JOIN_GAME_MSG_SIZE (TYPE_SIZE + USERNAME_SIZE + 1)
#define LOBBY_INFO_MSG_SIZE (TYPE_SIZE + PLAYER_COUNT_SIZE + MAX_PLAYER_COUNT * USERNAME_SIZE + 1)
#define GAME_START_MSG_SIZE (TYPE_SIZE + PLAYER_COUNT_SIZE + MAX_PLAYER_COUNT * USERNAME_SIZE + MAP_HEIGHT_SIZE + MAP_WIDTH_SIZE + 1)
#define MAP_MSG_SIZE (TYPE_SIZE + MAP_WIDTH_SIZE + MAX_MAP_WIDTH + 1)
#define MOVE_MSG_SIZE (TYPE_SIZE + MOVE_SIZE + 1)
#define GAME_UPDATE_MSG_SIZE (TYPE_SIZE + PLAYER_COUNT_SIZE + MAX_PLAYER_COUNT * (X_COORD_SIZE + Y_COORD_SIZE + POINT_SIZE) + FOOD_COUNT_SIZE + MAX_FOOD_COUNT * (X_COORD_SIZE + Y_COORD_SIZE) + 1)
#define GAME_END_MSG_SIZE (TYPE_SIZE + PLAYER_COUNT_SIZE + MAX_PLAYER_COUNT * (USERNAME_SIZE + POINT_SIZE))

#define GAME_NOT_STARTED 0
#define GAME_IN_PROGRESS 1

#define THREAD_RUNNING 0
#define THREAD_COMPLETED 1 /* thread status to know if game stopped and server should continue */
#define THREAD_ERRORED 2 /* thread status to know if game stopped and server should stop */

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

int g_netSocket; /* The main server socket */

struct MapData g_mapData = {};
struct PlayerData g_players[MAX_PLAYER_COUNT];
int g_connectedPlayerCount = 0;
int g_gameStatus = GAME_NOT_STARTED;

pthread_t g_moveResolverThread;
pthread_t g_moveSetterThread;
pthread_mutex_t g_moveLock;
pthread_mutex_t g_gameStatusLock;
pthread_mutex_t g_threadStatusLock;
int g_threadStatus = THREAD_RUNNING;

void printMap();

/* Thread shared data getters/setters */
int getThreadStatus();
void setThreadStatus(int status);
int getGameStatus();
void setGameStatus(int status);

void closeSocket(int socket);
void exitHandler(int status);
void sigIntHandler(int sig);
void handleDisconnect(int socket);
int socketSend(int socket, char *message, int messageSize);
int socketReceive(int socket, char *buff, int messageSize);
void sendToAll(char *message, int size);
void respondWithError(int clientSocket, char errorType);

/* Functions that add parameters to messages */
int addUsernamesAndPoints(char *buff);
int addFoodData(char *buff);
int addPositionsAndPoints(char *buff);
int addUsernames(char *buff);

/* Functions for sending all defined message types */
void sendLobbyInfoMessage();
void sendGameStartMessage();
void sendMap();
void sendGameUpdateMessage();
void sendGameEndMessage();

/* Game duration handling functions */
void resetGame();
int gameEnded();
void setPlayersOnMap();
void generateFood();
void resetOneFood(int targetRow, int targetCol);
void resolveIncomingMoves();
void readAndSetMove(int playerIndex);
void *setIncomingMoves(void *args);
void *handleGameStart(void *args);
void startGame();

/* Functions for accepting a client */
int usernameTaken(char *username);
int getRandomFreePosition();
int addClientToGame(int clientSocket);

/* Pre-listening set up */
void loadPlayerData();
int checkAndSetSpawnPositions(char *mapRow, int rowPosition);
void allocateMap();
void loadMapData();
void init();

void printMap() {
    int i;
    for (i = 0; i < cfg.mapHeight; i++) {
        printf("%s\n", g_mapData.map[i]);
    }
}

int getThreadStatus() {
    int status;
    pthread_mutex_lock(&g_threadStatusLock);
    status = g_threadStatus;
    pthread_mutex_unlock(&g_threadStatusLock);
    return status;
}

void setThreadStatus(int status) {
    pthread_mutex_lock(&g_threadStatusLock);
    if (g_threadStatus != THREAD_ERRORED) {
        g_threadStatus = status;
    }
    pthread_mutex_unlock(&g_threadStatusLock);
}

int getGameStatus() {
    int status;
    pthread_mutex_lock(&g_gameStatusLock);
    status = g_gameStatus;
    pthread_mutex_unlock(&g_gameStatusLock);

    return status;
}

void setGameStatus(int status) {
    printf("Setting game status to: %d\n", status);
    pthread_mutex_lock(&g_gameStatusLock);
    g_gameStatus = status;
    pthread_mutex_unlock(&g_gameStatusLock);
}

void closeSocket(int socket) {
    if (socket > 0) {
        if (shutdown(socket, SHUT_RDWR) < 0) {
            perror("Error shutting down socket");
        }
        if (close(socket) < 0) {
            perror("Error closing socket");
        }
    }
}

void exitHandler(int status) {
    int i;
    printf("Entered exit handler\n");

    setThreadStatus(THREAD_ERRORED); /* tell threads to stop */

    /* Clean up threads */
    pthread_join(g_moveSetterThread, NULL);
    pthread_join(g_moveResolverThread, NULL);
    pthread_mutex_destroy(&g_moveLock);
    pthread_mutex_destroy(&g_gameStatusLock);
    pthread_mutex_destroy(&g_threadStatusLock);

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (g_players[i].socket > 0) {
            closeSocket(g_players[i].socket);
        }
    }
    closeSocket(g_netSocket);

    freeMoveQueue();

    if (g_mapData.map != NULL) {
        for (i = 0; i < cfg.mapHeight; i++) {
            if (g_mapData.map[i] != NULL) {
                free(g_mapData.map[i]);
            }
        }
        free(g_mapData.map);
    }
    if (g_mapData.foodPositions != NULL) {
        free(g_mapData.foodPositions);
    }
    if (g_mapData.startPositions != NULL) {
        free(g_mapData.startPositions);
    }

    exit(status);
}

/* Our only way of correctly stopping the server */
void sigIntHandler(int sig) {
    printf("Signal caught, stopping server\n");
    exitHandler(0);
}

void handleDisconnect(int socket) {
    int i;
    printf("Handling a disconnected client\n");
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (g_players[i].socket == socket) {
            g_players[i].socket = 0;
            if (getGameStatus() == GAME_NOT_STARTED) {
                /* if a client disconnects during game, we still want to send this */
                g_connectedPlayerCount--;
                g_players[i].username[0] = '\0';
            } else {
                g_players[i].requestedMove = 0;
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

    /* socket might not send full message in one go, need to retry until all is sent */
    while (sentTotal < messageSize) {
        sentBytes = send(socket, &message[sentTotal], messageSize - sentTotal, MSG_NOSIGNAL);
        if (sentBytes < 0) {
            handleDisconnect(socket);
            errno = 0;
            return -1;
        } else if (sentBytes == 0) {
            handleDisconnect(socket);
            return -1;
        } else {
            sentTotal += sentBytes;
        }
    }

    return 0;
}

int socketReceive(int socket, char *buff, int messageSize) {
    int recBytes = 0;
    int recTotal = 0;

    /* socket might not receive full message in one go, need to retry until all is sent */
    while (recTotal < messageSize) {
        recBytes = recv(socket, &buff[recTotal], messageSize - recTotal, 0);
        if (recBytes < 0) {
            handleDisconnect(socket);
            errno = 0;
            return -1;
        } else if (recBytes == 0) {
            handleDisconnect(socket);
            return -1;
        } else {
            recTotal += recBytes;
        }
    }

    return 0;
}

void sendToAll(char *message, int size) {
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (g_players[i].socket == 0) {
            continue;
        }
        socketSend(g_players[i].socket, message, size);
    }
}

void respondWithError(int clientSocket, char errorType) {
    if (socketSend(clientSocket, &errorType, sizeof(errorType)) == 0) {
        close(clientSocket);
    }
}

int addUsernamesAndPoints(char *buff) {
    int size = 0;
    int i;
    char *ptr = buff;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        struct PlayerData *player = &g_players[i];
        if (player->socket == 0 && strlen(player->username) == 0) {
            continue;
        }


        strcat(ptr, g_players[i].username);
        ptr += USERNAME_SIZE;
        size += USERNAME_SIZE;

        sprintf(ptr, "%03d", player->points);
        ptr += POINT_SIZE;
        size += POINT_SIZE;
    }

    return size;
}

int addFoodData(char *buff) {
    int size = 0;
    int i;
    char *ptr = buff;
    for (i = 0; i < cfg.foodCount; i++) {
        struct Position *foodPosition = &g_mapData.foodPositions[i];
        if (foodPosition->rowPosition == 0 && foodPosition->columnPosition == 0) {
            continue;
        }

        sprintf(ptr, "%03d%03d", foodPosition->columnPosition, foodPosition->rowPosition);
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
        struct PlayerData *player = &g_players[i];
        if (player->socket == 0 && strlen(player->username) == 0) {
            continue;
        }

        sprintf(ptr, "%03d%03d%03d", player->position.columnPosition, player->position.rowPosition, player->points);
        ptr += X_COORD_SIZE + Y_COORD_SIZE + POINT_SIZE;
        size += X_COORD_SIZE + Y_COORD_SIZE + POINT_SIZE;
    }

    return size;
}

int addUsernames(char *buff) {
    int size = 0;
    char *ptr = buff;
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (g_players[i].socket == 0) {
            continue;
        }

        strcat(ptr, g_players[i].username);
        ptr += USERNAME_SIZE;
        size += USERNAME_SIZE;
    }

    return size;
}

void sendLobbyInfoMessage() {
    int actualSize = 0;
    char lobbyInfoMessage[LOBBY_INFO_MSG_SIZE] = "";

    actualSize += sprintf(lobbyInfoMessage, "%c%d", S_LOBBY_INFO, g_connectedPlayerCount);
    actualSize += addUsernames(&lobbyInfoMessage[actualSize]);

    printf("Sending lobby info message: ");
    printBytes(lobbyInfoMessage, actualSize);

    sendToAll(lobbyInfoMessage, actualSize);
}

void sendGameStartMessage() {
    int actualSize = 0;
    char gameStartMessage[GAME_START_MSG_SIZE] = "";

    actualSize += sprintf(gameStartMessage, "%c%d", S_GAME_START, g_connectedPlayerCount);
    actualSize += addUsernames(&gameStartMessage[actualSize]);
    actualSize += sprintf(&gameStartMessage[actualSize], "%03d%03d", cfg.mapWidth, cfg.mapHeight);

    printf("Sending game start message: ");
    printBytes(gameStartMessage, actualSize);

    sendToAll(gameStartMessage, actualSize);
}

void sendMap() {
    int actualSize = 4 + cfg.mapWidth;
    int i;
    int j;
    for (i = 0; i < cfg.mapHeight; i++) {
        char mapMessage[MAP_MSG_SIZE] = "";
        char *mapRowPtr = NULL;
        sprintf(mapMessage, "%c%03d%s", S_MAP_ROW, i + 1, g_mapData.map[i]);
        mapRowPtr = &mapMessage[4];
        for (j = 0; j < cfg.mapWidth; j++) {
            if ((int) mapRowPtr[j] >= 65 && (int) mapRowPtr[j] <= 72) {
                mapRowPtr[j] = ' ';
            }
        }
        printf("Sending map row: %s\n", mapMessage);
        sendToAll(mapMessage, actualSize);
    }
}

void sendGameUpdateMessage() {
    int actualSize = 0;
    char gameUpdateMessage[GAME_UPDATE_MSG_SIZE] = "";

    actualSize += sprintf(gameUpdateMessage, "%c%d", S_GAME_UPDATE, g_connectedPlayerCount);
    actualSize += addPositionsAndPoints(&gameUpdateMessage[actualSize]);
    actualSize += sprintf(&gameUpdateMessage[actualSize], "%03d", g_mapData.currentfoodCount);
    actualSize += addFoodData(&gameUpdateMessage[actualSize]);

    sendToAll(gameUpdateMessage, actualSize);
}

void sendGameEndMessage() {
    int actualSize = 0;
    char gameEndMessage[GAME_END_MSG_SIZE] = "";

    actualSize += sprintf(gameEndMessage, "%c%d", S_GAME_END, g_connectedPlayerCount);
    actualSize += addUsernamesAndPoints(&gameEndMessage[actualSize]);

    printf("Sending game end message: ");
    printBytes(gameEndMessage, actualSize);

    sendToAll(gameEndMessage, actualSize);
}

void resetGame() {
    char **map = g_mapData.map;
    int i;

    printf("Resetting game...\n");

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        struct PlayerData *player = &g_players[i];
        struct Position *playerPosition = &player->position;
        if (player->socket > 0) {
            closeSocket(player->socket);
            player->socket = 0;
        }
        player->username[0] = '\0';
        player->points = 1;
        player->requestedMove = 0;

        /* Reset player starting positions back on the simulated map */
        map[playerPosition->rowPosition][playerPosition->columnPosition] = ' ';
        playerPosition->rowPosition = g_mapData.startPositions[i].rowPosition;
        playerPosition->columnPosition = g_mapData.startPositions[i].columnPosition;
    }

    for (i = 0; i < cfg.foodCount; i++) {
        struct Position *foodPosition = &g_mapData.foodPositions[i];
        if (foodPosition->rowPosition != 0 || foodPosition->columnPosition != 0) {
            map[foodPosition->rowPosition][foodPosition->columnPosition] = ' ';
            foodPosition->rowPosition = 0;
            foodPosition->columnPosition = 0;
        }
    }
    g_mapData.currentfoodCount = 0;

    g_connectedPlayerCount = 0;

    setGameStatus(GAME_NOT_STARTED);
    setThreadStatus(THREAD_RUNNING);

    printf("Game reset\n");
}

int gameEnded() {
    int alivePlayerCount = 0;
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        struct PlayerData *player = &g_players[i];
        if (player->socket == 0 && strlen(player->username) == 0) {
            continue;
        }
        if (player->points != 0) {
            alivePlayerCount++;
            if (player->points == cfg.pointWinCount) {
                printf("A player has max points, end game\n");
                return 1;
            }
        }
    }

    if (alivePlayerCount <= 1) {
        printf("Only one player alive, end game\n");
        return 1;
    }

    return 0;
}

void setPlayersOnMap() {
    int i;
    char **map = g_mapData.map;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        struct PlayerData *player = &g_players[i];
        struct Position *playerPosition;
        char playerSymbol;
        if (player->socket == 0 && strlen(player->username) == 0) {
            continue;
        }
        playerPosition = &player->position;
        playerSymbol = (char) 65 + i;
        map[playerPosition->rowPosition][playerPosition->columnPosition] = playerSymbol;
    }
}

void generateFood() {
    int i;
    char **map = g_mapData.map;
    int foodCountBefore = g_mapData.currentfoodCount; /* just for logging */

    printf("Generating food...\n");

    for (i = 0; i < cfg.foodCount; i++) {
        struct Position *foodPosition = &g_mapData.foodPositions[i];
        if (foodPosition->rowPosition == 0 && foodPosition->columnPosition == 0) {
            int randRowPos = 0;
            int randColPos = 0;
            int i;
            /* Attempt to spawn the missing food configured amount of times, to avoid endless loop */
            for (i = 0; i < cfg.foodGenAttemptCount; i++) {
                randRowPos = rand() % (cfg.mapHeight - 2) + 1;
                randColPos = rand() % (cfg.mapWidth - 2) + 1;
                if (map[randRowPos][randColPos] == ' ') {
                    map[randRowPos][randColPos] = '@';
                    foodPosition->rowPosition = randRowPos;
                    foodPosition->columnPosition = randColPos;
                    g_mapData.currentfoodCount++;
                    break;
                }
            }
        }
    }

    printf("Generated food: %d\n", g_mapData.currentfoodCount - foodCountBefore);
}

void resetOneFood(int targetRow, int targetCol) {
    int i;
    for (i = 0; i < cfg.foodCount; i++) {
        struct Position *foodPosition = &g_mapData.foodPositions[i];
        if (foodPosition->rowPosition == targetRow && foodPosition->columnPosition == targetCol) {
            foodPosition->rowPosition = 0;
            foodPosition->columnPosition = 0;
            g_mapData.currentfoodCount--;
            return;
        }
    }
}

void resolveIncomingMoves() {
    int gameEndTickCount = 0;
    int gameEndTickTimeout = cfg.gameEndTimeout * 10;
    char playerDeadMessage[2] = "";
    playerDeadMessage[0] = S_PLAYER_DEAD;
    int tickDelay = cfg.tickDelay * 1000;

    while(1) {
        struct Node *moveNode = NULL;
        int tStatus;

        sendGameUpdateMessage();
        tStatus = getThreadStatus();
        if (tStatus == THREAD_COMPLETED || tStatus == THREAD_ERRORED) {
            return;
        }

        usleep(tickDelay);

        tStatus = getThreadStatus();
        if (tStatus == THREAD_ERRORED || tStatus == THREAD_COMPLETED) {
            return;
        }

        if (cfg.gameEndTimeout != 0) {
            gameEndTickCount++;
            if (gameEndTickCount == gameEndTickTimeout) {
                printf("Game end timeout reached\n");
                setThreadStatus(THREAD_COMPLETED);
                return;
            }
        }

        pthread_mutex_lock(&g_moveLock); /* Don't let new moves be assigned */

        if (moveQueue->head == NULL) {
            /* No moves have been assigned */
            pthread_mutex_unlock(&g_moveLock);
            continue;
        }

        for (moveNode = moveQueue->start; moveNode != moveQueue->head->next; moveNode = moveNode->next) {
            char playerSymbol;
            int playerIndex = moveNode->data;
            struct PlayerData *player = &g_players[playerIndex];
            struct Position *playerPosition = &player->position;
            char targetSymbol; /* Symbol located in the requested coordinates of the map */
            int targetRow = 0;
            int targetCol = 0;

            if (player->requestedMove == 0) {
                continue;
            }

            playerSymbol = 65 + playerIndex; /* Symbol of the move requesting player on the map */
            switch (player->requestedMove) {
                case 1: /* Up */
                    if (playerPosition->rowPosition != 0) {
                        targetRow = playerPosition->rowPosition - 1;
                        targetCol = playerPosition->columnPosition;
                    }
                    break;
                case 2: /* Down */
                    if (playerPosition->rowPosition != cfg.mapHeight - 1) {
                        targetRow = playerPosition->rowPosition + 1;
                        targetCol = playerPosition->columnPosition;
                    }
                    break;
                case 3: /* Right */
                    if (playerPosition->columnPosition != cfg.mapWidth - 1) {
                        targetRow = playerPosition->rowPosition;
                        targetCol = playerPosition->columnPosition + 1;
                    }
                    break;
                case 4: /* Left */
                    if (playerPosition->columnPosition != 0) {
                        targetRow = playerPosition->rowPosition;
                        targetCol = playerPosition->columnPosition - 1;
                    }
                    break;
            }

            if (targetRow != 0 && targetCol != 0) {
                char **map = g_mapData.map;
                targetSymbol = map[targetRow][targetCol];

                if (targetSymbol == ' ') { /* Move to empty position */
                    map[playerPosition->rowPosition][playerPosition->columnPosition] = ' ';
                    map[targetRow][targetCol] = playerSymbol;
                    playerPosition->rowPosition = targetRow;
                    playerPosition->columnPosition = targetCol;
                } else if (targetSymbol == '@') { /* Move to food */
                    map[playerPosition->rowPosition][playerPosition->columnPosition] = ' ';
                    map[targetRow][targetCol] = playerSymbol;
                    playerPosition->rowPosition = targetRow;
                    playerPosition->columnPosition = targetCol;
                    player->points++;
                    resetOneFood(targetRow, targetCol);
                } else if ((int) targetSymbol >= 65 && (int) targetSymbol <= 72) { /* Move to other player */
                    int targetPlayerIndex = (int) targetSymbol - 65;
                    struct PlayerData *targetPlayer = &g_players[targetPlayerIndex];
                    if (targetPlayer->points > player->points) { /* Move requesting player death scenario */
                        targetPlayer->points += player->points;
                        if (targetPlayer->points > cfg.pointWinCount) {
                            targetPlayer->points = cfg.pointWinCount;
                        }

                        player->points = 0;
                        printf("Sending player dead to %s\n", player->username);
                        socketSend(player->socket, playerDeadMessage, strlen(playerDeadMessage));

                        map[playerPosition->rowPosition][playerPosition->columnPosition] = ' ';
                    } else if (targetPlayer->points < player->points) { /* Target player death scenario */
                        player->points += targetPlayer->points;
                        if (player->points > cfg.pointWinCount) {
                            player->points = cfg.pointWinCount;
                        }

                        targetPlayer->points = 0;
                        printf("Sending player dead to %s\n", targetPlayer->username);
                        socketSend(targetPlayer->socket, playerDeadMessage, strlen(playerDeadMessage));

                        map[targetRow][targetCol] = playerSymbol;
                        map[playerPosition->rowPosition][playerPosition->columnPosition] = ' ';
                        playerPosition->rowPosition = targetRow;
                        playerPosition->columnPosition = targetCol;
                    }
                }

                if (gameEnded()) {
                    setThreadStatus(THREAD_COMPLETED);
                    break;
                }
                if (g_mapData.currentfoodCount <= cfg.foodRespawnThreshold) {
                    generateFood();
                }
            }

            player->requestedMove = 0;
        }

        moveQueue->head = NULL; /* Empty move queue */
        pthread_mutex_unlock(&g_moveLock); /* Let new moves be assigned again */
    }
}

void readAndSetMove(int playerIndex) {
    char moveRequest[MOVE_MSG_SIZE] = "";
    char requestType;
    int moveType;
    struct PlayerData *player = &g_players[playerIndex];

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

    if (player->points == 0) {
        printf("Move request from dead player\n");
        printf("Ignoring player move\n");
        return;
    }

    pthread_mutex_lock(&g_moveLock); /* Don't allow resolving moves while setting one */

    if ((cfg.moveResolutionMode == 'F' && player->requestedMove == 0) || cfg.moveResolutionMode == 'L') {
        switch (moveType) {
            case 85: /* 'U' for Up */
                player->requestedMove = 1;
                break;
            case 68: /* 'D' for Down */
                player->requestedMove = 2;
                break;
            case 82: /* 'R' for Right */
                player->requestedMove = 3;
                break;
            case 76: /* 'L' for Left */
                player->requestedMove = 4;
                break;
            default:
                printf("Requested move is not defined\n");
                printf("Ignoring player move\n");
                respondWithError(player->socket, E_TECHNICAL);
                break;
        }

        if (cfg.moveResolutionMode == 'F') {
            addMove(playerIndex);
        } else if (cfg.moveResolutionMode == 'L') {
            replaceMove(playerIndex);
        }
    }

    pthread_mutex_unlock(&g_moveLock); /* Allow resolving moves again after this move has been set */
}

void *setIncomingMoves(void *args) {
    struct pollfd pollList[MAX_PLAYER_COUNT];
    int ret;
    int i;
    int tStatus;
    int activeClientCount;

    while(1) {
        tStatus = getThreadStatus();
        if (tStatus == THREAD_ERRORED || tStatus == THREAD_COMPLETED) {
            return NULL;
        }

        activeClientCount = 0;

        /* Need to reassign polling client list every time in case players disconnect */
        for (i = 0; i < MAX_PLAYER_COUNT; i++) {
            if (g_players[i].socket > 0) {
                activeClientCount++;
            }
            pollList[i].fd = g_players[i].socket;
            pollList[i].events = POLLIN;
        }

        if (activeClientCount <= 1) {
            printf("One or no players connected, end game\n");
            setThreadStatus(THREAD_COMPLETED);
            return NULL;
        }

        /* Polling is used because i need to know which client is sending a message,
           cannot allow to be waiting for a single player */
        ret = poll(pollList, MAX_PLAYER_COUNT, 10);
        if(ret < 0) {
            perror("Error starting client polling");
            setThreadStatus(THREAD_ERRORED);
            return NULL;
        } else if (ret == 0) {
            continue;
        }


        for (i = 0; i < MAX_PLAYER_COUNT; i++) {
            if (pollList[i].fd == 0) {
                continue;
            }
            if ((pollList[i].revents & POLLHUP) == POLLHUP || (pollList[i].revents & POLLERR) == POLLERR ||
            (pollList[i].revents & POLLNVAL) == POLLNVAL) {
                printf("Error polling client socket\n");
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

void *handleGameStart(void *args) {
    int ret;
    int tStatus;

    sendGameStartMessage();
    sendMap();
    generateFood();
    setPlayersOnMap();

    ret = pthread_create(&g_moveSetterThread, NULL, setIncomingMoves, NULL);
    if (ret != 0) {
        perror("Failed to create moveSetterThread");
        return NULL;
    }

    printf("Game started\n");

    resolveIncomingMoves();
    pthread_join(g_moveSetterThread, NULL);

    tStatus = getThreadStatus();
    if (tStatus == THREAD_COMPLETED) {
        printf("game ended successfully\n");
        sendGameEndMessage();
        resetGame();
    }

    return NULL;
}

void startGame() {
    int ret;
    printf("Starting game...\n");
    setGameStatus(GAME_IN_PROGRESS);

    ret = pthread_create(&g_moveResolverThread, NULL, handleGameStart, NULL);
    if (ret != 0) {
        fprintf(stderr, "Failed to create moveResolverThread");
        perror("");
        exitHandler(1);
    }
}

int usernameTaken(char *username) {
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (g_players[i].socket != 0) {
            if (strcmp(username, g_players[i].username) == 0) {
                return 1;
            }
        }
    }

    return 0;
}

/* Assigns players random player slots that have different start positions */
int getRandomFreePosition() {
    int freePositions[MAX_PLAYER_COUNT];
    int freePositionCount = 0;
    int randomPosition;
    int lastIndex = 0;
    int i;

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (g_players[i].socket == 0) {
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

int addClientToGame(int clientSocket) {
    char joinGameRequest[JOIN_GAME_MSG_SIZE] = "";
    char requestType;
    int playerPosition;

    if (socketReceive(clientSocket, joinGameRequest, sizeof(joinGameRequest) - 1) < 0) {
        return 0;
    }

    if (strlen(joinGameRequest) < 2) {
        printf("Cannot add player to game - incorrect join game request size\n");
        respondWithError(clientSocket, E_TECHNICAL);
        close(clientSocket);
        return 0;
    }

    requestType = joinGameRequest[0];

    if (requestType != R_JOIN_GAME) {
        printf("Cannot add player to game - incorrect join game request type\n");
        respondWithError(clientSocket, E_TECHNICAL);
        close(clientSocket);
        return 0;
    }

    if (getGameStatus() == GAME_IN_PROGRESS) {
        printf("Cannot add player to game - game in progress\n");
        respondWithError(clientSocket, E_GAME_IN_PROGRESS);
        close(clientSocket);
        return 0;
    }

    playerPosition = getRandomFreePosition();
    if (playerPosition < 0) {
        printf("Cannot add player to game - game full\n");
        respondWithError(clientSocket, E_GAME_IN_PROGRESS);
        close(clientSocket);
        return 0;
    }

    if (usernameTaken(&joinGameRequest[1])) {
        printf("Cannot add player to game - username taken\n");
        respondWithError(clientSocket, E_USERNAME_TAKEN);
        close(clientSocket);
        return 0;
    }

    strcpy(g_players[playerPosition].username, &joinGameRequest[1]);
    g_players[playerPosition].socket = clientSocket;
    g_connectedPlayerCount++;

    printf("Player successfully added to game\n");
    return 1;
}

void loadPlayerData() {
    int i;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        g_players[i].points = 1;
        g_players[i].position.rowPosition = g_mapData.startPositions[i].rowPosition;
        g_players[i].position.columnPosition = g_mapData.startPositions[i].columnPosition;
    }
}

/* Parses the given map row to find player symbols to know where they spawn */
int checkAndSetSpawnPositions(char *mapRow, int rowPosition) {
    int i;
    for (i = 0; i < cfg.mapWidth; i++) {
        int cellValue = (int) mapRow[i];
        if (cellValue >= 65 && cellValue <= 72) {
            int index = cellValue - 65;
            if (index > MAX_PLAYER_COUNT - 1) {
                fprintf(stderr, "Mismatching max player count (%d) and player start position symbol %c\n", MAX_PLAYER_COUNT, mapRow[i]);
                return 0;
            }
            g_mapData.startPositions[index].rowPosition = rowPosition;
            g_mapData.startPositions[index].columnPosition = i;
            mapRow[i] = ' ';
        }
    }

    return 1;
}

void allocateMap() {
    int i;

    /* Allocate map */
    g_mapData.map = malloc(cfg.mapHeight * sizeof(char *));
    if (g_mapData.map == NULL) {
        fprintf(stderr, "Memory allocation of map failed\n");
        exitHandler(1);
    }
    for(i = 0; i < cfg.mapHeight; i++) {
        g_mapData.map[i] = malloc(cfg.mapWidth * sizeof(char) + 3);
        if (g_mapData.map[i] == NULL) {
            fprintf(stderr, "Memory allocation of map row failed\n");
            exitHandler(1);
        }
        memset(g_mapData.map[i], 0, cfg.mapWidth * sizeof(char) + 2);
    }

    /* Allocate food positions */
    g_mapData.foodPositions = malloc(cfg.foodCount * sizeof(struct Position));
    if (g_mapData.foodPositions == NULL) {
        fprintf(stderr, "Memory allocation of food positions failed\n");
        exitHandler(1);
    }
    memset(g_mapData.foodPositions, 0, cfg.foodCount * sizeof(struct Position));

    /* Allocate start positions */
    g_mapData.startPositions = malloc(MAX_PLAYER_COUNT * sizeof(struct Position));
    if (g_mapData.startPositions == NULL) {
        fprintf(stderr, "Memory allocation of start positions failed\n");
        exitHandler(1);
    }
    memset(g_mapData.startPositions, 0, MAX_PLAYER_COUNT * sizeof(struct Position));
}

void loadMapData() {
    FILE *mapFile = NULL;
    int i;

    printf("Loading map data\n");
    allocateMap();

    mapFile = fopen(cfg.mapFilename, "r");
    if (mapFile == NULL) {
        fprintf(stderr, "Could not open map file '%s': ", cfg.mapFilename);
        perror("");
        exitHandler(1);
    }

    for (i = 0; i < cfg.mapHeight; i++) {
        int rowLength;
        if (getLine(g_mapData.map[i], cfg.mapWidth * sizeof(char) + 3, mapFile) == NULL) {
            if (i != cfg.mapHeight - 1) {
                fprintf(stderr, "Mismatching map height and configured height\n");
                fclose(mapFile);
                exitHandler(1);
            }
            break;
        }

        rowLength = strlen(g_mapData.map[i]);
        if (rowLength != cfg.mapWidth) {
            fprintf(stderr, "Mismatching map width and configured width\n");
            fclose(mapFile);
            exitHandler(1);
        }

        if (checkAndSetSpawnPositions(g_mapData.map[i], i) == 0) {
            fclose(mapFile);
            exitHandler(1);
        }
    }

    fclose(mapFile);

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        struct Position *startPosition = &g_mapData.startPositions[i];
        if (startPosition->columnPosition == 0 && startPosition->rowPosition == 0) {
            fprintf(stderr, "All spawn points not set\n");
            exitHandler(1);
        }
    }

    printMap();
}

void init() {
    int enable = 1;
    struct sockaddr_in serverAddress;
    struct sigaction sigCfg;

    printf("Initializing server\n");

    /* Initialize locks */
    if (pthread_mutex_init(&g_moveLock, NULL) != 0) {
        fprintf(stderr, "Error initializing move lock\n");
        exit(1);
    }
    if (pthread_mutex_init(&g_gameStatusLock, NULL) != 0) {
        fprintf(stderr, "Error initializing game status lock\n");
        exit(1);
    }
    if (pthread_mutex_init(&g_threadStatusLock, NULL) != 0) {
        fprintf(stderr, "Error initializing thread status lock\n");
        exit(1);
    }

    /* Load initial configuration */
    if (loadCfg() < 0) {
        fprintf(stderr, "Error loading configuration\n");
        exit(1);
    }
    loadMapData();
    loadPlayerData();
    if (initMoveQueue(MAX_PLAYER_COUNT) < 0) {
        fprintf(stderr, "Error initializing move queue\n");
        exitHandler(1);
    }

    /* Initialization for setting players random positions and food generation later,
       should only be called once */
    srand(time(NULL));

    /* set sigIntHandler to handle signals */
    sigCfg.sa_handler = sigIntHandler;
    sigemptyset(&sigCfg.sa_mask);
    sigCfg.sa_flags = 0;
    sigaction(SIGINT, &sigCfg, NULL);

    /* Initialize main server socket */
    g_netSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_netSocket < 0) {
        perror("Error creating server socket");
        exitHandler(1);
    }
    /* Set main server socket as reusable so reboots work */
    if (setsockopt(g_netSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("Error on setting socket to SO_REUSEADDR");
        exitHandler(1);
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(cfg.serverPort);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(g_netSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        perror("Error binding server socket");
        exitHandler(1);
    }

    if (listen(g_netSocket, 30) < 0) {
        perror("Error listening on server socket");
        exitHandler(1);
    }
    printf("Listening...\n");
}

int main() {
    init();

    int ret;
    int gameStartTickCount = 0;
    int gameStartTickTimeout = cfg.gameStartTimeout * 10;
    struct pollfd pollList[1];
    pollList[0].fd = g_netSocket;
    pollList[0].events = POLLIN;

    /* Main connection accepting loop. Uses polling instead of waiting for accepts,
       because it needs to check whether any threads failed and it should stop */
    while (1) {
        ret = poll(pollList, 1, 100);
        if(ret < 0) {
            perror("Error starting polling for server socket");
            exitHandler(1);
        }

        if (getThreadStatus() == THREAD_ERRORED) {
            exitHandler(1);
        }

        if (ret == 0) { /* No clients to accept */
            if (cfg.gameStartTimeout != 0 && getGameStatus() == GAME_NOT_STARTED && gameStartTickCount < gameStartTickTimeout) {
                gameStartTickCount++;
            }
        } else {
            if ((pollList[0].revents & POLLHUP) == POLLHUP || (pollList[0].revents & POLLERR) == POLLERR ||
            (pollList[0].revents & POLLNVAL) == POLLNVAL) {
                printf("Error polling server socket\n");
                exitHandler(1);
            } else if ((pollList[0].revents & POLLIN) == POLLIN) {
                int clientSocket = accept(g_netSocket, NULL, NULL);
                if (clientSocket < 0) {
                    perror("Error accepting client");
                    exitHandler(1);
                }
                printf("Client connected\n");

                if (addClientToGame(clientSocket) == 1) {
                    sendLobbyInfoMessage();
                    if (cfg.gameStartTimeout != 0) {
                        gameStartTickCount = 0;
                    }
                }
            }
        }

        if (getGameStatus() == GAME_NOT_STARTED &&
        (g_connectedPlayerCount == MAX_PLAYER_COUNT ||
        (cfg.gameStartTimeout != 0 && gameStartTickCount == gameStartTickTimeout && g_connectedPlayerCount >= 2))) {
            startGame();
            gameStartTickCount = 0;
        }
    }

    /* Technically not possible to reach this part, but be ready just in case */
    exitHandler(0);

    return 0;
}
