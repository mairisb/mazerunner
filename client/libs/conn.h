#ifndef CONN_H
#define CONN_H

#define MAX_UNAME_SIZE 16

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

int netSock;

char getMsgType(char *);
void sockCreate();
int sockConn(char *, int);
int sockCreateConn(char *, int);
int socketSend(char *, int);
int sockSendJoinGame(char *);
void sockSendMove(char c);
int sockRecvLobbyInfo(char *);
int sockRecvMapRow(char *, int);
int sockRecvGameUpdate(char *);

#endif /* CONN_H */
