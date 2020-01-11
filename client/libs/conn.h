#ifndef CONN_H
#define CONN_H

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

enum Direction {
    UP = 'U',
    LEFT = 'L',
    DOWN = 'D',
    RIGHT = 'R'
};

int netSock;

enum MsgType getMsgType(char *);
void sockCreate();
int sockConn(char *, int);
int sockSendJoinGame(char *);
int sockSendMove(enum Direction c);
int sockRecvJoinGameResp(char *);
int sockRecvLobbyInfo(char *);
int sockRecvMapRow(char *, int);
int sockRecvGameUpdate(char *);

#endif /* CONN_H */
