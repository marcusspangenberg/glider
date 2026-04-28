#pragma once

#include <cstdint>

struct PlayerProgress
{
    int32_t lives = 4;
    int32_t score = 0;
    int32_t energy = 0;
    int32_t bands = 0;
    int32_t loopsInRoom = 0;
    int32_t roomsCompleted = 0;
};
