#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "Game.h"
#include "HouseData.h"
#include "HouseReader.h"
#include "Preferences.h"
#include "Renderer.h"
#include "SoundResources.h"
#include "TitleScreen.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <optional>
#include <SDL2/SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <string>
#include <string_view>
#include <vector>

namespace
{

void printUsage(const char* programName)
{
    fprintf(stdout,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -f, --house <path>   House file to load (default: title screen)\n"
        "  -i, --images <dir>   Images directory (default: \"images/\")\n"
        "  -s, --sounds <dir>   Sounds directory (default: \"sounds/\")\n"
        "  -r, --room <n>       Starting room index (default: 0)\n"
        "  -a, --airflow        Enable airflow rendering\n"
        "  -h, --help           Show this help message\n",
        programName);
}

using SdlWindow = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
using SdlRenderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;

struct SdlAudio
{
    SdlAudio()
    {
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0)
        {
            std::fprintf(stderr, "SdlAudio: Mix_OpenAudio failed: %s\n", Mix_GetError());
            return;
        }
        Mix_AllocateChannels(16);
        opened_ = true;
    }

    ~SdlAudio()
    {
        if (opened_)
        {
            Mix_CloseAudio();
        }
    }

    bool opened_ = false;
};

struct SdlImage
{
    SdlImage()
    {
        IMG_Init(IMG_INIT_JPG);
    }

    ~SdlImage()
    {
        IMG_Quit();
    }
};

std::optional<std::vector<HouseRec>> loadHouseChain(const std::string& housePath, const std::filesystem::path& basePath)
{
    constexpr size_t maxHouses = 64;
    std::vector<HouseRec> houses;
    auto house = HouseReader().read(housePath);
    if (!house.has_value())
    {
        return std::nullopt;
    }
    houses.emplace_back(std::move(house.value()));
    while (houses.size() < maxHouses && !houses.back().nextFile.empty())
    {
        auto nextHouse = HouseReader().read((basePath / houses.back().nextFile).string());
        if (!nextHouse.has_value())
        {
            return std::nullopt;
        }
        houses.emplace_back(std::move(nextHouse.value()));
    }
    return houses;
}

void updateRendererWindowSize(Renderer& renderer, SDL_Window* window)
{
    int32_t windowWidth = 0;
    int32_t windowHeight = 0;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    renderer.updateForWindowSize(windowWidth, windowHeight);
}

} // namespace

int main(int argc, char* argv[])
{
#if defined(__APPLE__) || defined(__linux__)
    if (char* base = SDL_GetBasePath())
    {
        std::filesystem::current_path(base);
        SDL_free(base);
    }
#endif

    std::string housePath;
    std::string imagesDir = "images/";
    std::string soundsDir = "sounds/";
    size_t startRoom = 0;
    bool cliHouseSet = false;
    bool cliAirflowSet = false;

    for (size_t i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 < argc)
            {
                return argv[++i];
            }
            fprintf(stderr, "Option '%s' requires an argument\n", argv[i]);
            return nullptr;
        };
        if (arg == "-f" || arg == "--house")
        {
            if (const char* v = next())
            {
                housePath = v;
                cliHouseSet = true;
            }
            else
            {
                return -1;
            }
        }
        else if (arg == "-i" || arg == "--images")
        {
            if (const char* v = next())
            {
                imagesDir = v;
            }
            else
            {
                return -1;
            }
        }
        else if (arg == "-s" || arg == "--sounds")
        {
            if (const char* v = next())
            {
                soundsDir = v;
            }
            else
            {
                return -1;
            }
        }
        else if (arg == "-r" || arg == "--room")
        {
            if (const char* v = next())
            {
                startRoom = std::strtoul(v, nullptr, 10);
            }
            else
            {
                return -1;
            }
        }
        else if (arg == "-a" || arg == "--airflow")
        {
            cliAirflowSet = true;
        }
        else if (arg == "-h" || arg == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }
        else
        {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            printUsage(argv[0]);
            return -1;
        }
    }

    Preferences prefs = Preferences::load();
    if (cliAirflowSet)
    {
        prefs.showAirflow = true;
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SdlImage sdlImage;
    SdlAudio sdlAudio;

    constexpr uint32_t windowFlags = SDL_WINDOW_MAXIMIZED | SDL_WINDOW_RESIZABLE;
    static constexpr SDL_Rect screenRect = {0, 0, 512, 342};
    SdlWindow window(SDL_CreateWindow("Glider",
                         SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED,
                         screenRect.w * 2,
                         screenRect.h * 2,
                         windowFlags),
        SDL_DestroyWindow);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SdlRenderer sdlRenderer(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
        SDL_DestroyRenderer);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(window.get(), sdlRenderer.get());
    ImGui_ImplSDLRenderer2_Init(sdlRenderer.get());

    Resources resources(imagesDir, sdlRenderer.get());
    Renderer renderer(sdlRenderer.get(), resources, screenRect);
    updateRendererWindowSize(renderer, window.get());
    SoundResources sounds(soundsDir);

    enum class AppState
    {
        titleScreen,
        playing,
    };
    auto appState = cliHouseSet ? AppState::playing : AppState::titleScreen;

    while (true)
    {
        if (appState == AppState::titleScreen)
        {
            TitleScreen titleScreen(window.get(), sdlRenderer.get(), prefs, &resources, renderer);
            auto action = titleScreen.run();
            if (action.result == TitleScreenResult::quit)
            {
                break;
            }
            if (action.result == TitleScreenResult::loadHouse)
            {
                housePath = action.housePath;
            }
            else
            {
                housePath = "The House";
            }
            appState = AppState::playing;
        }
        else
        {
            const auto basePath = std::filesystem::path(housePath).parent_path();
            auto houses = loadHouseChain(housePath, basePath);
            if (!houses.has_value())
            {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                    "Error",
                    ("Failed to load house: " + housePath).c_str(),
                    window.get());
                appState = AppState::titleScreen;
                continue;
            }
            renderer.setShowAirflow(prefs.showAirflow);
            updateRendererWindowSize(renderer, window.get());
            Game game(std::move(*houses), renderer, sounds, prefs, startRoom);
            if (game.run() == GameResult::quit)
            {
                break;
            }
            appState = AppState::titleScreen;
        }
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return SDL_main(__argc, __argv);
}
#endif
