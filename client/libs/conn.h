#ifndef CONN_H
#define CONN_H

enum MsgType {
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

char getMsgType(char *);
int sockCreate();
void sockConn(int, char *, int);
int sockSend(int, char*);
int sockRecv(int, char*, short);
int sockSendJoinGame(int, char *);

#endif /* CONN_H */
