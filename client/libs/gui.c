#include "gui.h"

#include <ctype.h>
#include <ncurses.h>
#include <string.h>

#define ALT_KEY_BACKSPACE 127

int mapOriginY;
int mapOriginX;

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
    guiPrintLineMid(1, "----------------");
    for (int i = 0; i < playerCnt; i++) {
        mvprintw((scrCtrY + 2 + i), (scrCtrX - (MAX_UNAME_SIZE / 2)), players[i].uname);
    }
    refresh();
}

void displayMap(int mapHeight, int mapWidth, char mapState[MAX_MAP_HEIGHT][MAX_MAP_WIDTH + 1]) {
    mapOriginY = ((scrHeight - mapHeight) / 2);
    mapOriginX = ((scrWidth - mapWidth) / 2);

    erase();
    for (int i = 0; i < mapHeight; i++) {
        mvprintw((mapOriginY + i), mapOriginX, mapState[i]);
    }
    refresh();
}

void updateMap(struct Player players[], int playerCnt, struct Food food[], int foodCnt) {

    for (int i = 0; i < playerCnt; i++) {
        mvprintw((mapOriginY + players[i].pos.y), (mapOriginX + players[i].pos.x), "%c", ('A' + i));
        // mvprintw((mapOriginY + i), (mapOriginX + 2), "%c", ('A' + i));
    }
    for (int i = 0; i < foodCnt; i++) {
        mvprintw((mapOriginY + food[i].pos.y), (mapOriginX + food[i].pos.x), "@");
        // mvprintw((mapOriginY + i), (mapOriginX), "@");
    }
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