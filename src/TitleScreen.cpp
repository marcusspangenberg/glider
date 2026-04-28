#include "TitleScreen.h"
#include "Renderer.h"
#include <cstdint>
#include <cstdio>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

TitleScreen::TitleScreen(SDL_Window* window,
    SDL_Renderer* renderer,
    Preferences& prefs,
    Resources* resources,
    Renderer& gameRenderer)
    : window_(window),
      renderer_(renderer),
      prefs_(prefs),
      resources_(resources),
      gameRenderer_(gameRenderer)
{
}

TitleScreenAction TitleScreen::run()
{
    running_ = true;
    pendingAction_ = {};

    while (running_)
    {
        processEvents();

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        drawUI();

        ImGui::Render();

        SDL_SetRenderDrawColor(renderer_, 30, 30, 30, 255);
        SDL_RenderClear(renderer_);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
        SDL_RenderPresent(renderer_);
    }

    return pendingAction_;
}

void TitleScreen::processEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (rebindingControl_ >= 0 && event.type == SDL_KEYDOWN)
        {
            const auto scanCode = event.key.keysym.scancode;
            if (scanCode != SDL_SCANCODE_ESCAPE)
            {
                switch (rebindingControl_)
                {
                case 0:
                    prefsKeyLeft_ = scanCode;
                    break;
                case 1:
                    prefsKeyRight_ = scanCode;
                    break;
                case 2:
                    prefsKeyThrust_ = scanCode;
                    break;
                case 3:
                    prefsKeyFireBand_ = scanCode;
                    break;
                default:
                    break;
                }
            }
            rebindingControl_ = -1;
            continue;
        }

        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT)
        {
            pendingAction_ = {TitleScreenResult::quit, {}};
            running_ = false;
        }
        else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            const int32_t windowWidth = event.window.data1;
            const int32_t windowHeight = event.window.data2;
            gameRenderer_.updateForWindowSize(windowWidth, windowHeight);
        }
    }
}

void TitleScreen::drawUI()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Game"))
        {
            if (ImGui::MenuItem("New Game"))
            {
                pendingAction_ = {TitleScreenResult::newGame, {}};
                running_ = false;
            }
            if (ImGui::MenuItem("Load House..."))
            {
                fileBrowser_.openForOpen("Load House", [this](const std::string& path) {
                    pendingAction_ = {TitleScreenResult::loadHouse, path};
                    running_ = false;
                });
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit"))
            {
                pendingAction_ = {TitleScreenResult::quit, {}};
                running_ = false;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options"))
        {
            if (ImGui::MenuItem("Preferences..."))
            {
                showPrefsDialog_ = true;
                prefsAirflow_ = prefs_.showAirflow;
                prefsKeyLeft_ = prefs_.keyLeft;
                prefsKeyRight_ = prefs_.keyRight;
                prefsKeyThrust_ = prefs_.keyThrust;
                prefsKeyFireBand_ = prefs_.keyFireBand;
                rebindingControl_ = -1;
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
        ImGui::EndMainMenuBar();
    }

    fileBrowser_.draw();

    drawAboutDialog();

    if (showPrefsDialog_)
    {
        ImGui::OpenPopup("Preferences");
        showPrefsDialog_ = false;
    }
    if (ImGui::BeginPopupModal("Preferences", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Checkbox("Show air flow", &prefsAirflow_);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Key Bindings");
        ImGui::Spacing();

        const std::array scancodes {&prefsKeyLeft_, &prefsKeyRight_, &prefsKeyThrust_, &prefsKeyFireBand_};

        for (int32_t i = 0; i < 4; ++i)
        {
            constexpr std::array<std::string_view, 4> actionNames {"Left", "Right", "Battery", "Rubber band"};
            ImGui::Text("%-10s", actionNames[i].data());
            ImGui::SameLine(100.0f);
            char buttonLabel[64];
            if (rebindingControl_ == i)
            {
                std::snprintf(buttonLabel, sizeof(buttonLabel), "[press key...]##bind%d", i);
            }
            else
            {
                std::snprintf(buttonLabel, sizeof(buttonLabel), "%-16s##bind%d", SDL_GetScancodeName(*scancodes[i]), i);
            }
            if (ImGui::Button(buttonLabel, ImVec2(160.0f, 0.0f)))
            {
                rebindingControl_ = i;
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)))
        {
            prefs_.showAirflow = prefsAirflow_;
            prefs_.keyLeft = prefsKeyLeft_;
            prefs_.keyRight = prefsKeyRight_;
            prefs_.keyThrust = prefsKeyThrust_;
            prefs_.keyFireBand = prefsKeyFireBand_;
            prefs_.save();
            rebindingControl_ = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        {
            rebindingControl_ = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.20f),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));
    ImGui::Begin("##title",
        nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground
            | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::Text(
        "Fly your glider from room to room avoiding\nsome objects while seeking out others.\nIf you are successful you "
        "will reach the last\nroom and \"escape\" the house!");
    ImGui::Spacing();
    ImGui::TextDisabled("Game > New Game to start");
    ImGui::End();

    drawCatalog();
}

void TitleScreen::drawAboutDialog()
{
    if (showAboutDialog_)
    {
        ImGui::OpenPopup("About Glider");
        showAboutDialog_ = false;
    }
    if (ImGui::BeginPopupModal("About Glider", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Glider");
        ImGui::Spacing();
        ImGui::TextUnformatted("A reimplementation of Glider 4,");
        ImGui::TextUnformatted("originally designed by John Calhoun (softdorothy).");
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
}

void TitleScreen::drawCatalog() const
{
    if (!resources_)
    {
        return;
    }
    auto* texture = resources_->getTexture(128);
    if (!texture)
    {
        return;
    }
    int32_t textureWidth = 0;
    int32_t textureHeight = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight);
    if (textureWidth == 0 || textureHeight == 0)
    {
        return;
    }

    const auto textureId = reinterpret_cast<ImTextureID>(texture);
    const auto widthFloat = static_cast<float>(textureWidth);
    const auto heightFloat = static_cast<float>(textureHeight);
    const auto renderScale
        = static_cast<float>(gameRenderer_.getRenderScale(static_cast<int32_t>(ImGui::GetIO().DisplaySize.x),
              static_cast<int32_t>(ImGui::GetIO().DisplaySize.y)))
        / 2.0f;
    const float textLineHeight = ImGui::GetTextLineHeight();
    const float spriteColWidth = 64.0f * renderScale + ImGui::GetStyle().ItemSpacing.x * 2.0f;

    auto drawRow = [&](const SDL_Rect sourceRect, const std::string_view description) {
        const auto x = static_cast<float>(sourceRect.x);
        const auto y = static_cast<float>(sourceRect.y);
        const auto w = static_cast<float>(sourceRect.w);
        const auto h = static_cast<float>(sourceRect.h);

        const ImVec2 uv0(x / widthFloat, y / heightFloat);
        const ImVec2 uv1((x + w) / widthFloat, (y + h) / heightFloat);
        const ImVec2 spriteSize(w * renderScale, h * renderScale);
        ImGui::Image(textureId, spriteSize, uv0, uv1);
        ImGui::SameLine(spriteColWidth);
        const auto topY = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(topY + (spriteSize.y - textLineHeight) * 0.5f);
        ImGui::TextUnformatted(description.data());
        ImGui::SetCursorPosY(topY + spriteSize.y + ImGui::GetStyle().ItemSpacing.y);
    };

    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.33f),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.0f));
    constexpr ImGuiWindowFlags catalogFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##catalog", nullptr, catalogFlags);
    drawRow(resources_->getGliderSourceRect(0), "Your glider");
    ImGui::Spacing();

    drawRow(resources_->getEnemyRects(AnimateKind::DART)[1], "Dart");
    drawRow(resources_->getEnemyRects(AnimateKind::COPTER)[1], "Copter");
    drawRow(resources_->getEnemyRects(AnimateKind::BALOON)[3], "Balloon");

    ImGui::Spacing();
    drawRow(resources_->getSourceRect(ObjectType::battery), "Battery - speed boost");
    drawRow(resources_->getSourceRect(ObjectType::rubberBand), "Rubber bands - your only defense");
    drawRow(resources_->getSourceRect(ObjectType::clock), "Clock - bonus points");
    drawRow(resources_->getSourceRect(ObjectType::paper), "Extra life");

    ImGui::End();
}
