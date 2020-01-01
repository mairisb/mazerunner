#include "gui.h"

#include <ctype.h>
#include <curses.h>
#include <string.h>

#define ALT_KEY_BACKSPACE 127

void initGui() {
    initscr();
    getmaxyx(stdscr, row, col);
    curs_set(0);
}

void endGui() {
    endwin();
}

void displayTitle() {
    erase();
    mvprintw((row / 2) - 2, (col - 63) / 2, "    __  ___");
    mvprintw((row / 2) - 1, (col - 63) / 2, "   /  |/  /___ _____  ___     _______  ______  ____  ___  _____");
    mvprintw((row / 2), (col - 63) / 2, "  / /|_/ / __ `/_  / / _ \\   / ___/ / / / __ \\/ __ \\/ _ \\/ ___/");
    mvprintw((row / 2) + 1, (col - 63) / 2, " / /  / / /_/ / / /_/  __/  / /  / /_/ / / / / / / /  __/ /");
    mvprintw((row / 2) + 2, (col - 63) / 2, "/_/  /_/\\__,_/ /___/\\___/  /_/   \\__,_/_/ /_/_/ /_/\\___/_/");
    mvprintw((row / 2) + 4, (col - 18) / 2, "<Press any button>");
    move(row, col);
    refresh();
    getch();
}

void displayStr(char *msg, ...) {
    erase();
    mvprintw(row / 2, (col - strlen(msg)) / 2, "%s", msg);
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