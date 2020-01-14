#include "gui.h"

#include <ctype.h>
#include <ncurses.h>
#include <string.h>
 #include <unistd.h>

#define ALT_KEY_BACKSPACE 127

int mapOriginY;
int mapOriginX;
int mapHeight;
int mapWidth;

void guiStart() {
    initscr();
    getmaxyx(stdscr, scrHeight, scrWidth);
    scrCtrY = scrHeight / 2;
    scrCtrX = scrWidth / 2;
    noecho();
    curs_set(0);
}

void guiEnd() {
    endwin();
}

void guiPrintLineMid(int offsCtrY, char *str) {
    mvprintw((scrCtrY + offsCtrY), (scrCtrX -(strlen(str)) / 2), str);
}

void displayStr(char *msg) {
    erase();
    guiPrintLineMid(0, msg);
    refresh();
}

void displayMainScreen() {
    erase();
    guiPrintLineMid(-6, "    __  ___                                                    ");
    guiPrintLineMid(-5, "   /  |/  /___ _____  ___     _______  ______  ____  ___  _____");
    guiPrintLineMid(-4, "  / /|_/ / __ `/_  / / _ \\   / ___/ / / / __ \\/ __ \\/ _ \\/ ___/");
    guiPrintLineMid(-3, " / /  / / /_/ / / /_/  __/  / /  / /_/ / / / / / / /  __/ /    ");
    guiPrintLineMid(-2, "/_/  /_/\\__,_/ /___/\\___/  /_/   \\__,_/_/ /_/_/ /_/\\___/_/     ");
    guiPrintLineMid(0, "<Press any button>");
    move(scrHeight, scrWidth);
    refresh();
    getch();
}

void displayUnamePrompt() {
    erase();
    mvprintw(scrCtrY, (scrCtrX - ((16 + MAX_UNAME_SIZE) / 2)), "Enter username: ");
    refresh();
}

void displayConnError() {
    erase();
    guiPrintLineMid(0, "Connection Failure");
    guiPrintLineMid(1, "<Press any button to try connecting again>");
    refresh();
    getch();
}

void displayGameInProgress() {
    erase();
    guiPrintLineMid(0, "Game already in progress");
    guiPrintLineMid(1, "<Press any button to join again>");
    refresh();
    getch();
}

void displayUnameTaken() {
    erase();
    guiPrintLineMid(0, "Username already taken");
    mvprintw((scrCtrY + 2), (scrCtrX - ((20 + MAX_UNAME_SIZE) / 2)), "Enter new username: ");
    refresh();
}

void displayLobbyInfo(int playerCnt, struct Player players[]) {
    erase();
    mvprintw(scrCtrY, (scrCtrX - (12 / 2)), "Players: %d/%d", playerCnt, MAX_PLAYER_CNT);
    guiPrintLineMid(1, "------------------");
    for (int i = 0; i < playerCnt; i++) {
        mvprintw((scrCtrY + 2 + i), (scrCtrX - (MAX_UNAME_SIZE / 2)), players[i].uname);
    }
    refresh();
}

void displayMap(char mapState[MAX_MAP_HEIGHT][MAX_MAP_WIDTH + 1], int _mapHeight, int _mapWidth, struct Player players[], int playerCnt) {
    mapHeight = _mapHeight;
    mapWidth = _mapWidth;
    mapOriginY = ((scrHeight - mapHeight) / 2);
    mapOriginX = ((scrWidth - mapWidth) / 2);

    erase();

    /* draw map */
    for (int i = 0; i < mapHeight; i++) {
        mvprintw((mapOriginY + i), mapOriginX, mapState[i]);
    }

    /* draw sidebar */
    mvprintw(mapOriginY, (mapOriginX - 28), "+------------------------+");
    for (int i = 0; i < ((MAX_PLAYER_CNT * 2) + 1); i++) {
        mvprintw((mapOriginY + 1 + i), (mapOriginX - 28), "|                        |");
    }
    mvprintw((mapOriginY + (MAX_PLAYER_CNT * 2) + 2), (mapOriginX - 28), "+------------------------+");
    /* write player usernames in the sidebar */
    for (int i = 0; i < playerCnt; i++) {
        mvprintw((mapOriginY + 2 + (2 * i)), (mapOriginX - 26), "%16s", players[i].uname);
    }
    /* write player letters in the sidebar */
    attron(A_BOLD);
    for (int i = 0; i < playerCnt; i++) {
        mvprintw((mapOriginY + 2 + (2 * i)), (mapOriginX - 9), "%c", ('A' + i));
    }
    attroff(A_BOLD);

    refresh();
}

void updateMap(struct Player players[], struct Player playersOld[], int playerCnt, struct Food food[], struct Food foodOld[], int foodCnt, int foodCntOld) {
    /* remove old player and food positions for redrawing */
    for (int i = 0; i < playerCnt; i++) {
        mvprintw((mapOriginY + playersOld[i].pos.y), (mapOriginX + playersOld[i].pos.x), " ");
    }
    for (int i = 0; i < foodCntOld; i++) {
        mvprintw((mapOriginY + foodOld[i].pos.y), (mapOriginX + foodOld[i].pos.x), " ");
    }

    /* draw player positions and points */
    attron(A_BOLD);
    for (int i = 0; i < playerCnt; i++) {
        if (players[i].points != 0) {
            mvprintw((mapOriginY + players[i].pos.y), (mapOriginX + players[i].pos.x), "%c", ('A' + i));
        }
        mvprintw((mapOriginY + 2 + (2 * i)), (mapOriginX - 7), "%3d", players[i].points);
    }
    attroff(A_BOLD);
    /* draw food */
    for (int i = 0; i < foodCnt; i++) {
        mvprintw((mapOriginY + food[i].pos.y), (mapOriginX + food[i].pos.x), "@");
    }

    refresh();
}

void displayScoreBoard(struct Player players[], int playerCnt) {
    erase();
    guiPrintLineMid(0, "FINAL SCORE");
    guiPrintLineMid(1, "----------------------");
    for (int i = 0; i < playerCnt; i++) {
        mvprintw((scrCtrY + 2 + i), (scrCtrX - ((MAX_UNAME_SIZE + 4) / 2)), "%16s %3d", players[i].uname, players[i].points);
    }
    refresh();
    getch();
}

void displayGameOver(int winStatus) {
    guiPrintLineMid(-((mapHeight / 2) + 4), "GAME OVER");
    attron(A_BOLD);
    switch (winStatus) {
        case -1:
            guiPrintLineMid(-((mapHeight / 2) + 3), "YOU LOST!");
            break;
        case 0:
            guiPrintLineMid(-((mapHeight / 2) + 3), "DRAW!");
            break;
        case 1:
            guiPrintLineMid(-((mapHeight / 2) + 3), "YOU WON!");
            break;
    }
    attroff(A_BOLD);
    refresh();
    sleep(2);
    guiPrintLineMid(-((mapHeight / 2) + 2), "<Press any key to continue>");
    refresh();
    flushinp();
    getch();
}

void displayYouLost() {
    guiPrintLineMid(-((mapHeight / 2) + 3), "YOU LOST!");
    refresh();
}

void getUname(char *uname) {
    char c;
    int i;
    int y, x;

    memset(uname, 0, MAX_UNAME_SIZE);

    curs_set(1);
    keypad(stdscr, TRUE);
    attron(A_BOLD);

    i = 0;
    do {
        c = getch();
        if (i < (MAX_UNAME_SIZE - 1) && (isalnum(c) || c == '_')) {
            printw("%c", c);
            uname[i] = c;
            i++;
        } else if ( i != 0 && ((int)c == KEY_BACKSPACE || c == ALT_KEY_BACKSPACE)) {
            getyx(stdscr, y, x);
            mvdelch(y, x - 1);
            uname[i] = '\0';
            i--;
        }
        refresh();
    } while (c != '\n' && strlen(uname) != 0);

    attroff(A_BOLD);
    keypad(stdscr, FALSE);
    curs_set(0);
}