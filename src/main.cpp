
#include <windows.h>
#include <cstdio>
#include "include/Game.hpp"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    if (0) {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONIN$", "r", stdin);
    }

    Game game;
    game.run();

    return 0;
}
