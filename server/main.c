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
#define MAX_PLAYER_COUNT 2
#define PLAYER_COUNT_SIZE 1
#define MAX_MAP_HEIGHT 999
#define MAX_MAP_WIDTH 999
#define MAP_HEIGHT_SIZE 3
#define MAP_WIDTH_SIZE 3
#define X_COORD_SIZE 3
#define Y_COORD_SIZE 3
#define MOVE_SIZE 1
#define MAX_FOOD_COUNT 20
#define FOOD_SIZE 3
#define FOOD_RESPAWN_TRESHOLD 5

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

struct ThreadArgs {
    int resolverStatus;
    int setterStatus;
};

int netSocket;

int connectedPlayerCount = 0;
struct Position startPositions[MAX_PLAYER_COUNT];
struct PlayerData players[MAX_PLAYER_COUNT];

int mapWidth = 0;
int mapHeight = 0;
char mapState[MAX_MAP_HEIGHT][MAX_MAP_WIDTH + 2];

int gameStarted = 0;
int foodCount = MAX_FOOD_COUNT;
struct Position foodPositions[MAX_FOOD_COUNT];

pthread_t moveResolverThread;
pthread_t moveSetterThread;
pthread_mutex_t lock;

void exitHandler(int sig);
void handleDisconnect(int socket);
int socketSend(int socket, char *message, int messageSize);
int socketReceive(int socket, char *buff, int messageSize);
void sendToAll(char *message, int size);
void respondWithError(int clientSocket, char errorType);
int addUsernames(char *buff);
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
void sendLobbyInfoMessage();
int addClientToGame(int clientSocket);
void checkAndSetSpawnPositions(char *mapRow, int rowLength, int rowPosition);
void printMap();
void loadMap();
void init();

void exitHandler(int sig) {
    int i;
    printf("Entered exit handler\n");

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].socket > 0) {
            close(players[i].socket);
        }
    }

    if (netSocket > 0) {
        close(netSocket);
    }

    pthread_mutex_destroy(&lock);

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
    actualSize += sprintf(&gameStartMessage[actualSize], "%03d%03d", mapWidth, mapHeight);

    sendToAll(gameStartMessage, actualSize);
}

void sendMap() {
    int actualSize = 4 + mapWidth;
    int i;
    int j;
    for (i = 0; i < mapHeight; i++) {
        char mapMessage[MAP_MSG_SIZE] = "";
        char *mapRowPtr = NULL;
        sprintf(mapMessage, "%c%03d%s", S_MAP_ROW, i + 1, mapState[i]);
        mapRowPtr = &mapMessage[4];
        for (j = 0; j < mapWidth; j++) {
            if ((int) mapRowPtr[j] >= 65 && (int) mapRowPtr[j] <= 72) {
                mapRowPtr[j] = ' ';
            }
        }
        sendToAll(mapMessage, actualSize);
    }
}

int addFoodData(char *buff) {
    int size = 0;
    int i;
    char *ptr = buff;
    for (i = 0; i < MAX_FOOD_COUNT; i++) {
        struct Position *position = &foodPositions[i];
        if (position->rowPosition == 0 && position->columnPosition == 0) {
            continue;
        }

        sprintf(ptr, "%03d%03d", position->columnPosition, position->rowPosition);
        ptr += 6;
        size +=6;
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
    actualSize += sprintf(&gameUpdateMessage[actualSize], "%03d", foodCount);
    actualSize += addFoodData(&gameUpdateMessage[actualSize]);

    sendToAll(gameUpdateMessage, actualSize);
}

void generateFood() {
    int i;
    for (i = 0; i < MAX_FOOD_COUNT; i++) {
        struct Position *position = &foodPositions[i];
        if (position->rowPosition == 0 && position->columnPosition == 0) {
            int randRowPos = 0;
            int randColPos = 0;
            do {
                randRowPos = rand() % (mapHeight - 2) + 1;
                randColPos = rand() % (mapWidth - 2) + 1;
            } while (mapState[randRowPos][randColPos] != ' ');

            mapState[randRowPos][randColPos] = '!';
            position->rowPosition = randRowPos;
            position->columnPosition = randColPos;
        }
    }
}

int gameEnded() {
    return 0;
}

void resolveIncomingMoves(int *threadStatus) {
    int i;
    while(1) {
        if (*threadStatus == THREAD_ERRORED) {
            return;
        }

        sendGameUpdateMessage();
        usleep(100000);
        pthread_mutex_lock(&lock);

        for (i = 0; i < MAX_PLAYER_COUNT; i++) {
            char playerSymbol;
            struct PlayerData *player = &players[i];
            struct Position *playerPosition = &player->position;
            char targetSymbol;
            int targetRowModifier = 0;
            int targetColModifier = 0;

            if (player->requestedMove == 0) {
                continue;
            }

            playerSymbol = 65 + i;
            switch (player->requestedMove) {
                case 1:
                    if (playerPosition->rowPosition != 0) {
                        targetRowModifier = -1;
                    }
                    break;
                case 2:
                    if (playerPosition->rowPosition != mapHeight - 1) {
                        targetRowModifier = 1;
                    }
                    break;
                case 3:
                    if (playerPosition->columnPosition != mapWidth - 1) {
                        targetColModifier = 1;
                    }
                    break;
                case 4:
                    if (playerPosition->columnPosition != 0) {
                        targetColModifier = -1;
                    }
                    break;
            }

            if (targetRowModifier != 0 && targetColModifier != 0) {
                targetSymbol = mapState[playerPosition->rowPosition + targetRowModifier][playerPosition->columnPosition + targetColModifier];
                if (targetSymbol == ' ') {
                    mapState[playerPosition->rowPosition][playerPosition->columnPosition] = ' ';
                    mapState[playerPosition->rowPosition + targetRowModifier][playerPosition->columnPosition + targetColModifier] = playerSymbol;
                    playerPosition->rowPosition += targetRowModifier;
                    playerPosition->columnPosition += targetColModifier;
                } else if (targetSymbol == '!') {
                    mapState[playerPosition->rowPosition][playerPosition->columnPosition] = ' ';
                    mapState[playerPosition->rowPosition + targetRowModifier][playerPosition->columnPosition + targetColModifier] = playerSymbol;
                    playerPosition->rowPosition += targetRowModifier;
                    playerPosition->columnPosition += targetColModifier;
                    player->points++;
                } else if ((int) targetSymbol >= 65 && (int) targetSymbol <= 72) {
                    int targetPlayerIndex = (int) targetSymbol - 65;
                    struct PlayerData *targetPlayer = &players[targetPlayerIndex];
                    if (targetPlayer->points > player->points) {
                        targetPlayer->points += player->points;
                        player->points = 0;
                        mapState[playerPosition->rowPosition][playerPosition->columnPosition] = ' ';
                    } else if (targetPlayer->points < player->points) {
                        player->points += targetPlayer->points;
                        targetPlayer->points = 0;
                        mapState[playerPosition->rowPosition + targetRowModifier][playerPosition->columnPosition + targetColModifier] = playerSymbol;
                    } else {
                        printf("Player collision, both players have the same points, ignoring\n");
                    }
                } else {
                    printf("Tagret position is in wall, ignoring\n");
                }

                if (foodCount <= FOOD_RESPAWN_TRESHOLD) {
                    generateFood();
                } else if (gameEnded()) {
                    *threadStatus = THREAD_COMPLETED;
                }
            }
        }

        pthread_mutex_unlock(&lock);
        if (*threadStatus == THREAD_COMPLETED) {
            return;
        }
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
    int *threadStatus = args;

    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        pollList[i].fd = players[i].socket;
        pollList[i].events = POLLIN;
    }

    while(1) {
        if (*threadStatus == THREAD_ERRORED || *threadStatus == THREAD_COMPLETED) {
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

void *handleGameStart(void *args) {
    int ret;
    int *threadStatus = args;

    sendGameStartMessage();
    sendMap();
    generateFood();

    ret = pthread_create(&moveSetterThread, NULL, setIncomingMoves, threadStatus);
    if (ret != 0) {
        perror("Failed to create moveSetterThread");
        *threadStatus = THREAD_ERRORED;
        return NULL;
    }

    resolveIncomingMoves(threadStatus);
    pthread_join(moveSetterThread, NULL);

    if (*threadStatus == THREAD_COMPLETED) {
        /* Reset game here */
    }

    return NULL;
}

void startGame() {
    int ret;
    gameStarted = 1;
    int threadStatus = THREAD_RUNNING;

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

void checkAndSetSpawnPositions(char *mapRow, int rowLength, int rowPosition) {
    int i;
    for (i = 0; i < rowLength; i++) {
        int cellValue = (int) mapRow[i];
        if (cellValue >= 65 && cellValue <= 72) {
            int index = cellValue - 65;
            startPositions[index].rowPosition = rowPosition;
            startPositions[index].columnPosition = i;
            players[index].position.rowPosition = rowPosition;
            players[index].position.columnPosition = i;
            players[index].points = 1; /* Move this down later */
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
        fprintf(stderr, "Could not open map file '%s': ", cfg.mapFilename);
        perror("");
        exitHandler(1);
    }

    for (i = 0; i < MAX_MAP_HEIGHT; i++) {
        int rowLength;
        if (getLine(mapState[i], sizeof(mapState[i]), mapFile) == NULL) {
            if (mapWidth == 0) {
                fprintf(stderr, "A map can not be empty\n");
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
            fprintf(stderr, "A map can not contain different width rows\n");
            fclose(mapFile);
            exitHandler(1);
        }

        checkAndSetSpawnPositions(mapState[i], rowLength, i);
    }

    fclose(mapFile);
    printMap();
}

void init() {
    int enable = 1;
    struct sockaddr_in serverAddress;
    struct sigaction sigIntHandler;

    /* load initial configuration */
    loadCfg();
    loadMap();

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
