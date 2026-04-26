#pragma once

#include "FileBrowser.h"
#include "GameState.h"
#include "HouseData.h"
#include "HouseProgress.h"
#include "ObjectType.h"
#include "PlayerState.h"
#include "Renderer.h"
#include "Resources.h"
#include <imgui.h>
#include <optional>
#include <SDL2/SDL.h>
#include <string>
#include <utility>

class Editor
{
public:
    Editor(SDL_Window* window, SDL_Renderer* renderer, std::string_view imagesDir, std::string_view gameExecutablePath);
    ~Editor();
    void run();
    void openHouse(const std::string& path);

private:
    enum class DragMode
    {
        none,
        move,
        resizeTopLeft,
        resizeTopRight,
        resizeBottomLeft,
        resizeBottomRight,
        resizeAmount,
    };

    enum class PendingAction
    {
        none,
        quit,
        newHouse,
        openHouse,
    };

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdlRenderer_ = nullptr;
    Resources resources_;
    Renderer previewRenderer_;

    HouseRec house_ {};
    bool houseLoaded_ = false;
    std::string housePath_;
    std::string imagesDir_;
    std::string gameExecutablePath_;
    bool dirty_ = false;

    size_t selectedRoomIndex_ = 0;
    int32_t selectedObjectIndex_ = -1;
    ObjectType activePaletteTool_ = ObjectType::nullObject;

    GameState staticGameState_;
    HouseProgress staticHouseProgress_;
    PlayerState staticPlayerState_;

    SDL_Texture* previewTexture_ = nullptr;
    SDL_Texture* paletteIconsTexture_ = nullptr;
    float previewScale_ = 2.0f;
    ImVec2 previewImageOrigin_ {};

    DragMode dragMode_ = DragMode::none;
    float dragOffsetX_ = 0.0f;
    float dragOffsetY_ = 0.0f;

    bool running_ = true;
    bool quitRequested_ = false;
    FileBrowser fileBrowser_;
    std::string statusMessage_;

    PendingAction pendingAction_ = PendingAction::none;
    bool pendingUnsavedDialog_ = false;

    void processEvents();
    void renderPreview();
    void drawInvisibleObjectOverlays();
    void drawAmountOverlays();
    void drawSelectionOverlay();
    void drawUI();
    void drawMenuBar();
    void drawRoomListPanel(float menuBarHeight, float windowHeight);
    void drawCenterPanel(float menuBarHeight, float windowHeight, float leftWidth, float rightWidth);
    void drawRightPanel(float menuBarHeight, float windowHeight, float rightWidth);
    void handlePreviewClick(float roomX, float roomY);
    void handlePreviewDrag(float roomX, float roomY);
    void placeObject(float roomX, float roomY);
    void selectObjectAt(float roomX, float roomY);

    RoomData& currentRoom();
    const RoomData& currentRoom() const;
    void updateStaticGameState();
    void saveHouse();
    void saveHouseAs(const std::string& path);
    void testHouse(std::optional<size_t> startRoom);
    void newHouse();
    void confirmUnsaved(PendingAction action);
    void executePendingAction();
    void drawUnsavedChangesDialog();
    void addRoom();
    void removeRoom(size_t index);

    std::pair<int32_t, int32_t> defaultObjectSize(ObjectType type) const;
};
