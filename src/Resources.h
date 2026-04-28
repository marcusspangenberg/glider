#pragma once

#include "HouseData.h"
#include "ObjectType.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <SDL2/SDL.h>
#include <string_view>
#include <unordered_map>
#include <vector>

class Resources
{
public:
    Resources(const std::string_view& directoryName, SDL_Renderer* renderer);
    ~Resources();

    [[nodiscard]] SDL_Texture* getTexture(uint32_t id) const;
    [[nodiscard]] SDL_Rect getSourceRect(ObjectType objectType) const;
    [[nodiscard]] const std::vector<SDL_Rect>& getSourceRects(ObjectType objectType) const;
    [[nodiscard]] SDL_Rect getGliderSourceRect(size_t srcNum) const;
    [[nodiscard]] SDL_Rect getGliderShadowRect(size_t shadoNum) const;
    // index 0=dead frame, 1..N=live animation frames
    [[nodiscard]] const std::vector<SDL_Rect>& getEnemyRects(AnimateKind animateKind) const;
    [[nodiscard]] const std::array<SDL_Rect, 3>& getBandRects() const;

private:
    std::unordered_map<uint32_t, SDL_Texture*> textures_;
    std::unordered_map<ObjectType, std::vector<SDL_Rect>> sourceRects_;
    // srcNum: 0=right forward, 1=right tipped, 2=left forward, 3=left tipped, 4-9=turn frames,
    //         24-27=burn frames (right frame0/1, left frame0/1)
    std::array<SDL_Rect, 28> gliderRects_ {};
    // shadoNum: 0=right-facing, 1=left-facing
    std::array<SDL_Rect, 2> shadowRects_ {};
    // AnimateKind::DART/COPTER/BALOON; index 0=dead frame, 1..N=live animation frames
    std::array<std::vector<SDL_Rect>, 3> enemyRects_ {};
    std::array<SDL_Rect, 3> bandRects_ {};
};
