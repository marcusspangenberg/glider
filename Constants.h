#pragma once

#include <cstdint>

namespace constants
{

constexpr uint32_t ceilingVert = 24;
constexpr uint32_t airMaxVert = 44;
constexpr uint32_t stairVert = 54;
constexpr uint32_t floorVert = 325;
constexpr uint32_t floorLimit = floorVert + 5;

constexpr int32_t maxThrust = 5;
constexpr int32_t liftVentAmount = -7;
constexpr int32_t dropVentAmount = 7;
constexpr int32_t gravityAmount = 2;
constexpr int32_t fanThrustAmount = -7;
constexpr int32_t batteryBoost = 16;
constexpr int32_t initialLives = 4;

} // namespace constants
