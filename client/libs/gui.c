#include "gui.h"

#include <ctype.h>
#include <curses.h>
#include <string.h>

#define ALT_KEY_BACKSPACE 127

void initGui() {
    initscr();
    getmaxyx(stdscr, maxY, maxX);
    curs_set(0);
}

void endGui() {
    endwin();
}

void displayStr(char *msg, ...) {
    erase();
    mvprintw(maxY / 2, (maxX - strlen(msg)) / 2, "%s", msg);
    refresh();
}

void displayTitle() {
    erase();
    mvprintw((maxY / 2) - 2, (maxX - 63) / 2, "    __  ___");
    mvprintw((maxY / 2) - 1, (maxX - 63) / 2, "   /  |/  /___ _____  ___     _______  ______  ____  ___  _____");
    mvprintw((maxY / 2), (maxX - 63) / 2, "  / /|_/ / __ `/_  / / _ \\   / ___/ / / / __ \\/ __ \\/ _ \\/ ___/");
    mvprintw((maxY / 2) + 1, (maxX - 63) / 2, " / /  / / /_/ / / /_/  __/  / /  / /_/ / / / / / / /  __/ /");
    mvprintw((maxY / 2) + 2, (maxX - 63) / 2, "/_/  /_/\\__,_/ /___/\\___/  /_/   \\__,_/_/ /_/_/ /_/\\___/_/");
    mvprintw((maxY / 2) + 4, (maxX - 18) / 2, "<Press any button>");
    move(maxY, maxX);
    refresh();
    getch();
}

void displayUnamePrompt() {
    erase();
    char displayTxt[] = "Enter username: ";
    mvprintw(maxY / 2, (maxX - strlen(displayTxt) - MAX_UNAME_SIZE) / 2, "%s", displayTxt);
    refresh();
}

void displayConnError() {
    char displayTxt1[] = "Connection Failure";
    char displayTxt2[] = "<Press any button to try connecting again>";
    erase();
    mvprintw(maxY / 2, (maxX - strlen(displayTxt1)) / 2, "%s", displayTxt1);
    mvprintw(maxY / 2 + 1, (maxX - strlen(displayTxt2)) / 2, "%s", displayTxt2);
    refresh();
    getch();
}

void displayGameInProgress() {
    char displayTxt1[] = "Game already in progress";
    char displayTxt2[] = "<Press any button to join again>";
    erase();
    mvprintw(maxY / 2, (maxX - strlen(displayTxt1)) / 2, "%s", displayTxt1);
    mvprintw(maxY / 2 + 1, (maxX - strlen(displayTxt2)) / 2, "%s", displayTxt2);
    refresh();
    getch();
}

void displayUnameTaken() {
    char displayTxt1[] = "Username already taken";
    char displayTxt2[] = "Enter new username: ";
    erase();
    mvprintw(maxY / 2, (maxX - strlen(displayTxt1)) / 2, "%s", displayTxt1);
    mvprintw(maxY / 2 + 2, (maxX - strlen(displayTxt2) - MAX_UNAME_SIZE) / 2, "%s", displayTxt2);
    refresh();
}

void displayLobbyInfo(int playerCnt, char players[MAX_PLAYER_CNT][MAX_UNAME_SIZE + 1]) {
    erase();
    mvprintw(maxY / 2, (maxX - 12) / 2, "Players: %d/%d\n", playerCnt, MAX_PLAYER_CNT);
    for (int i = 0; i < playerCnt; i++) {
        mvprintw(maxY / 2 + 2 + i, (maxX - 16) / 2, "%s", players[i]);
    }
    refresh();
}

void getUname(char *uname, int unameSize) {
    char c;
    int i;
    int y, x;

    memset(uname, 0, unameSize);

    curs_set(1);
    noecho();
    keypad(stdscr, TRUE);
    attron(A_BOLD);

    i = 0;
    do {
        c = getch();
        if (i < (unameSize - 1) && (isalnum(c) || c == '_')) {
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
    } while (c != '\n');

    attroff(A_BOLD);
    keypad(stdscr, FALSE);
    echo();
    curs_set(0);
}