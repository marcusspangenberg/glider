#pragma once

#include "EndScreenAnimState.h"
#include "GameOverState.h"
#include "HouseProgress.h"
#include "PlayerProgress.h"
#include "Resources.h"
#include <memory>
#include <SDL2/SDL.h>

struct RoomData;
struct ObjectData;
struct PlayerState;
struct GameState;

class Renderer
{
public:
    Renderer(SDL_Renderer* sdlRenderer, Resources& resources, SDL_Rect screenRect, int32_t renderScale = 1);
    ~Renderer();

    std::unique_ptr<Resources> loadResources;

    void beginFrame() const;
    void drawRoom(const GameState& gameState, const HouseProgress& progress, const RoomData& roomData) const;
    void drawObjects(const GameState& gameState,
        const HouseProgress& progress,
        const RoomData& roomData,
        const PlayerState& playerState,
        float alpha) const;
    void drawEnemies(const GameState& gameState, const RoomData& roomData, float alpha) const;
    void drawGlider(const PlayerState& playerState, float alpha) const;
    void drawBand(const PlayerState& playerState) const;
    void drawStatusBar(const PlayerProgress& progress, const RoomData& roomData, size_t roomNumber) const;
    void drawEndScreen(const EndScreenAnimState& animState) const;
    void drawGameOverScreen(const PlayerProgress& progress, const GameOverAnimState& animState) const;
    void drawTimeBonusOverlay(int32_t amount) const;
    void endOfFrame() const;
    void setShowAirflow(bool show);
    void updateForWindowSize(int32_t windowWidth, int32_t windowHeight);
    int32_t getRenderScale(int32_t windowWidth, int32_t windowHeight) const;

    static int32_t drawOrderPriority(const ObjectData& obj);

private:
    SDL_Renderer* sdlRenderer_ = nullptr;
    Resources& resources_;
    SDL_Rect screenRect_;
    SDL_Rect physScreenRect_;
    int32_t renderScale_ = 1;
    mutable SDL_Rect viewport_ = {};
    SDL_Texture* glyphAtlasSans_ = nullptr;
    bool showAirflow_ = false;

    void renderText(std::string_view text, SDL_Color color, SDL_Rect clipRect, int32_t align) const;

    void drawTable(const ObjectData& object) const;
    void drawShelf(const ObjectData& object) const;
    void drawCabinet(const ObjectData& object) const;
    void drawMirror(const ObjectData& object, const PlayerState& playerState, float alpha) const;
    void drawWindow(const ObjectData& object,
        const GameState& gameState,
        size_t objectIndex,
        bool frameVisible = true) const;
    void drawGrease(const ObjectData& objectData, const GameState& gameState, size_t objectIndex) const;
    void drawCandle(const ObjectData& object, uint32_t animTick, bool bodyVisible = true) const;
    void drawOutlet(const ObjectData& object, const GameState& gameState, size_t objectIndex) const;
    void drawDrip(const ObjectData& object, const GameState& gameState, size_t objectIndex, float alpha) const;
    void drawBall(const ObjectData& object, const GameState& gameState, size_t objectIndex, float alpha) const;
    void drawFishBowl(const ObjectData& object,
        const GameState& gameState,
        size_t objectIndex,
        float alpha,
        bool bowlVisible = true) const;
    void drawToaster(const ObjectData& object,
        const GameState& gameState,
        size_t objectIndex,
        float alpha,
        bool bodyVisible = true) const;
    void drawDebugRect(const ObjectData& object) const;
    void drawAirflow(const RoomData& roomData, const GameState& gameState, const HouseProgress& progress) const;
    [[nodiscard]] SDL_Rect mapRect(SDL_Rect rect) const;
};
