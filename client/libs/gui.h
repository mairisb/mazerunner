#ifndef GUI_H
#define GUI_H

#define MAX_PLAYER_CNT 8
#define MAX_UNAME_SIZE 16
#define MAX_MAP_HEIGHT 999
#define MAX_MAP_WIDTH 999

void initGui();
void endGui();
void displayStr(char *, ...);
void displayTitle();
void displayUnamePrompt();
void displayConnError();
void displayGameInProgress();
void displayUnameTaken();
void displayLobbyInfo(int, char [MAX_PLAYER_CNT][MAX_UNAME_SIZE + 1]);
void displayMap(int, int, char [MAX_MAP_HEIGHT][MAX_MAP_WIDTH + 1]);
void getUname(char *, int);

int maxY, maxX;

#endif /* GUI_H */
