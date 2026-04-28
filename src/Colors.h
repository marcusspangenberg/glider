#pragma once

#include <SDL2/SDL.h>

namespace Colors
{

constexpr SDL_Color white = {255, 255, 255, 255};
constexpr SDL_Color black = {0, 0, 0, 255};
constexpr SDL_Color red = {255, 0, 0, 255};
constexpr SDL_Color green = {0, 255, 0, 255};
constexpr SDL_Color blue = {0, 0, 255, 255};
constexpr SDL_Color yellow = {255, 255, 0, 255};
constexpr SDL_Color magenta = {255, 0, 255, 255};
constexpr SDL_Color cyan = {0, 255, 255, 255};
constexpr SDL_Color gray = {128, 128, 128, 255};
constexpr SDL_Color lightGray = {192, 192, 192, 255};
constexpr SDL_Color darkGray = {64, 64, 64, 255};
constexpr SDL_Color darkBlue = {0, 0, 128, 255};
constexpr SDL_Color darkGreen = {0, 128, 0, 255};
constexpr SDL_Color darkCyan = {0, 128, 128, 255};
constexpr SDL_Color darkRed = {128, 0, 0, 255};
constexpr SDL_Color darkMagenta = {128, 0, 128, 255};
constexpr SDL_Color darkYellow = {128, 128, 0, 255};
constexpr SDL_Color brown = {144, 94, 0, 255};
constexpr SDL_Color lightBrown = {190, 164, 107, 255};
constexpr SDL_Color orange = {255, 128, 0, 255};
constexpr SDL_Color pink = {255, 128, 192, 255};
constexpr SDL_Color transparent = {0, 0, 0, 0};

} // namespace Colors
