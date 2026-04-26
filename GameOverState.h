#pragma once

#include <cstdint>
#include <vector>

struct GameOverLetterDraw
{
    int32_t srcIdx = 0;
    int32_t destX = 0;
    int32_t destY = 0;
};

struct GameOverAnimState
{
    int32_t phase = 0; // 0=reveal, 1=fall, 2=wait
    int32_t revealTimer = 0; // counts up to 6 per letter
    int32_t revealNext = 0; // 0..7
    int32_t fallIter = 1; // 1..20
    int32_t fallLetter = 0; // 0..7 within current iter
    int32_t fallX = 0; // current x within iter (reset each iter)
    int32_t waitTick = 0; // 0..200
    std::vector<GameOverLetterDraw> letters;
};
