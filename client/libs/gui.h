#ifndef GUI_H
#define GUI_H

#include "misc.h"
#include "log.h"

void guiStart();
void guiEnd();
void displayStr(char *);
void displayMainScreen();
void displayUnamePrompt();
void displayConnError();
void displayGameInProgress();
void displayUnameTaken();
void displayLobbyInfo(int, struct Player []);
void displayMap(int, int, char [MAX_MAP_HEIGHT][MAX_MAP_WIDTH + 1]);
void updateMap(struct Player [], int, struct Food [], int);
void displayScoreBoard(struct Player [], int);
void getUname(char *);

int scrHeight, scrWidth;
int scrCtrY, scrCtrX;

#endif /* GUI_H */
