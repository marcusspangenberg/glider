#pragma once

#include <SDL_rect.h>

struct WindowPanes
{
    SDL_Rect top {0, 0, 0, 0};
    SDL_Rect bot {0, 0, 0, 0};
};

constexpr WindowPanes computeWindowPanes(const SDL_Rect windowRect, const bool isOpen)
{
    if (isOpen)
    {
        return {
            .top = {windowRect.x + 18, windowRect.y + 34, windowRect.w - 36, windowRect.h / 2 - 34},
            .bot = {windowRect.x + 9, windowRect.y + windowRect.h / 2 + 11, windowRect.w - 17, windowRect.h / 2 - 27},
        };
    }

    return {
        .top = {windowRect.x + 18, windowRect.y + 26, windowRect.w - 36, windowRect.h / 2 - 34},
        .bot = {windowRect.x + 18, windowRect.y + windowRect.h / 2 + 8, windowRect.w - 36, windowRect.h / 2 - 34},
    };
}
