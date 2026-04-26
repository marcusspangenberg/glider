#pragma once

#include <cstdint>
#include <SDL_rect.h>

struct Rect
{
    uint16_t top = 0;
    uint16_t left = 0;
    uint16_t bottom = 0;
    uint16_t right = 0;

    [[nodiscard]] constexpr SDL_Rect toSDLRect() const
    {
        return {left, top, right - left, bottom - top};
    }
};
