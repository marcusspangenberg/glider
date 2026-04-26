#pragma once

#include "Constants.h"
#include "ObjectKey.h"
#include "Rect.h"
#include <cstddef>
#include <cstdint>
#include <unordered_set>

enum class GliderMode
{
    normal,
    turningRtToLf,
    turningLfToRt,
    ascending,
    descending,
    burning,
    fadingOut,
    fadingIn,
    shredding,
};

struct PlayerState
{
    bool isRight = true;
    bool isForward = true;
    size_t srcNum = 0;
    int32_t forVel = 0;
    int32_t liftAmount = constants::gravityAmount;
    int32_t shiftAmount = 0;
    bool enteredLeft = true;
    GliderMode mode = GliderMode::fadingIn;
    int32_t turnPhase = 0;
    int32_t verticalTransitionCooldown = 0;
    uint32_t burnUntil = 0;
    int32_t burnPhase = 0;
    int32_t fadePhase = 0;
    int32_t shredBlade = 0;
    bool sliding = false;
    bool bandBorne = false;
    int32_t bandPhase = 0;
    int32_t bandVelocity = 0;
    Rect bandDest {0, 0, 0, 0};
    Rect destRect {.top = constants::ceilingVert, .left = 0, .bottom = constants::ceilingVert + 20, .right = 48};
    Rect prevRect {.top = constants::ceilingVert, .left = 0, .bottom = constants::ceilingVert + 20, .right = 48};
    std::unordered_set<ObjectKey, ObjectKeyHash> touchingObjects;
};
