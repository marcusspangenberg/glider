#pragma once

#include "FileBrowser.h"
#include "Preferences.h"
#include "Resources.h"
#include <SDL2/SDL.h>
#include <string>

class Renderer;

enum class TitleScreenResult
{
    newGame,
    loadHouse,
    quit,
};

struct TitleScreenAction
{
    TitleScreenResult result = TitleScreenResult::quit;
    std::string housePath;
};

class TitleScreen
{
public:
    TitleScreen(SDL_Window* window,
        SDL_Renderer* renderer,
        Preferences& prefs,
        Resources* resources,
        Renderer& gameRenderer);
    TitleScreenAction run();

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    Preferences& prefs_;
    bool running_ = true;
    TitleScreenAction pendingAction_ {};
    bool showAboutDialog_ = false;
    bool showPrefsDialog_ = false;
    bool prefsAirflow_ = false;
    bool prefsMusicEnabled_ = true;
    SDL_Scancode prefsKeyLeft_ = SDL_SCANCODE_LEFT;
    SDL_Scancode prefsKeyRight_ = SDL_SCANCODE_RIGHT;
    SDL_Scancode prefsKeyThrust_ = SDL_SCANCODE_UP;
    SDL_Scancode prefsKeyFireBand_ = SDL_SCANCODE_SPACE;
    int32_t rebindingControl_ = -1;
    Resources* resources_ = nullptr;
    Renderer& gameRenderer_;
    FileBrowser fileBrowser_;

    void processEvents();
    void drawUI();
    void drawAboutDialog();
    void drawCatalog() const;
};
