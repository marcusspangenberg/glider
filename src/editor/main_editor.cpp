#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "Editor.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <SDL2/SDL.h>
#include <SDL_image.h>
#include <string>
#include <string_view>

namespace
{

void printUsage(const char* programName)
{
    fprintf(stdout,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -i, --images <dir>   Images directory (default: \"images/\")\n"
        "  -f, --house <path>   House file to open on startup\n"
        "  -h, --help           Show this help message\n",
        programName);
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

    std::string imagesDir = "images/";
    std::string startupHousePath;

    for (size_t i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 < argc)
            {
                return argv[++i];
            }
            fprintf(stderr, "Option '%s' requires an argument\n", argv[i]);
            return nullptr;
        };
        if (arg == "-i" || arg == "--images")
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
        else if (arg == "-f" || arg == "--house")
        {
            if (const char* v = next())
            {
                startupHousePath = v;
            }
            else
            {
                return -1;
            }
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

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);

    SDL_Window* window = SDL_CreateWindow("Gliderport Editor",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1400,
        900,
        SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);

    if (!renderer)
    {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(1.0f);
    ImGui::GetIO().Fonts->AddFontDefault()->Scale = 1.0f;

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    {
#ifdef __APPLE__
        // When both .app bundles are in the same folder, find Gliderport.app next to GliderportEditor.app
        const auto editorApp = std::filesystem::path(argv[0]).parent_path().parent_path().parent_path();
        const auto bundleExe = editorApp.parent_path() / "Glider.app" / "Contents" / "MacOS" / "Glider";
        const auto gameExePath = std::filesystem::exists(bundleExe)
            ? bundleExe.string()
            : (std::filesystem::path(argv[0]).parent_path() / "glider").string();
#else
        const auto gameExePath = (std::filesystem::path(argv[0]).parent_path() / "glider").string();
#endif
        Editor editor(window, renderer, imagesDir, gameExePath);
        if (!startupHousePath.empty())
        {
            editor.openHouse(startupHousePath);
        }
        editor.run();
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    IMG_Quit();
    SDL_Quit();

    return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return SDL_main(__argc, __argv);
}
#endif
