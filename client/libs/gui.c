#include "gui.h"

#include <ncurses.h>

void printTitle(int row, int col) {
    erase();
    mvprintw((row / 2) - 2, (col - 63) / 2, "%s", "    __  ___");
    mvprintw((row / 2) - 1, (col - 63) / 2, "%s", "   /  |/  /___ _____  ___     _______  ______  ____  ___  _____");
    mvprintw((row / 2), (col - 63) / 2, "%s", "  / /|_/ / __ `/_  / / _ \\   / ___/ / / / __ \\/ __ \\/ _ \\/ ___/");
    mvprintw((row / 2) + 1, (col - 63) / 2, "%s", " / /  / / /_/ / / /_/  __/  / /  / /_/ / / / / / / /  __/ /");
    mvprintw((row / 2) + 2, (col - 63) / 2, "%s", "/_/  /_/\\__,_/ /___/\\___/  /_/   \\__,_/_/ /_/_/ /_/\\___/_/");
    mvprintw((row / 2) + 4, (col - 18) / 2, "%s", "<Press any button>");
    refresh();
}