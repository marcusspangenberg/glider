#include "Resources.h"
#include <cassert>
#include <ranges>
#include <SDL_image.h>
#include <string>

Resources::Resources(const std::string_view& directoryName, SDL_Renderer* renderer)
{
    gliderRects_ = {};
    // srcNum: 0=right forward, 1=right tipped, 2=left forward, 3=left tipped, 4-9=turn frames
    gliderRects_[0] = {0, 0, 48, 20};
    gliderRects_[1] = {0, 21, 48, 20};
    gliderRects_[2] = {0, 42, 48, 20};
    gliderRects_[3] = {0, 63, 48, 20};
    gliderRects_[4] = {208, 0, 48, 20};
    gliderRects_[5] = {208, 21, 48, 20};
    gliderRects_[6] = {208, 42, 48, 20};
    gliderRects_[7] = {208, 63, 48, 20};
    gliderRects_[8] = {208, 84, 48, 20};
    gliderRects_[9] = {208, 105, 48, 20};
    // Burn frames (48×36): right-facing frame0/1, left-facing frame0/1
    gliderRects_[24] = {256, 24, 48, 36};
    gliderRects_[25] = {256, 61, 48, 36};
    gliderRects_[26] = {256, 98, 48, 36};
    gliderRects_[27] = {256, 135, 48, 36};

    // shadoNum: 0=right-facing, 1=left-facing
    shadowRects_[0] = {256, 0, 48, 11};
    shadowRects_[1] = {256, 12, 48, 11};

    for (const auto ids = {128, 129, 130, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209};
         const auto id : ids)
    {
        std::string fileName;
        fileName.append(directoryName);
        fileName.append(std::to_string(id));
        fileName.append(".png");
        if (auto texture = IMG_LoadTexture(renderer, fileName.c_str()))
        {
            textures_.emplace(id, texture);
        }
    }

    sourceRects_.emplace(ObjectType::ceilingVent, std::vector<SDL_Rect> {{0, 84, 48, 12}});
    sourceRects_.emplace(ObjectType::ceilingDuct, std::vector<SDL_Rect> {{0, 97, 48, 13}});
    sourceRects_.emplace(ObjectType::floorVent, std::vector<SDL_Rect> {{0, 111, 48, 13}});
    sourceRects_.emplace(ObjectType::paper, std::vector<SDL_Rect> {{0, 125, 48, 21}});
    sourceRects_.emplace(ObjectType::toaster,
        std::vector<SDL_Rect> {
            {0, 147, 38, 27}, // 0: toaster body
            {304, 84, 32, 31}, // 1: toast phase 60
            {304, 116, 32, 31}, // 2: toast phase 61
            {304, 148, 32, 31}, // 3: toast phase 62
            {304, 180, 32, 31}, // 4: toast phase 63
            {304, 212, 32, 31}, // 5: toast phase 64
            {304, 244, 32, 31}, // 6: toast phase 65
        });
    sourceRects_.emplace(ObjectType::teaKettle, std::vector<SDL_Rect> {{0, 175, 41, 30}});
    sourceRects_.emplace(ObjectType::leftFan, std::vector<SDL_Rect> {{0, 206, 35, 55}});
    sourceRects_.emplace(ObjectType::rightFan, std::vector<SDL_Rect> {{0, 262, 35, 54}});
    sourceRects_.emplace(ObjectType::table, std::vector<SDL_Rect> {{48, 23, 64, 22}});
    sourceRects_.emplace(ObjectType::shredder, std::vector<SDL_Rect> {{48, 46, 64, 24}});
    sourceRects_.emplace(ObjectType::books, std::vector<SDL_Rect> {{48, 71, 64, 55}});
    sourceRects_.emplace(ObjectType::clock, std::vector<SDL_Rect> {{112, 0, 32, 29}});
    sourceRects_.emplace(ObjectType::candle,
        std::vector<SDL_Rect> {
            {112, 30, 32, 21}, // 0: candle body
            {144, 189, 16, 12}, // 1: flame frame 0
            {144, 202, 16, 12}, // 2: flame frame 1
            {144, 215, 16, 12}, // 3: flame frame 2
        });
    sourceRects_.emplace(ObjectType::rubberBand, std::vector<SDL_Rect> {{112, 52, 32, 23}});
    sourceRects_.emplace(ObjectType::ball, std::vector<SDL_Rect> {{112, 76, 32, 32}});
    sourceRects_.emplace(ObjectType::fishBowl,
        std::vector<SDL_Rect> {
            {112, 109, 32, 29}, // 0: bowl body
            {144, 109, 16, 16}, // 1: fish going up (phase 66)
            {144, 126, 16, 16}, // 2: fish unused (phase 67)
            {144, 143, 16, 16}, // 3: fish going down (phase 68)
            {144, 160, 16, 16}, // 4: fish horizontal (phase 69)
        });
    sourceRects_.emplace(ObjectType::grease,
        std::vector<SDL_Rect> {
            {112, 139, 32, 29}, // 0: upright bottle
            {112, 169, 32, 29}, // 1: falling/tipping
            {112, 199, 32, 29}, // 3: spilled
        });
    sourceRects_.emplace(ObjectType::lightSwitch, std::vector<SDL_Rect> {{142, 0, 18, 26}});
    sourceRects_.emplace(ObjectType::thermostat, std::vector<SDL_Rect> {{144, 27, 18, 27}});
    sourceRects_.emplace(ObjectType::outlet,
        std::vector<SDL_Rect> {
            {160, 264, 32, 25}, // 0: outlet body
            {160, 290, 32, 25}, // 1: spark frame 1
            {160, 316, 32, 25}, // 2: spark frame 2
        });
    sourceRects_.emplace(ObjectType::powerSwitch, std::vector<SDL_Rect> {{144, 82, 18, 26}});
    sourceRects_.emplace(ObjectType::guitar, std::vector<SDL_Rect> {{48, 127, 64, 170}});
    sourceRects_.emplace(ObjectType::drip,
        std::vector<SDL_Rect> {
            {192, 0, 16, 13}, // 0: phase 53 — small bead forming
            {192, 14, 16, 13}, // 1: phase 54
            {192, 28, 16, 13}, // 2: phase 55
            {192, 42, 16, 13}, // 3: phase 56 — full bead, about to fall
            {192, 56, 16, 14}, // 4: phase 57 — falling drop
        });
    sourceRects_.emplace(ObjectType::shelf, std::vector<SDL_Rect> {{192, 71, 16, 29}});
    sourceRects_.emplace(ObjectType::basket, std::vector<SDL_Rect> {{448, 270, 63, 71}});
    sourceRects_.emplace(ObjectType::painting, std::vector<SDL_Rect> {{408, 53, 102, 93}});
    sourceRects_.emplace(ObjectType::battery, std::vector<SDL_Rect> {{144, 55, 16, 26}});
    sourceRects_.emplace(ObjectType::macintosh, std::vector<SDL_Rect> {{256, 209, 45, 58}});
    sourceRects_.emplace(ObjectType::upStairs, std::vector<SDL_Rect> {{0, 0, 161, 254}});
    sourceRects_.emplace(ObjectType::downStairs, std::vector<SDL_Rect> {{0, 0, 161, 254}});

    // animateKind 0=dart: [dead, alive×8] — dart has no animation, same frame for all phases 0-7
    enemyRects_[0] = {
        SDL_Rect {304, 0, 64, 22}, // dead
        SDL_Rect {48, 0, 64, 22}, // phase 0
        SDL_Rect {48, 0, 64, 22}, // phase 1
        SDL_Rect {48, 0, 64, 22}, // phase 2
        SDL_Rect {48, 0, 64, 22}, // phase 3
        SDL_Rect {48, 0, 64, 22}, // phase 4
        SDL_Rect {48, 0, 64, 22}, // phase 5
        SDL_Rect {48, 0, 64, 22}, // phase 6
        SDL_Rect {48, 0, 64, 22}, // phase 7
    };
    // animateKind 1=copter: [dead, frame0..7]
    enemyRects_[1] = {
        SDL_Rect {304, 276, 32, 32}, // dead
        SDL_Rect {160, 0, 32, 32}, // frame 0
        SDL_Rect {160, 33, 32, 32}, // frame 1
        SDL_Rect {160, 66, 32, 32}, // frame 2
        SDL_Rect {160, 99, 32, 32}, // frame 3
        SDL_Rect {160, 132, 32, 32}, // frame 4
        SDL_Rect {160, 165, 32, 32}, // frame 5
        SDL_Rect {160, 198, 32, 32}, // frame 6
        SDL_Rect {160, 231, 32, 32}, // frame 7
    };
    // animateKind 2=balloon: [dead, frame0..7]
    enemyRects_[2] = {
        SDL_Rect {304, 309, 32, 32}, // dead
        SDL_Rect {112, 229, 32, 32}, // frame 0
        SDL_Rect {112, 229, 32, 32}, // frame 1
        SDL_Rect {112, 262, 32, 32}, // frame 2
        SDL_Rect {112, 262, 32, 32}, // frame 3
        SDL_Rect {112, 295, 32, 32}, // frame 4
        SDL_Rect {112, 295, 32, 32}, // frame 5
        SDL_Rect {112, 262, 32, 32}, // frame 6
        SDL_Rect {112, 262, 32, 32}, // frame 7
    };

    bandRects_[0] = {192, 155, 16, 7};
    bandRects_[1] = {192, 163, 16, 7};
    bandRects_[2] = {192, 171, 16, 7};
}

Resources::~Resources()
{
    for (const auto& texture : textures_ | std::views::values)
    {
        SDL_DestroyTexture(texture);
    }
}

SDL_Texture* Resources::getTexture(const uint32_t id) const
{
    const auto it = textures_.find(id);
    if (it == textures_.end())
    {
        assert(false);
        return nullptr;
    }
    return it->second;
}

SDL_Rect Resources::getGliderSourceRect(const size_t srcNum) const
{
    assert(srcNum < gliderRects_.size());
    return gliderRects_[srcNum];
}

SDL_Rect Resources::getGliderShadowRect(const size_t shadoNum) const
{
    assert(shadoNum < shadowRects_.size());
    return shadowRects_[shadoNum];
}

SDL_Rect Resources::getSourceRect(const ObjectType objectType) const
{
    const auto it = sourceRects_.find(objectType);
    if (it == sourceRects_.end() || it->second.empty())
    {
        return {0, 0, 0, 0};
    }
    return it->second[0];
}

const std::vector<SDL_Rect>& Resources::getSourceRects(const ObjectType objectType) const
{
    const auto it = sourceRects_.find(objectType);
    assert(it != sourceRects_.end());
    return it->second;
}

const std::vector<SDL_Rect>& Resources::getEnemyRects(const AnimateKind animateKind) const
{
    const auto index = static_cast<size_t>(animateKind);
    assert(index < enemyRects_.size());
    return enemyRects_[index];
}

const std::array<SDL_Rect, 3>& Resources::getBandRects() const
{
    return bandRects_;
}
