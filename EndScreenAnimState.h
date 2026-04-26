#pragma once

#include <cstdint>
#include <utility>
#include <vector>

struct EndScreenAnimState
{
    int32_t tick = 0;
    int32_t animSubTick = 0;
    int32_t twisterX = 230;
    int32_t twisterFrame = 0;
    int32_t gliderX = 420;
    int32_t gliderY = 90;
    bool boltActive = false;
    bool boltErase = false;
    std::vector<std::pair<int32_t, int32_t>> boltSegments;
    bool done = false;
};
