#pragma once

#include "EndScreenAnimState.h"
#include "GameOverState.h"
#include "GameState.h"
#include "HouseData.h"
#include "HouseProgress.h"
#include "PlayerProgress.h"
#include "PlayerState.h"
#include "Preferences.h"

#include <chrono>
#include <optional>
#include <SDL2/SDL.h>
#include <vector>

class Renderer;
class SoundResources;

enum class GameResult
{
    returnToTitle,
    quit,
};

enum class GamePhase
{
    playing,
    gameOver,
    endScreen,
    done,
};

class Game
{
public:
    Game(std::vector<HouseRec> houses,
        Renderer& renderer,
        SoundResources& sounds,
        const Preferences& prefs,
        size_t startRoom = 0);
    GameResult run();

private:
    static constexpr std::chrono::nanoseconds tickDuration {1000000000ULL / 30ULL};

    struct TimeBonusOverlay
    {
        int32_t amount = 0;
        int32_t ticksRemaining = 0;
    };

    std::vector<HouseRec> houses_;
    size_t houseIndex_ = 0;
    Renderer& renderer_;
    SoundResources& sounds_;
    SDL_Scancode keyLeft_ = SDL_SCANCODE_LEFT;
    SDL_Scancode keyRight_ = SDL_SCANCODE_RIGHT;
    SDL_Scancode keyThrust_ = SDL_SCANCODE_UP;
    SDL_Scancode keyFireBand_ = SDL_SCANCODE_SPACE;
    GameState gameState_;
    HouseProgress houseProgress_;
    PlayerState playerState_;
    PlayerProgress playerProgress_;
    GamePhase phase_ = GamePhase::playing;
    GameResult result_ = GameResult::returnToTitle;
    std::optional<GameOverAnimState> gameOverState_;
    std::optional<EndScreenAnimState> endScreenAnimState_;
    TimeBonusOverlay timeBonusOverlay_;
    std::chrono::steady_clock::time_point prevTime_ {};
    std::chrono::nanoseconds accumulator_ {0};

    bool advanceHouse();
    void killGlider(bool& roomTransition);
    void processEvents();
    void updateGliderMode(bool& roomTransition);
    void processInput();
    void updateObjects(bool& roomTransition);
    void updateEnemies(bool& roomTransition);
    void applyPhysics(bool& roomTransition);
    void updateBand();
    void draw(float alpha, int32_t timeBonusAmount) const;
    void savePrevPositions();
    void snapPrevPositions();
    void updateGameOverAnim();
    void updateEndScreenAnim();
};
