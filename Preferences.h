#pragma once

#include <SDL2/SDL.h>

struct Preferences
{
    bool showAirflow = false;
    SDL_Scancode keyLeft = SDL_SCANCODE_LEFT;
    SDL_Scancode keyRight = SDL_SCANCODE_RIGHT;
    SDL_Scancode keyThrust = SDL_SCANCODE_UP;
    SDL_Scancode keyFireBand = SDL_SCANCODE_SPACE;

    static Preferences load();
    void save() const;
};
