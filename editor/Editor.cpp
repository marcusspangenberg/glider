#include "editor/Editor.h"
#include "Constants.h"
#include "HouseReader.h"
#include "HouseWriter.h"
#include "ProcessLauncher.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <map>
#include <numeric>
#include <optional>
#include <ranges>
#include <SDL2/SDL.h>
#include <SDL_image.h>

namespace
{

enum class AmountLimitKind
{
    none, // no amount field
    fixed, // constant [amountMin, amountMax]
    upward, // [airMaxVert, boundRect.top]   — floorVent, candle, toaster, ball, fishBowl
    downward, // [boundRect.bottom, floorVert] — ceilingVent, ceilingDuct, drip
    leftward, // [0, boundRect.left]            — leftFan
    rightward, // [boundRect.right, roomWidth]   — rightFan, grease
};

struct PaletteItem
{
    std::string_view name;
    size_t iconIndex = 0;
    AmountLimitKind amountKind = AmountLimitKind::none;
    std::string_view amountLabel;
    int32_t amountMin = 0; // only used when amountKind == fixed
    int32_t amountMax = 0; // only used when amountKind == fixed
    int32_t amountDefault = 0; // initial value when placing object
    std::optional<int32_t> fixedY; // fixed top edge when placing/moving
    std::optional<std::pair<int32_t, int32_t>> defaultSize; // override default width/height when placing
    bool resizable = false; // can the bounding rect be resized?
    bool xOnlyResizable = false; // resize handles only on left/right edges
};

// iconIndex is the 0-based sequential index in enum declaration order,
// which corresponds to left-to-right, top-to-bottom position in palette_icons.png (8 columns, 32×32 each).
const std::map<ObjectType, PaletteItem> paletteItems = {
    {ObjectType::table,
        {.name = "Table", .iconIndex = 0, .defaultSize = {{128, 9}}, .resizable = true, .xOnlyResizable = true}},
    {ObjectType::shelf,
        {.name = "Shelf", .iconIndex = 1, .defaultSize = {{128, 7}}, .resizable = true, .xOnlyResizable = true}},
    {ObjectType::books, {.name = "Books", .iconIndex = 2}},
    {ObjectType::cabinet, {.name = "Cabinet", .iconIndex = 3, .defaultSize = {{100, 100}}, .resizable = true}},
    {ObjectType::exitRect,
        {.name = "Exit Rect",
            .iconIndex = 4,
            .amountKind = AmountLimitKind::fixed,
            .amountLabel = "Room",
            .amountMin = 1,
            .amountMax = 41,
            .amountDefault = 1,
            .defaultSize = {{64, 64}},
            .resizable = true}},
    {ObjectType::obstacleRect, {.name = "Obstacle Rect", .iconIndex = 5, .defaultSize = {{64, 64}}, .resizable = true}},
    {ObjectType::floorVent,
        {.name = "Floor Vent",
            .iconIndex = 6,
            .amountKind = AmountLimitKind::upward,
            .amountLabel = "Air top",
            .amountDefault = constants::airMaxVert,
            .fixedY = constants::floorVert}},
    {ObjectType::ceilingVent,
        {.name = "Ceiling Vent",
            .iconIndex = 7,
            .amountKind = AmountLimitKind::downward,
            .amountLabel = "Air bottom",
            .amountDefault = constants::floorVert,
            .fixedY = constants::ceilingVert}},
    {ObjectType::ceilingDuct,
        {.name = "Ceiling Duct",
            .iconIndex = 8,
            .amountKind = AmountLimitKind::downward,
            .amountLabel = "Air bottom",
            .amountDefault = constants::floorVert,
            .fixedY = constants::ceilingVert}},
    {ObjectType::candle,
        {.name = "Candle",
            .iconIndex = 9,
            .amountKind = AmountLimitKind::upward,
            .amountLabel = "Air top",
            .amountDefault = constants::airMaxVert}},
    {ObjectType::leftFan,
        {.name = "Left Fan",
            .iconIndex = 10,
            .amountKind = AmountLimitKind::leftward,
            .amountLabel = "Air left",
            .amountDefault = 0}},
    {ObjectType::rightFan,
        {.name = "Right Fan",
            .iconIndex = 11,
            .amountKind = AmountLimitKind::rightward,
            .amountLabel = "Air right",
            .amountDefault = 512}},
    {ObjectType::clock,
        {.name = "Clock",
            .iconIndex = 12,
            .amountKind = AmountLimitKind::fixed,
            .amountLabel = "Amount",
            .amountMax = 32767,
            .amountDefault = 100}},
    {ObjectType::paper,
        {.name = "Paper",
            .iconIndex = 13,
            .amountKind = AmountLimitKind::fixed,
            .amountLabel = "Amount",
            .amountMax = 32767,
            .amountDefault = 100}},
    {ObjectType::grease,
        {.name = "Grease",
            .iconIndex = 14,
            .amountKind = AmountLimitKind::rightward,
            .amountLabel = "Travel right",
            .amountDefault = 512}},
    {ObjectType::bonusRect,
        {.name = "Bonus Rect",
            .iconIndex = 15,
            .amountKind = AmountLimitKind::fixed,
            .amountLabel = "Amount",
            .amountMax = 32767,
            .amountDefault = 100,
            .defaultSize = {{32, 64}},
            .resizable = true}},
    {ObjectType::battery,
        {.name = "Battery",
            .iconIndex = 16,
            .amountKind = AmountLimitKind::fixed,
            .amountLabel = "Amount",
            .amountMax = 32767,
            .amountDefault = 10}},
    {ObjectType::rubberBand,
        {.name = "Rubber Band",
            .iconIndex = 17,
            .amountKind = AmountLimitKind::fixed,
            .amountLabel = "Amount",
            .amountMax = 32767,
            .amountDefault = 10}},
    {ObjectType::lightSwitch, {.name = "Light Switch", .iconIndex = 18}},
    {ObjectType::outlet,
        {.name = "Outlet",
            .iconIndex = 19,
            .amountKind = AmountLimitKind::fixed,
            .amountLabel = "Delay",
            .amountMax = 32767,
            .amountDefault = 0}},
    {ObjectType::thermostat, {.name = "Thermostat", .iconIndex = 20}},
    {ObjectType::shredder, {.name = "Shredder", .iconIndex = 21}},
    {ObjectType::powerSwitch,
        {.name = "Power Switch",
            .iconIndex = 22,
            .amountKind = AmountLimitKind::fixed,
            .amountLabel = "Object",
            .amountMin = 1,
            .amountMax = 16,
            .amountDefault = 1}},
    {ObjectType::guitar, {.name = "Guitar", .iconIndex = 23}},
    {ObjectType::drip,
        {.name = "Drip",
            .iconIndex = 24,
            .amountKind = AmountLimitKind::downward,
            .amountLabel = "Drip bottom",
            .amountDefault = constants::floorVert}},
    {ObjectType::toaster,
        {.name = "Toaster",
            .iconIndex = 25,
            .amountKind = AmountLimitKind::upward,
            .amountLabel = "Travel top",
            .amountDefault = constants::airMaxVert}},
    {ObjectType::ball,
        {.name = "Ball",
            .iconIndex = 26,
            .amountKind = AmountLimitKind::upward,
            .amountLabel = "Travel top",
            .amountDefault = constants::airMaxVert}},
    {ObjectType::fishBowl,
        {.name = "Fish Bowl",
            .iconIndex = 27,
            .amountKind = AmountLimitKind::upward,
            .amountLabel = "Travel top",
            .amountDefault = constants::airMaxVert}},
    {ObjectType::teaKettle,
        {.name = "Tea Kettle",
            .iconIndex = 28,
            .amountKind = AmountLimitKind::fixed,
            .amountLabel = "Delay",
            .amountMax = 32767,
            .amountDefault = 0}},
    {ObjectType::window, {.name = "Window", .iconIndex = 29, .defaultSize = {{100, 100}}, .resizable = true}},
    {ObjectType::painting, {.name = "Painting", .iconIndex = 30}},
    {ObjectType::mirror, {.name = "Mirror", .iconIndex = 31, .defaultSize = {{100, 100}}, .resizable = true}},
    {ObjectType::basket, {.name = "Basket", .iconIndex = 32}},
    {ObjectType::macintosh, {.name = "Macintosh", .iconIndex = 33}},
    {ObjectType::upStairs,
        {.name = "Up Stairs",
            .iconIndex = 34,
            .amountKind = AmountLimitKind::fixed,
            .amountLabel = "Room",
            .amountMin = 1,
            .amountMax = 41,
            .amountDefault = 1,
            .fixedY = constants::stairVert}},
    {ObjectType::downStairs,
        {.name = "Down Stairs",
            .iconIndex = 35,
            .amountKind = AmountLimitKind::fixed,
            .amountLabel = "Room",
            .amountMin = 1,
            .amountMax = 41,
            .amountDefault = 1,
            .fixedY = constants::stairVert}},
};

constexpr float leftPanelWidth = 300.0f;
constexpr float rightPanelWidth = 300.0f;
constexpr int32_t roomWidth = 512;
constexpr int32_t roomHeight = 342;

std::pair<int32_t, int32_t> resolveAmountLimits(const PaletteItem& palette, const ObjectData& object)
{
    switch (palette.amountKind)
    {
    case AmountLimitKind::fixed:
        return {palette.amountMin, palette.amountMax};
    case AmountLimitKind::upward:
        return {static_cast<int32_t>(constants::airMaxVert), static_cast<int32_t>(object.boundRect.top)};
    case AmountLimitKind::downward:
        return {static_cast<int32_t>(object.boundRect.bottom), static_cast<int32_t>(constants::floorVert)};
    case AmountLimitKind::leftward:
        return {0, static_cast<int32_t>(object.boundRect.left)};
    case AmountLimitKind::rightward:
        return {static_cast<int32_t>(object.boundRect.right), roomWidth};
    default:
        return {0, 0};
    }
}

// A line from (x1, y1) on the object to (x2, y2) at the amount handle position.
struct AmountLine
{
    int32_t x1 = 0;
    int32_t y1 = 0;
    int32_t x2 = 0;
    int32_t y2 = 0;
};

std::optional<AmountLine> getAmountLine(const ObjectData& object)
{
    const auto type = static_cast<ObjectType>(object.objectIs);
    if (object.objectIs == 0)
    {
        return std::nullopt;
    }
    const int32_t centerX = (static_cast<int32_t>(object.boundRect.left) + object.boundRect.right) / 2;
    const int32_t centerY = (static_cast<int32_t>(object.boundRect.top) + object.boundRect.bottom) / 2;
    const auto amount = static_cast<int32_t>(object.amount);

    switch (type)
    {
    case ObjectType::floorVent:
    case ObjectType::candle:
    case ObjectType::toaster:
    case ObjectType::ball:
    case ObjectType::fishBowl:
        return AmountLine {centerX, object.boundRect.top, centerX, amount};
    case ObjectType::ceilingVent:
    case ObjectType::ceilingDuct:
    case ObjectType::drip:
        return AmountLine {centerX, object.boundRect.bottom, centerX, amount};
    case ObjectType::leftFan:
        return AmountLine {object.boundRect.left, centerY, amount, centerY};
    case ObjectType::rightFan:
    case ObjectType::grease:
        return AmountLine {object.boundRect.right, centerY, amount, centerY};
    default:
        return std::nullopt;
    }
}

const char* objectTypeName(const ObjectType type)
{
    const auto it = paletteItems.find(type);
    return it != paletteItems.end() ? it->second.name.data() : "Unknown";
}

} // namespace

Editor::Editor(SDL_Window* window,
    SDL_Renderer* renderer,
    const std::string_view imagesDir,
    const std::string_view gameExecutablePath)
    : window_(window),
      sdlRenderer_(renderer),
      resources_(imagesDir, renderer),
      previewRenderer_(renderer, resources_, SDL_Rect {0, 0, roomWidth, roomHeight}),
      imagesDir_(imagesDir),
      gameExecutablePath_(gameExecutablePath)
{
    previewTexture_
        = SDL_CreateTexture(sdlRenderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, roomWidth, roomHeight);
    paletteIconsTexture_ = IMG_LoadTexture(sdlRenderer_, "images/palette_icons.png");
    newHouse();
}

Editor::~Editor()
{
    if (previewTexture_)
    {
        SDL_DestroyTexture(previewTexture_);
    }
    if (paletteIconsTexture_)
    {
        SDL_DestroyTexture(paletteIconsTexture_);
    }
}

void Editor::run()
{
    while (running_)
    {
        processEvents();

        {
            std::string title = "Gliderport Editor";
            if (!housePath_.empty())
            {
                title += " - ";
                title += housePath_;
            }
            if (dirty_)
            {
                title += " *";
            }
            SDL_SetWindowTitle(window_, title.c_str());
        }

        if (houseLoaded_)
        {
            renderPreview();
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            saveHouse();
        }

        drawUI();

        ImGui::Render();

        SDL_SetRenderDrawColor(sdlRenderer_, 40, 40, 40, 255);
        SDL_RenderClear(sdlRenderer_);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), sdlRenderer_);
        SDL_RenderPresent(sdlRenderer_);
    }
}

void Editor::processEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT)
        {
            if (dirty_)
            {
                quitRequested_ = true;
            }
            else
            {
                running_ = false;
            }
        }
        else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
        {
            if (activePaletteTool_ != ObjectType::nullObject)
            {
                activePaletteTool_ = ObjectType::nullObject;
            }
            else
            {
                selectedObjectIndex_ = -1;
            }
        }
    }
}

void Editor::renderPreview()
{
    SDL_SetRenderTarget(sdlRenderer_, previewTexture_);
    SDL_RenderSetClipRect(sdlRenderer_, nullptr);
    SDL_RenderSetViewport(sdlRenderer_, nullptr);
    SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sdlRenderer_, 0, 0, 0, 255);
    SDL_RenderClear(sdlRenderer_);

    previewRenderer_.drawRoom(staticGameState_, staticHouseProgress_, currentRoom());
    previewRenderer_.drawObjects(staticGameState_, staticHouseProgress_, currentRoom(), staticPlayerState_, 1.0f);

    drawInvisibleObjectOverlays();
    drawAmountOverlays();
    drawSelectionOverlay();

    SDL_SetRenderTarget(sdlRenderer_, nullptr);
}

void Editor::drawInvisibleObjectOverlays()
{
    if (!houseLoaded_)
    {
        return;
    }
    const auto& room = currentRoom();
    SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);

    for (auto object : room.theObjects)
    {
        const auto type = static_cast<ObjectType>(object.objectIs);
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        bool draw = true;

        switch (type)
        {
        case ObjectType::exitRect:
            r = 0;
            g = 200;
            b = 0;
            break;
        case ObjectType::obstacleRect:
            r = 200;
            g = 0;
            b = 0;
            break;
        case ObjectType::bonusRect:
            r = 200;
            g = 200;
            b = 0;
            break;
        default:
            draw = false;
            break;
        }

        if (!draw)
        {
            continue;
        }

        const SDL_Rect rect = object.boundRect.toSDLRect();
        SDL_SetRenderDrawColor(sdlRenderer_, r, g, b, 70);
        SDL_RenderFillRect(sdlRenderer_, &rect);
        SDL_SetRenderDrawColor(sdlRenderer_, r, g, b, 180);
        SDL_RenderDrawRect(sdlRenderer_, &rect);
    }
}

void Editor::drawAmountOverlays()
{
    if (!houseLoaded_)
    {
        return;
    }
    SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);

    const auto& room = currentRoom();
    for (size_t i = 0; i < RoomData::numObjects; ++i)
    {
        const auto& object = room.theObjects[i];
        const auto line = getAmountLine(object);
        if (!line.has_value())
        {
            continue;
        }

        const bool isSelected = (static_cast<int32_t>(i) == selectedObjectIndex_);

        SDL_SetRenderDrawColor(sdlRenderer_, 0, 200, 255, isSelected ? 220 : 130);
        SDL_RenderDrawLine(sdlRenderer_, line->x1, line->y1, line->x2, line->y2);

        const SDL_Rect handle = {line->x2 - 2, line->y2 - 2, 5, 5};
        if (isSelected)
        {
            SDL_SetRenderDrawColor(sdlRenderer_, 0, 200, 255, 255);
            SDL_RenderFillRect(sdlRenderer_, &handle);
        }
        else
        {
            SDL_SetRenderDrawColor(sdlRenderer_, 0, 200, 255, 180);
            SDL_RenderDrawRect(sdlRenderer_, &handle);
        }
    }
}

void Editor::drawSelectionOverlay()
{
    if (selectedObjectIndex_ < 0 || !houseLoaded_)
    {
        return;
    }
    const auto& object = currentRoom().theObjects[static_cast<size_t>(selectedObjectIndex_)];
    const SDL_Rect rect = object.boundRect.toSDLRect();

    SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sdlRenderer_, 255, 255, 0, 80);
    SDL_RenderFillRect(sdlRenderer_, &rect);
    SDL_SetRenderDrawColor(sdlRenderer_, 255, 255, 0, 220);
    SDL_RenderDrawRect(sdlRenderer_, &rect);

    const auto objectType = static_cast<ObjectType>(object.objectIs);
    if (paletteItems.at(objectType).resizable)
    {
        const SDL_Rect handles[4] = {
            {rect.x - 2, rect.y - 2, 5, 5},
            {rect.x + rect.w - 3, rect.y - 2, 5, 5},
            {rect.x - 2, rect.y + rect.h - 3, 5, 5},
            {rect.x + rect.w - 3, rect.y + rect.h - 3, 5, 5},
        };
        SDL_SetRenderDrawColor(sdlRenderer_, 255, 255, 0, 255);
        SDL_RenderFillRects(sdlRenderer_, handles, 4);
    }
}

void Editor::drawUI()
{
    int32_t windowWidth = 0;
    int32_t windowHeight = 0;
    SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
    const auto ww = static_cast<float>(windowWidth);
    const auto wh = static_cast<float>(windowHeight);

    drawMenuBar();

    const float menuBarHeight = ImGui::GetFrameHeight();

    drawRoomListPanel(menuBarHeight, wh);
    drawCenterPanel(menuBarHeight, wh, leftPanelWidth, rightPanelWidth);
    drawRightPanel(menuBarHeight, wh, rightPanelWidth);

    fileBrowser_.draw();

    if (quitRequested_)
    {
        quitRequested_ = false;
        confirmUnsaved(PendingAction::quit);
    }

    drawUnsavedChangesDialog();

    if (showAboutDialog_)
    {
        ImGui::OpenPopup("About Glider Editor");
        showAboutDialog_ = false;
    }
    if (ImGui::BeginPopupModal("About Glider Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Glider Editor");
        ImGui::Spacing();
        ImGui::TextUnformatted("House editor for the Glider reimplementation.");
        ImGui::TextUnformatted("Glider 4 originally designed by John Calhoun (softdorothy).");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextUnformatted("Copyright (C) 2026 Marcus Spangenberg");
        ImGui::TextUnformatted("Licensed under the GNU General Public License v3.");
        ImGui::Spacing();
        ImGui::TextUnformatted("Original game design, artwork, sounds, and house data:");
        ImGui::TextUnformatted("Copyright (C) 2016 softdorothy (John Calhoun)");
        ImGui::TextUnformatted("Licensed under the MIT License.");
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (!statusMessage_.empty())
    {
        ImGui::SetNextWindowPos(ImVec2(0.0f, wh - 22.0f));
        ImGui::SetNextWindowSize(ImVec2(ww, 22.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 3.0f));
        constexpr ImGuiWindowFlags statusFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs;
        ImGui::Begin("##statusbar", nullptr, statusFlags);
        ImGui::TextUnformatted(statusMessage_.c_str());
        ImGui::End();
        ImGui::PopStyleVar();
    }
}

void Editor::drawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New House"))
            {
                confirmUnsaved(PendingAction::newHouse);
            }
            if (ImGui::MenuItem("Open House...", "Ctrl+O"))
            {
                confirmUnsaved(PendingAction::openHouse);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S", false, houseLoaded_))
            {
                saveHouse();
            }
            if (ImGui::MenuItem("Save As...", nullptr, false, houseLoaded_))
            {
                fileBrowser_.openForSave("Save House As", housePath_, [this](const std::string& path) {
                    saveHouseAs(path);
                });
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Test House", nullptr, false, houseLoaded_))
            {
                testHouse(std::nullopt);
            }
            if (ImGui::MenuItem("Test House from This Room", nullptr, false, houseLoaded_))
            {
                testHouse(selectedRoomIndex_);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit"))
            {
                confirmUnsaved(PendingAction::quit);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About..."))
            {
                showAboutDialog_ = true;
            }
            ImGui::EndMenu();
        }

        if (houseLoaded_ && ImGui::BeginMenu("House"))
        {
            ImGui::Text("Version: %u", house_.version);
            ImGui::Text("Rooms: %u / 40", house_.numberORooms);
            ImGui::Separator();
            ImGui::Text("Next file:");
            ImGui::SetNextItemWidth(200.0f);
            char nextFileBuf[35] = {};
            strncpy(nextFileBuf, house_.nextFile.c_str(), 34);
            if (ImGui::InputText("##nextfile", nextFileBuf, sizeof(nextFileBuf)))
            {
                house_.nextFile = nextFileBuf;
                dirty_ = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void Editor::drawRoomListPanel(const float menuBarHeight, const float windowHeight)
{
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

    const float statusBarHeight = statusMessage_.empty() ? 0.0f : 22.0f;
    ImGui::SetNextWindowPos(ImVec2(0.0f, menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, windowHeight - menuBarHeight - statusBarHeight));
    ImGui::Begin("Rooms", nullptr, flags);

    if (houseLoaded_)
    {
        ImGui::Text("Rooms (%u / 40):", house_.numberORooms);
        ImGui::BeginChild("##roomlist", ImVec2(0, 150), true);
        for (uint16_t i = 0; i < house_.numberORooms; ++i)
        {
            const auto& room = house_.theRooms[i];
            char label[64];
            snprintf(label, sizeof(label), "[%u] %s", i, room.roomName.empty() ? "(unnamed)" : room.roomName.c_str());
            const bool selected = (selectedRoomIndex_ == static_cast<size_t>(i));
            if (ImGui::Selectable(label, selected))
            {
                selectedRoomIndex_ = i;
                selectedObjectIndex_ = -1;
                activePaletteTool_ = ObjectType::nullObject;
                updateStaticGameState();
            }
        }
        ImGui::EndChild();

        if (ImGui::Button("+ Add Room") && house_.numberORooms < 40)
        {
            addRoom();
        }
        ImGui::SameLine();
        if (ImGui::Button("- Remove") && house_.numberORooms > 0)
        {
            removeRoom(selectedRoomIndex_);
        }

        ImGui::Separator();

        if (house_.numberORooms > 0)
        {
            auto& room = currentRoom();
            ImGui::Text("Room Properties:");

            char nameBuf[26] = {};
            strncpy(nameBuf, room.roomName.c_str(), 25);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##roomname", nameBuf, sizeof(nameBuf)))
            {
                room.roomName = nameBuf;
                dirty_ = true;
            }

            {
                auto backID = static_cast<int32_t>(room.backPictID);
                if (ImGui::InputInt("Background", &backID, 1, 10))
                {
                    room.backPictID = static_cast<uint16_t>(std::clamp(backID, 200, 209));
                    dirty_ = true;
                }
            }

            if (ImGui::TreeNode("Tile Order"))
            {
                for (int32_t t = 0; t < 8; ++t)
                {
                    char tileLabel[16];
                    snprintf(tileLabel, sizeof(tileLabel), "Tile %d", t);
                    auto tileValue = static_cast<int32_t>(room.tileOrder[t]);
                    if (ImGui::InputInt(tileLabel, &tileValue, 1, 10))
                    {
                        room.tileOrder[t] = static_cast<uint16_t>(std::clamp(tileValue, 0, 7));
                        dirty_ = true;
                    }
                }
                ImGui::TreePop();
            }

            {
                bool leftOpen = room.leftOpen != 0;
                bool rightOpen = room.rightOpen != 0;
                if (ImGui::Checkbox("Left Open", &leftOpen))
                {
                    room.leftOpen = leftOpen ? 1 : 0;
                    dirty_ = true;
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("Right Open", &rightOpen))
                {
                    room.rightOpen = rightOpen ? 1 : 0;
                    dirty_ = true;
                }
            }

            {
                const char* enemyKinds[] = {"Dart", "Copter", "Balloon"};
                auto kind = static_cast<int32_t>(room.animateKind);
                if (kind < 0 || kind > 2)
                {
                    kind = 0;
                }
                if (ImGui::Combo("Enemy", &kind, enemyKinds, 3))
                {
                    room.animateKind = static_cast<AnimateKind>(kind);
                    dirty_ = true;
                }
                auto enemyCount = static_cast<int32_t>(room.animateNumber);
                if (ImGui::InputInt("Count", &enemyCount, 1, 1))
                {
                    room.animateNumber = static_cast<uint16_t>(std::clamp(enemyCount, 0, 16));
                    dirty_ = true;
                }
                auto delay = static_cast<int32_t>(room.animateDelay);
                if (ImGui::InputInt("Delay", &delay, 1, 10))
                {
                    room.animateDelay = static_cast<uint32_t>(std::max(delay, 0));
                    dirty_ = true;
                }
            }

            {
                bool airOut = (room.conditionCode == ConditionCode::AIR_OUT);
                bool lightsOut = (room.conditionCode == ConditionCode::LIGHTS_OUT);
                if (ImGui::Checkbox("Air Out", &airOut))
                {
                    room.conditionCode = airOut ? ConditionCode::AIR_OUT : ConditionCode::NONE;
                    dirty_ = true;
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("Lights Out", &lightsOut))
                {
                    room.conditionCode = lightsOut ? ConditionCode::LIGHTS_OUT : ConditionCode::NONE;
                    dirty_ = true;
                }
            }
        }
    }
    else
    {
        ImGui::TextDisabled("No house loaded.");
        ImGui::Spacing();
        if (ImGui::Button("Open House..."))
        {
            fileBrowser_.openForOpen("Open House File", [this](const std::string& path) { openHouse(path); });
        }
        if (ImGui::Button("New House"))
        {
            newHouse();
        }
    }

    ImGui::End();
}

void Editor::drawCenterPanel(const float menuBarHeight,
    const float windowHeight,
    const float leftWidth,
    const float rightWidth)
{
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_HorizontalScrollbar;

    int32_t windowWidth = 0;
    SDL_GetWindowSize(window_, &windowWidth, nullptr);
    const float centerWidth = static_cast<float>(windowWidth) - leftWidth - rightWidth;
    const float statusBarHeight = statusMessage_.empty() ? 0.0f : 22.0f;

    ImGui::SetNextWindowPos(ImVec2(leftWidth, menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(centerWidth, windowHeight - menuBarHeight - statusBarHeight));
    ImGui::Begin("Preview", nullptr, flags);

    if (!houseLoaded_)
    {
        ImGui::TextDisabled("No house loaded. Use File > Open House.");
        ImGui::End();
        return;
    }

    const float availWidth = centerWidth - 20.0f;
    const float availHeight = windowHeight - menuBarHeight - statusBarHeight - 40.0f;
    previewScale_ = std::min(availWidth / static_cast<float>(roomWidth), availHeight / static_cast<float>(roomHeight));
    previewScale_ = std::max(previewScale_, 0.5f);

    const float scaledW = static_cast<float>(roomWidth) * previewScale_;
    const float scaledH = static_cast<float>(roomHeight) * previewScale_;

    const float offsetX = std::max(0.0f, (availWidth - scaledW) * 0.5f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

    previewImageOrigin_ = ImGui::GetCursorScreenPos();
    ImGui::Image(reinterpret_cast<ImTextureID>(previewTexture_), ImVec2(scaledW, scaledH));

    if (activePaletteTool_ != ObjectType::nullObject && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Click to place: %s\nEsc to cancel", objectTypeName(activePaletteTool_));
    }

    if (ImGui::IsItemHovered())
    {
        const ImVec2 mousePos = ImGui::GetMousePos();
        const float roomX = (mousePos.x - previewImageOrigin_.x) / previewScale_;
        const float roomY = (mousePos.y - previewImageOrigin_.y) / previewScale_;

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            handlePreviewClick(roomX, roomY);
        }
        else if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            activePaletteTool_ = ObjectType::nullObject;
            selectedObjectIndex_ = -1;
        }
    }

    if (dragMode_ != DragMode::none)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const ImVec2 mousePos = ImGui::GetMousePos();
            const float roomX = (mousePos.x - previewImageOrigin_.x) / previewScale_;
            const float roomY = (mousePos.y - previewImageOrigin_.y) / previewScale_;
            handlePreviewDrag(roomX, roomY);
        }
        else
        {
            dragMode_ = DragMode::none;
        }
    }

    ImGui::End();
}

void Editor::drawRightPanel(const float menuBarHeight, const float windowHeight, const float rightWidth)
{
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

    int32_t windowWidth = 0;
    SDL_GetWindowSize(window_, &windowWidth, nullptr);
    const float statusBarHeight = statusMessage_.empty() ? 0.0f : 22.0f;

    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(windowWidth) - rightWidth, menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(rightWidth, windowHeight - menuBarHeight - statusBarHeight));
    ImGui::Begin("Objects", nullptr, flags);

    if (!houseLoaded_)
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Object Palette:");
    if (activePaletteTool_ != ObjectType::nullObject)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "(%s)", objectTypeName(activePaletteTool_));
    }

    int32_t iconsW = 1;
    int32_t iconsH = 1;
    if (paletteIconsTexture_)
    {
        SDL_QueryTexture(paletteIconsTexture_, nullptr, nullptr, &iconsW, &iconsH);
    }
    const auto iconsTexW = static_cast<float>(iconsW);
    const auto iconsTexH = static_cast<float>(iconsH);

    ImGui::BeginChild("##palette", ImVec2(0, 300), true, ImGuiWindowFlags_None);

    for (const auto& [type, item] : paletteItems)
    {
        ImGui::PushID(static_cast<int32_t>(type));

        const bool isActive = (activePaletteTool_ == type);
        if (isActive)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.1f, 1.0f));
        }

        bool clicked = false;

        if (paletteIconsTexture_)
        {
            const auto iconIdx = static_cast<int32_t>(item.iconIndex);
            const int32_t col = iconIdx % 8;
            const int32_t row = iconIdx / 8;
            const ImVec2 uv0
                = {static_cast<float>(col) * 32.0f / iconsTexW, static_cast<float>(row) * 32.0f / iconsTexH};
            const ImVec2 uv1
                = {static_cast<float>(col + 1) * 32.0f / iconsTexW, static_cast<float>(row + 1) * 32.0f / iconsTexH};
            clicked = ImGui::ImageButton("##btn",
                reinterpret_cast<ImTextureID>(paletteIconsTexture_),
                ImVec2(32.0f, 32.0f),
                uv0,
                uv1);
        }
        else
        {
            clicked = ImGui::Button("##btn", ImVec2(34.0f, 34.0f));
        }

        if (clicked)
        {
            activePaletteTool_ = (isActive ? ObjectType::nullObject : type);
        }

        if (isActive)
        {
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
        if (ImGui::Selectable(item.name.data(), isActive, 0, ImVec2(0, 34)))
        {
            activePaletteTool_ = (isActive ? ObjectType::nullObject : type);
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::Separator();

    if (selectedObjectIndex_ < 0)
    {
        ImGui::TextDisabled("No object selected.");
        ImGui::TextDisabled("Click room to select/place.");
        ImGui::End();
        return;
    }

    auto& object = currentRoom().theObjects[static_cast<size_t>(selectedObjectIndex_)];
    const auto type = static_cast<ObjectType>(object.objectIs);

    ImGui::Text("Object #%d: %s", selectedObjectIndex_, objectTypeName(type));
    ImGui::Spacing();

    auto editUInt16 = [&](const char* label, uint16_t& field, int32_t minVal, int32_t maxVal) -> bool {
        auto value = static_cast<int32_t>(field);
        if (ImGui::InputInt(label, &value, 1, 10))
        {
            field = static_cast<uint16_t>(std::clamp(value, minVal, maxVal));
            dirty_ = true;
            return true;
        }
        return false;
    };

    ImGui::Text("Bound Rect:");
    switch (type)
    {
    case ObjectType::shelf:
        if (editUInt16("Top", object.boundRect.top, 0, roomHeight - 7))
        {
            object.boundRect.bottom = static_cast<uint16_t>(object.boundRect.top + 7);
        }
        editUInt16("Left", object.boundRect.left, 0, roomWidth);
        ImGui::LabelText("Bottom", "%d (fixed height)", static_cast<int32_t>(object.boundRect.bottom));
        editUInt16("Right", object.boundRect.right, 0, roomWidth);
        break;

    case ObjectType::table:
        if (editUInt16("Top", object.boundRect.top, 0, roomHeight - 9))
        {
            object.boundRect.bottom = static_cast<uint16_t>(object.boundRect.top + 9);
        }
        editUInt16("Left", object.boundRect.left, 0, roomWidth);
        ImGui::LabelText("Bottom", "%d (fixed height)", static_cast<int32_t>(object.boundRect.bottom));
        editUInt16("Right", object.boundRect.right, 0, roomWidth);
        break;

    case ObjectType::window:
        editUInt16("Top", object.boundRect.top, 0, roomHeight);
        editUInt16("Left", object.boundRect.left, 0, roomWidth);
        editUInt16("Bottom", object.boundRect.bottom, 0, roomHeight);
        editUInt16("Right", object.boundRect.right, 0, roomWidth);
        ImGui::Spacing();
        {
            bool isOn = object.isOn != 0;
            if (ImGui::Checkbox("Is open", &isOn))
            {
                object.isOn = isOn ? 1 : 0;
                dirty_ = true;
            }
        }
        break;

    case ObjectType::cabinet:
    case ObjectType::mirror:
    case ObjectType::nullObject:
        editUInt16("Top", object.boundRect.top, 0, roomHeight);
        editUInt16("Left", object.boundRect.left, 0, roomWidth);
        editUInt16("Bottom", object.boundRect.bottom, 0, roomHeight);
        editUInt16("Right", object.boundRect.right, 0, roomWidth);
        break;

    case ObjectType::exitRect:
        editUInt16("Top", object.boundRect.top, 0, roomHeight);
        editUInt16("Left", object.boundRect.left, 0, roomWidth);
        editUInt16("Bottom", object.boundRect.bottom, 0, roomHeight);
        editUInt16("Right", object.boundRect.right, 0, roomWidth);
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
        break;

    case ObjectType::obstacleRect:
        editUInt16("Top", object.boundRect.top, 0, roomHeight);
        editUInt16("Left", object.boundRect.left, 0, roomWidth);
        editUInt16("Bottom", object.boundRect.bottom, 0, roomHeight);
        editUInt16("Right", object.boundRect.right, 0, roomWidth);
        break;

    case ObjectType::bonusRect:
        editUInt16("Top", object.boundRect.top, 0, roomHeight);
        editUInt16("Left", object.boundRect.left, 0, roomWidth);
        editUInt16("Bottom", object.boundRect.bottom, 0, roomHeight);
        editUInt16("Right", object.boundRect.right, 0, roomWidth);
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
        break;

    case ObjectType::upStairs:
    case ObjectType::downStairs:
        ImGui::LabelText("Top", "%d (fixed)", constants::stairVert);
        editUInt16("Left", object.boundRect.left, 0, roomWidth);
        ImGui::LabelText("Bottom", "%d (fixed)", object.boundRect.bottom);
        editUInt16("Right", object.boundRect.right, 0, roomWidth);
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
        break;

    case ObjectType::floorVent:
        ImGui::LabelText("Top", "%d (fixed)", constants::floorVert);
        editUInt16("Left", object.boundRect.left, 0, roomWidth);
        ImGui::LabelText("Bottom", "%d (fixed)", object.boundRect.bottom);
        editUInt16("Right", object.boundRect.right, 0, roomWidth);
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
        break;

    case ObjectType::ceilingVent:
        ImGui::LabelText("Top", "%d (fixed)", constants::ceilingVert);
        editUInt16("Left", object.boundRect.left, 0, roomWidth);
        ImGui::LabelText("Bottom", "%d (fixed)", object.boundRect.bottom);
        editUInt16("Right", object.boundRect.right, 0, roomWidth);
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
        break;

    case ObjectType::ceilingDuct:
        ImGui::LabelText("Top", "%d (fixed)", constants::ceilingVert);
        editUInt16("Left", object.boundRect.left, 0, roomWidth);
        ImGui::LabelText("Bottom", "%d (fixed)", object.boundRect.bottom);
        editUInt16("Right", object.boundRect.right, 0, roomWidth);
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
        editUInt16("Room", object.extra, 1, 41);
        {
            bool isOn = object.isOn != 0;
            if (ImGui::Checkbox("Blowing", &isOn))
            {
                object.isOn = isOn ? 1 : 0;
                dirty_ = true;
            }
        }
        break;

    case ObjectType::candle:
    {
        auto top = object.boundRect.top;
        if (editUInt16("Top", top, 0, roomHeight))
        {
            const auto height = object.boundRect.bottom - object.boundRect.top;
            object.boundRect.top = top;
            object.boundRect.bottom = static_cast<uint16_t>(object.boundRect.top + height);
        }
        auto left = object.boundRect.left;
        if (editUInt16("Left", left, 0, roomWidth))
        {
            const auto width = object.boundRect.right - object.boundRect.left;
            object.boundRect.left = left;
            object.boundRect.right = static_cast<uint16_t>(object.boundRect.left + width);
        }
        ImGui::LabelText("Bottom", "%d (fixed height)", static_cast<int32_t>(object.boundRect.bottom));
        ImGui::LabelText("Right", "%d (fixed width)", static_cast<int32_t>(object.boundRect.right));
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
    }
    break;

    case ObjectType::leftFan:
    {
        auto top = object.boundRect.top;
        if (editUInt16("Top", top, 0, roomHeight))
        {
            const auto height = object.boundRect.bottom - object.boundRect.top;
            object.boundRect.top = top;
            object.boundRect.bottom = static_cast<uint16_t>(object.boundRect.top + height);
        }
        auto left = object.boundRect.left;
        if (editUInt16("Left", left, 0, roomWidth))
        {
            const auto width = object.boundRect.right - object.boundRect.left;
            object.boundRect.left = left;
            object.boundRect.right = static_cast<uint16_t>(object.boundRect.left + width);
        }
        ImGui::LabelText("Bottom", "%d (fixed height)", static_cast<int32_t>(object.boundRect.bottom));
        ImGui::LabelText("Right", "%d (fixed width)", static_cast<int32_t>(object.boundRect.right));
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
        {
            bool isOn = object.isOn != 0;
            if (ImGui::Checkbox("Is On", &isOn))
            {
                object.isOn = isOn ? 1 : 0;
                dirty_ = true;
            }
        }
    }
    break;

    case ObjectType::rightFan:
    {
        auto top = object.boundRect.top;
        if (editUInt16("Top", top, 0, roomHeight))
        {
            const auto height = object.boundRect.bottom - object.boundRect.top;
            object.boundRect.top = top;
            object.boundRect.bottom = static_cast<uint16_t>(object.boundRect.top + height);
        }
        auto left = object.boundRect.left;
        if (editUInt16("Left", left, 0, roomWidth))
        {
            const auto width = object.boundRect.right - object.boundRect.left;
            object.boundRect.left = left;
            object.boundRect.right = static_cast<uint16_t>(object.boundRect.left + width);
        }
        ImGui::LabelText("Bottom", "%d (fixed height)", static_cast<int32_t>(object.boundRect.bottom));
        ImGui::LabelText("Right", "%d (fixed width)", static_cast<int32_t>(object.boundRect.right));
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
        {
            bool isOn = object.isOn != 0;
            if (ImGui::Checkbox("Is On", &isOn))
            {
                object.isOn = isOn ? 1 : 0;
                dirty_ = true;
            }
        }
    }
    break;

    case ObjectType::grease:
    {
        auto top = object.boundRect.top;
        if (editUInt16("Top", top, 0, roomHeight))
        {
            const auto height = object.boundRect.bottom - object.boundRect.top;
            object.boundRect.top = top;
            object.boundRect.bottom = static_cast<uint16_t>(object.boundRect.top + height);
        }
        auto left = object.boundRect.left;
        if (editUInt16("Left", left, 0, roomWidth))
        {
            const auto width = object.boundRect.right - object.boundRect.left;
            object.boundRect.left = left;
            object.boundRect.right = static_cast<uint16_t>(object.boundRect.left + width);
        }
        ImGui::LabelText("Bottom", "%d (fixed height)", static_cast<int32_t>(object.boundRect.bottom));
        ImGui::LabelText("Right", "%d (fixed width)", static_cast<int32_t>(object.boundRect.right));
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
        {
            bool spilled = object.isOn == 0;
            if (ImGui::Checkbox("Spilled", &spilled))
            {
                object.isOn = spilled ? 0 : 1;
                dirty_ = true;
            }
        }
    }
    break;

    case ObjectType::clock:
    case ObjectType::paper:
    case ObjectType::battery:
    case ObjectType::rubberBand:
    {
        auto top = object.boundRect.top;
        if (editUInt16("Top", top, 0, roomHeight))
        {
            const auto height = object.boundRect.bottom - object.boundRect.top;
            object.boundRect.top = top;
            object.boundRect.bottom = static_cast<uint16_t>(object.boundRect.top + height);
        }
        auto left = object.boundRect.left;
        if (editUInt16("Left", left, 0, roomWidth))
        {
            const auto width = object.boundRect.right - object.boundRect.left;
            object.boundRect.left = left;
            object.boundRect.right = static_cast<uint16_t>(object.boundRect.left + width);
        }
        ImGui::LabelText("Bottom", "%d (fixed height)", static_cast<int32_t>(object.boundRect.bottom));
        ImGui::LabelText("Right", "%d (fixed width)", static_cast<int32_t>(object.boundRect.right));
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
    }
    break;

    case ObjectType::powerSwitch:
    {
        auto top = object.boundRect.top;
        if (editUInt16("Top", top, 0, roomHeight))
        {
            const auto height = object.boundRect.bottom - object.boundRect.top;
            object.boundRect.top = top;
            object.boundRect.bottom = static_cast<uint16_t>(object.boundRect.top + height);
        }
        auto left = object.boundRect.left;
        if (editUInt16("Left", left, 0, roomWidth))
        {
            const auto width = object.boundRect.right - object.boundRect.left;
            object.boundRect.left = left;
            object.boundRect.right = static_cast<uint16_t>(object.boundRect.left + width);
        }
        ImGui::LabelText("Bottom", "%d (fixed height)", static_cast<int32_t>(object.boundRect.bottom));
        ImGui::LabelText("Right", "%d (fixed width)", static_cast<int32_t>(object.boundRect.right));
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
    }
    break;

    case ObjectType::outlet:
    case ObjectType::teaKettle:
    {
        auto top = object.boundRect.top;
        if (editUInt16("Top", top, 0, roomHeight))
        {
            const auto height = object.boundRect.bottom - object.boundRect.top;
            object.boundRect.top = top;
            object.boundRect.bottom = static_cast<uint16_t>(object.boundRect.top + height);
        }
        auto left = object.boundRect.left;
        if (editUInt16("Left", left, 0, roomWidth))
        {
            const auto width = object.boundRect.right - object.boundRect.left;
            object.boundRect.left = left;
            object.boundRect.right = static_cast<uint16_t>(object.boundRect.left + width);
        }
        ImGui::LabelText("Bottom", "%d (fixed height)", static_cast<int32_t>(object.boundRect.bottom));
        ImGui::LabelText("Right", "%d (fixed width)", static_cast<int32_t>(object.boundRect.right));
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
    }
    break;

    case ObjectType::shredder:
    {
        auto top = object.boundRect.top;
        if (editUInt16("Top", top, 0, roomHeight))
        {
            const auto height = object.boundRect.bottom - object.boundRect.top;
            object.boundRect.top = top;
            object.boundRect.bottom = static_cast<uint16_t>(object.boundRect.top + height);
        }
        auto left = object.boundRect.left;
        if (editUInt16("Left", left, 0, roomWidth))
        {
            const auto width = object.boundRect.right - object.boundRect.left;
            object.boundRect.left = left;
            object.boundRect.right = static_cast<uint16_t>(object.boundRect.left + width);
        }
        ImGui::LabelText("Bottom", "%d (fixed height)", static_cast<int32_t>(object.boundRect.bottom));
        ImGui::LabelText("Right", "%d (fixed width)", static_cast<int32_t>(object.boundRect.right));
        ImGui::Spacing();

        {
            bool isOn = object.isOn != 0;
            if (ImGui::Checkbox("Is On", &isOn))
            {
                object.isOn = isOn ? 1 : 0;
                dirty_ = true;
            }
        }
    }
    break;

    case ObjectType::drip:
    {
        auto top = object.boundRect.top;
        if (editUInt16("Top", top, 0, roomHeight))
        {
            const auto height = object.boundRect.bottom - object.boundRect.top;
            object.boundRect.top = top;
            object.boundRect.bottom = static_cast<uint16_t>(object.boundRect.top + height);
        }
        auto left = object.boundRect.left;
        if (editUInt16("Left", left, 0, roomWidth))
        {
            const auto width = object.boundRect.right - object.boundRect.left;
            object.boundRect.left = left;
            object.boundRect.right = static_cast<uint16_t>(object.boundRect.left + width);
        }
        ImGui::LabelText("Bottom", "%d (fixed height)", static_cast<int32_t>(object.boundRect.bottom));
        ImGui::LabelText("Right", "%d (fixed width)", static_cast<int32_t>(object.boundRect.right));
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
        editUInt16("Delay", object.extra, 0, 32767);
    }
    break;

    case ObjectType::toaster:
    case ObjectType::ball:
    case ObjectType::fishBowl:
    {
        auto top = object.boundRect.top;
        if (editUInt16("Top", top, 0, roomHeight))
        {
            const auto height = object.boundRect.bottom - object.boundRect.top;
            object.boundRect.top = top;
            object.boundRect.bottom = static_cast<uint16_t>(object.boundRect.top + height);
        }
        auto left = object.boundRect.left;
        if (editUInt16("Left", left, 0, roomWidth))
        {
            const auto width = object.boundRect.right - object.boundRect.left;
            object.boundRect.left = left;
            object.boundRect.right = static_cast<uint16_t>(object.boundRect.left + width);
        }
        ImGui::LabelText("Bottom", "%d (fixed height)", static_cast<int32_t>(object.boundRect.bottom));
        ImGui::LabelText("Right", "%d (fixed width)", static_cast<int32_t>(object.boundRect.right));
        ImGui::Spacing();
        {
            const auto [minVal, maxVal] = resolveAmountLimits(paletteItems.at(type), object);
            editUInt16(paletteItems.at(type).amountLabel.data(), object.amount, minVal, maxVal);
        }
        editUInt16("Delay", object.extra, 0, 32767);
    }
    break;

    case ObjectType::lightSwitch:
    case ObjectType::thermostat:
    case ObjectType::books:
    case ObjectType::guitar:
    case ObjectType::painting:
    case ObjectType::basket:
    case ObjectType::macintosh:
    {
        auto top = object.boundRect.top;
        if (editUInt16("Top", top, 0, roomHeight))
        {
            const auto height = object.boundRect.bottom - object.boundRect.top;
            object.boundRect.top = top;
            object.boundRect.bottom = static_cast<uint16_t>(object.boundRect.top + height);
        }
        auto left = object.boundRect.left;
        if (editUInt16("Left", left, 0, roomWidth))
        {
            const auto width = object.boundRect.right - object.boundRect.left;
            object.boundRect.left = left;
            object.boundRect.right = static_cast<uint16_t>(object.boundRect.left + width);
        }
        ImGui::LabelText("Bottom", "%d (fixed height)", static_cast<int32_t>(object.boundRect.bottom));
        ImGui::LabelText("Right", "%d (fixed width)", static_cast<int32_t>(object.boundRect.right));
    }
    break;
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("Delete Object", ImVec2(-1.0f, 0.0f)))
    {
        currentRoom().theObjects[static_cast<size_t>(selectedObjectIndex_)] = ObjectData {};
        selectedObjectIndex_ = -1;
        dirty_ = true;
        updateStaticGameState();
    }
    ImGui::PopStyleColor();
    ImGui::End();
}

void Editor::handlePreviewClick(const float roomX, const float roomY)
{
    if (activePaletteTool_ != ObjectType::nullObject)
    {
        placeObject(roomX, roomY);
        return;
    }

    constexpr float handleRadius = 6.0f;

    if (selectedObjectIndex_ >= 0)
    {
        const auto& object = currentRoom().theObjects[static_cast<size_t>(selectedObjectIndex_)];
        const auto line = getAmountLine(object);
        if (line.has_value())
        {
            if (std::abs(roomX - static_cast<float>(line->x2)) <= handleRadius
                && std::abs(roomY - static_cast<float>(line->y2)) <= handleRadius)
            {
                dragMode_ = DragMode::resizeAmount;
                return;
            }
        }
    }

    if (selectedObjectIndex_ >= 0)
    {
        const auto& object = currentRoom().theObjects[static_cast<size_t>(selectedObjectIndex_)];
        const auto objectType = static_cast<ObjectType>(object.objectIs);
        if (paletteItems.at(objectType).resizable)
        {
            const auto left = static_cast<float>(object.boundRect.left);
            const auto top = static_cast<float>(object.boundRect.top);
            const auto right = static_cast<float>(object.boundRect.right);
            const auto bottom = static_cast<float>(object.boundRect.bottom);

            const bool nearLeft = std::abs(roomX - left) <= handleRadius;
            const bool nearRight = std::abs(roomX - right) <= handleRadius;
            const bool nearTop = std::abs(roomY - top) <= handleRadius;
            const bool nearBottom = std::abs(roomY - bottom) <= handleRadius;

            if ((nearLeft || nearRight) && (nearTop || nearBottom))
            {
                if (nearLeft && nearTop)
                {
                    dragMode_ = DragMode::resizeTopLeft;
                }
                else if (nearRight && nearTop)
                {
                    dragMode_ = DragMode::resizeTopRight;
                }
                else if (nearLeft && nearBottom)
                {
                    dragMode_ = DragMode::resizeBottomLeft;
                }
                else
                {
                    dragMode_ = DragMode::resizeBottomRight;
                }
                return;
            }
        }
    }

    selectObjectAt(roomX, roomY);
    if (selectedObjectIndex_ >= 0)
    {
        dragMode_ = DragMode::move;
        const auto& object = currentRoom().theObjects[static_cast<size_t>(selectedObjectIndex_)];
        dragOffsetX_ = roomX - static_cast<float>(object.boundRect.left);
        dragOffsetY_ = roomY - static_cast<float>(object.boundRect.top);
    }
}

void Editor::handlePreviewDrag(const float roomX, const float roomY)
{
    if (selectedObjectIndex_ < 0)
    {
        return;
    }
    auto& object = currentRoom().theObjects[static_cast<size_t>(selectedObjectIndex_)];
    const auto objectType = static_cast<ObjectType>(object.objectIs);

    if (dragMode_ == DragMode::move)
    {
        const int32_t width
            = static_cast<int32_t>(object.boundRect.right) - static_cast<int32_t>(object.boundRect.left);
        const int32_t height
            = static_cast<int32_t>(object.boundRect.bottom) - static_cast<int32_t>(object.boundRect.top);
        const auto newLeft = static_cast<int32_t>(roomX - dragOffsetX_);
        const int32_t clampedLeft = std::clamp(newLeft, 0, roomWidth - std::max(width, 1));
        object.boundRect.left = static_cast<uint16_t>(clampedLeft);
        object.boundRect.right = static_cast<uint16_t>(clampedLeft + width);
        const auto& fixTop = paletteItems.at(objectType).fixedY;
        if (!fixTop.has_value())
        {
            const auto newTop = static_cast<int32_t>(roomY - dragOffsetY_);
            const int32_t clampedTop = std::clamp(newTop, 0, roomHeight - std::max(height, 1));
            object.boundRect.top = static_cast<uint16_t>(clampedTop);
            object.boundRect.bottom = static_cast<uint16_t>(clampedTop + height);
        }
    }
    else
    {
        constexpr int32_t minSize = 4;
        const bool xOnly = paletteItems.at(objectType).xOnlyResizable;
        const auto x = static_cast<int32_t>(roomX);
        const auto y = static_cast<int32_t>(roomY);

        switch (dragMode_)
        {
        case DragMode::resizeTopLeft:
        {
            const int32_t newLeft = std::clamp(x, 0, static_cast<int32_t>(object.boundRect.right) - minSize);
            object.boundRect.left = static_cast<uint16_t>(newLeft);
            if (!xOnly)
            {
                const int32_t newTop = std::clamp(y, 0, static_cast<int32_t>(object.boundRect.bottom) - minSize);
                object.boundRect.top = static_cast<uint16_t>(newTop);
            }
            break;
        }
        case DragMode::resizeTopRight:
        {
            const int32_t newRight = std::clamp(x, static_cast<int32_t>(object.boundRect.left) + minSize, roomWidth);
            object.boundRect.right = static_cast<uint16_t>(newRight);
            if (!xOnly)
            {
                const int32_t newTop = std::clamp(y, 0, static_cast<int32_t>(object.boundRect.bottom) - minSize);
                object.boundRect.top = static_cast<uint16_t>(newTop);
            }
            break;
        }
        case DragMode::resizeBottomLeft:
        {
            const int32_t newLeft = std::clamp(x, 0, static_cast<int32_t>(object.boundRect.right) - minSize);
            object.boundRect.left = static_cast<uint16_t>(newLeft);
            if (!xOnly)
            {
                const int32_t newBottom
                    = std::clamp(y, static_cast<int32_t>(object.boundRect.top) + minSize, roomHeight);
                object.boundRect.bottom = static_cast<uint16_t>(newBottom);
            }
            break;
        }
        case DragMode::resizeBottomRight:
        {
            const int32_t newRight = std::clamp(x, static_cast<int32_t>(object.boundRect.left) + minSize, roomWidth);
            object.boundRect.right = static_cast<uint16_t>(newRight);
            if (!xOnly)
            {
                const int32_t newBottom
                    = std::clamp(y, static_cast<int32_t>(object.boundRect.top) + minSize, roomHeight);
                object.boundRect.bottom = static_cast<uint16_t>(newBottom);
            }
            break;
        }
        case DragMode::resizeAmount:
        {
            const auto& palette = paletteItems.at(objectType);
            const auto [minVal, maxVal] = resolveAmountLimits(palette, object);
            switch (palette.amountKind)
            {
            case AmountLimitKind::upward:
            case AmountLimitKind::downward:
                object.amount = static_cast<uint16_t>(std::clamp(y, minVal, maxVal));
                break;
            case AmountLimitKind::leftward:
            case AmountLimitKind::rightward:
                object.amount = static_cast<uint16_t>(std::clamp(x, minVal, maxVal));
                break;
            default:
                break;
            }
            break;
        }
        default:
            break;
        }
    }

    dirty_ = true;
    updateStaticGameState();
}

void Editor::placeObject(const float roomX, const float roomY)
{
    for (size_t i = 0; i < currentRoom().theObjects.size(); ++i)
    {
        auto& object = currentRoom().theObjects[i];
        if (object.objectIs != 0)
        {
            continue;
        }

        object.objectIs = static_cast<uint16_t>(activePaletteTool_);
        object.isOn = 1;

        const auto [width, height] = defaultObjectSize(activePaletteTool_);
        const int32_t clampedLeft = std::clamp(static_cast<int32_t>(roomX), 0, roomWidth - width);
        object.boundRect.left = static_cast<uint16_t>(clampedLeft);
        object.boundRect.right = static_cast<uint16_t>(clampedLeft + width);
        const auto& fixTop = paletteItems.at(activePaletteTool_).fixedY;
        if (fixTop.has_value())
        {
            object.boundRect.top = static_cast<uint16_t>(fixTop.value());
            object.boundRect.bottom = static_cast<uint16_t>(fixTop.value() + height);
        }
        else
        {
            const int32_t clampedTop = std::clamp(static_cast<int32_t>(roomY), 0, roomHeight - height);
            object.boundRect.top = static_cast<uint16_t>(clampedTop);
            object.boundRect.bottom = static_cast<uint16_t>(clampedTop + height);
        }

        const auto& palette = paletteItems.at(activePaletteTool_);
        if (palette.amountKind != AmountLimitKind::none)
        {
            object.amount = static_cast<uint16_t>(palette.amountDefault);
        }

        selectedObjectIndex_ = static_cast<int32_t>(i);
        activePaletteTool_ = ObjectType::nullObject;
        dirty_ = true;
        updateStaticGameState();
        return;
    }
    statusMessage_ = "Room is full (16 objects maximum).";
}

void Editor::selectObjectAt(const float roomX, const float roomY)
{
    selectedObjectIndex_ = -1;
    const auto x = static_cast<int32_t>(roomX);
    const auto y = static_cast<int32_t>(roomY);

    const auto& room = currentRoom();

    std::array<size_t, RoomData::numObjects> reverseDrawOrder {};
    std::iota(reverseDrawOrder.begin(), reverseDrawOrder.end(), 0);
    std::ranges::stable_sort(reverseDrawOrder, [&](const size_t a, const size_t b) {
        return Renderer::drawOrderPriority(room.theObjects[a]) >= Renderer::drawOrderPriority(room.theObjects[b]);
    });

    for (const auto i : reverseDrawOrder)
    {
        const auto& object = room.theObjects[static_cast<size_t>(i)];
        if (object.objectIs == 0)
        {
            continue;
        }
        const auto rect = object.boundRect.toSDLRect();
        if (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h)
        {
            selectedObjectIndex_ = static_cast<int32_t>(i);
            break;
        }
    }
}

RoomData& Editor::currentRoom()
{
    return house_.theRooms[selectedRoomIndex_];
}

const RoomData& Editor::currentRoom() const
{
    return house_.theRooms[selectedRoomIndex_];
}

void Editor::updateStaticGameState()
{
    staticGameState_.roomIndex = selectedRoomIndex_;
    staticGameState_.ballStates.clear();
    staticGameState_.dripStates.clear();
    staticGameState_.fishStates.clear();

    const auto& room = currentRoom();
    for (size_t i = 0; i < RoomData::numObjects; ++i)
    {
        const auto& object = room.theObjects[i];
        const ObjectKey key = {selectedRoomIndex_, i};

        switch (static_cast<ObjectType>(object.objectIs))
        {
        case ObjectType::ball:
        {
            BallState state;
            state.position = object.boundRect.bottom * 32;
            state.prevPosition = state.position;
            state.initialised = true;
            staticGameState_.ballStates.emplace(key, state);
            break;
        }
        case ObjectType::drip:
        {
            DripState state;
            state.initialised = true;
            staticGameState_.dripStates.emplace(key, state);
            break;
        }
        case ObjectType::fishBowl:
        {
            FishState state;
            state.position = (object.boundRect.top + 20) * 32;
            state.prevPosition = state.position;
            state.initialised = true;
            staticGameState_.fishStates.emplace(key, state);
            break;
        }
        default:
            break;
        }
    }
}

void Editor::openHouse(const std::string& path)
{
    auto result = HouseReader().read(path);
    if (!result.has_value())
    {
        statusMessage_ = "Failed to open: " + path;
        return;
    }
    house_ = std::move(*result);
    housePath_ = path;
    houseLoaded_ = true;
    selectedRoomIndex_ = 0;
    selectedObjectIndex_ = -1;
    activePaletteTool_ = ObjectType::nullObject;
    dirty_ = false;
    updateStaticGameState();
    statusMessage_ = "Opened: " + path;
}

void Editor::saveHouse()
{
    if (!houseLoaded_)
    {
        return;
    }
    if (housePath_.empty())
    {
        fileBrowser_.openForSave("Save House As", housePath_, [this](const std::string& path) { saveHouseAs(path); });
        return;
    }

    for (auto& room : house_.theRooms)
    {
        uint16_t count = 0;
        for (const auto& object : room.theObjects)
        {
            if (object.objectIs != 0)
            {
                ++count;
            }
        }
        room.numberOObjects = count;
    }

    if (HouseWriter().write(house_, housePath_))
    {
        dirty_ = false;
        statusMessage_ = "Saved: " + housePath_;
    }
    else
    {
        statusMessage_ = "Error: could not write to " + housePath_;
    }
}

void Editor::saveHouseAs(const std::string& path)
{
    if (path.empty())
    {
        return;
    }
    housePath_ = path;
    saveHouse();
    if (!dirty_ && pendingAction_ != PendingAction::none)
    {
        executePendingAction();
    }
}

void Editor::testHouse(std::optional<size_t> startRoom)
{
    if (!houseLoaded_)
    {
        return;
    }

    for (auto& room : house_.theRooms)
    {
        uint16_t count = 0;
        for (const auto& object : room.theObjects)
        {
            if (object.objectIs != 0)
            {
                ++count;
            }
        }
        room.numberOObjects = count;
    }

    const std::string tmpPath = tempHousePath();
    if (!HouseWriter().write(house_, tmpPath))
    {
        statusMessage_ = "Error: could not write test house to " + tmpPath;
        return;
    }

    std::vector<std::string> args;
    args.emplace_back(gameExecutablePath_);
    args.emplace_back("-f");
    args.emplace_back(tmpPath);
    args.emplace_back("-i");
    args.emplace_back(imagesDir_);
    if (startRoom.has_value())
    {
        args.emplace_back("-r");
        args.emplace_back(std::to_string(startRoom.value()));
    }

    if (launchProcess(args))
    {
        statusMessage_ = "Launched test game.";
    }
    else
    {
        statusMessage_ = "Error: failed to launch game process.";
    }
}

void Editor::newHouse()
{
    house_ = HouseRec {};
    house_.version = 2;
    addRoom();
    housePath_.clear();
    houseLoaded_ = true;
    statusMessage_ = "New house created.";
    dirty_ = false;
}

void Editor::addRoom()
{
    if (house_.numberORooms >= 40)
    {
        statusMessage_ = "House is full (40 rooms maximum).";
        return;
    }
    auto& room = house_.theRooms[house_.numberORooms];
    room = RoomData {};
    room.roomName = "New Room";
    room.backPictID = 200;
    for (size_t i = 0; i < 8; ++i)
    {
        room.tileOrder[i] = i;
    }
    room.leftOpen = 1;
    room.rightOpen = 1;

    selectedRoomIndex_ = static_cast<size_t>(house_.numberORooms);
    ++house_.numberORooms;
    selectedObjectIndex_ = -1;
    activePaletteTool_ = ObjectType::nullObject;
    updateStaticGameState();
    dirty_ = true;
}

void Editor::removeRoom(const size_t index)
{
    if (house_.numberORooms == 0)
    {
        return;
    }
    for (size_t i = index; i < static_cast<size_t>(house_.numberORooms) - 1; ++i)
    {
        house_.theRooms[i] = house_.theRooms[i + 1];
    }
    house_.theRooms[house_.numberORooms - 1] = RoomData {};
    --house_.numberORooms;

    if (house_.numberORooms == 0)
    {
        houseLoaded_ = false;
        selectedRoomIndex_ = 0;
    }
    else if (selectedRoomIndex_ >= static_cast<size_t>(house_.numberORooms))
    {
        selectedRoomIndex_ = static_cast<size_t>(house_.numberORooms) - 1;
    }

    selectedObjectIndex_ = -1;
    activePaletteTool_ = ObjectType::nullObject;
    updateStaticGameState();
    dirty_ = true;
}

std::pair<int32_t, int32_t> Editor::defaultObjectSize(const ObjectType type) const
{
    const auto it = paletteItems.find(type);
    if (it != paletteItems.end() && it->second.defaultSize.has_value())
    {
        return it->second.defaultSize.value();
    }

    const auto sourceRect = resources_.getSourceRect(type);
    if (sourceRect.w > 0 && sourceRect.h > 0)
    {
        return {sourceRect.w, sourceRect.h};
    }
    return {32, 32};
}

void Editor::confirmUnsaved(const PendingAction action)
{
    if (dirty_)
    {
        pendingAction_ = action;
        pendingUnsavedDialog_ = true;
    }
    else
    {
        pendingAction_ = action;
        executePendingAction();
    }
}

void Editor::executePendingAction()
{
    const PendingAction action = pendingAction_;
    pendingAction_ = PendingAction::none;
    switch (action)
    {
    case PendingAction::quit:
        running_ = false;
        break;
    case PendingAction::newHouse:
        newHouse();
        break;
    case PendingAction::openHouse:
        fileBrowser_.openForOpen("Open House File", [this](const std::string& path) { openHouse(path); });
        break;
    case PendingAction::none:
        break;
    }
}

void Editor::drawUnsavedChangesDialog()
{
    if (pendingUnsavedDialog_)
    {
        pendingUnsavedDialog_ = false;
        ImGui::OpenPopup("Unsaved Changes##dialog");
    }

    ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Unsaved Changes##dialog", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::TextWrapped("The house has unsaved changes. Save before continuing?");
        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(100.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
            saveHouse();
            if (!dirty_)
            {
                executePendingAction();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Discard", ImVec2(100.0f, 0.0f)))
        {
            dirty_ = false;
            ImGui::CloseCurrentPopup();
            executePendingAction();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
        {
            pendingAction_ = PendingAction::none;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
