#pragma once

#include "ObjectKey.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

struct OutletState
{
    bool isSparking = false;
    uint32_t stateChangeAt = 0; // animTick when state next changes (0 = not yet initialized)
};

struct DripState
{
    int32_t phase = 53; // 53-56: forming; 57: falling
    int32_t position = 0; // fixed-point Y of drop bottom (×32)
    int32_t prevPosition = 0; // position snapshot before last physics tick, for interpolation
    int32_t velocity = 0; // fixed-point velocity (×32) per tick
    bool initialised = false;
};

struct ToasterState
{
    int32_t phase = 60; // 60-65: toast frame cycling; 0=paused
    int32_t position = 0; // fixed-point Y of toast bottom (×32)
    int32_t prevPosition = 0; // position snapshot before last physics tick, for interpolation
    int32_t velocity = 0; // fixed-point velocity; negative=upward
    int32_t bottomPos = 0; // fixed-point Y of the toaster slot floor
    uint32_t pauseUntil = 0; // animTick when pause ends
    bool paused = false;
    bool initialised = false;
};

struct FishState
{
    int32_t phase = 69; // 66=going up, 68=going down, 69=horizontal; 0=paused
    int32_t position = 0; // fixed-point Y of fish bottom (×32)
    int32_t prevPosition = 0; // position snapshot before last physics tick, for interpolation
    int32_t velocity = 0; // fixed-point velocity; negative=upward
    int32_t bottomPos = 0; // fixed-point Y of the bounce floor
    uint32_t pauseUntil = 0; // animTick when pause ends
    bool paused = false;
    bool initialised = false;
};

struct WindowState
{
    int32_t bolts[3][8][2] = {}; // [bolt 0-2][joint 0-7][0=x, 1=y]
    int32_t whichBolt = 0; // 0-based active bolt
    int32_t whatPhase = 0; // 0=waiting; 1-3=bolt flash; 4=black fill
    uint32_t whatTime = 0; // animTick when next flash triggers
    bool initialised = false;
};

struct BallState
{
    int32_t position = 0; // fixed-point Y of ball bottom (×32)
    int32_t prevPosition = 0; // position snapshot before last physics tick, for interpolation
    int32_t velocity = 0; // fixed-point velocity (×32) per tick; negative=upward
    int32_t reset = 0; // fixed-point Y of the floor (×32)
    bool initialised = false;
};

struct GreaseState
{
    int32_t reset = 0; // 0=upright, 1=triggered, 2-4=spill frames, 5+=extending, 999=fully spilled
    int32_t currentRight = 0; // current right edge of the spill line
    bool initialised = false;
};

struct TeaKtlState
{
    int32_t phase = 0; // 0-10: increments each tick while steaming
    bool steamActive = false; // true while steam is blowing
    uint32_t nextEventAt = 0; // animTick when steam next starts
    bool initialised = false;
};

struct EnemyInstance
{
    int32_t left = 0;
    int32_t top = 0;
    int32_t right = 0;
    int32_t bottom = 0; // pixel position
    int32_t prevLeft = 0; // position snapshot before last physics tick, for interpolation
    int32_t prevTop = 0; // position snapshot before last physics tick, for interpolation
    int32_t horizontalOffset = 0; // pixels/tick velocity
    int32_t verticalOffset = 0; // pixels/tick velocity
    int32_t phase = 0; // 0-7 cycling; -1 = crushed/popped
    uint32_t tickStamp = 0; // animTick when this enemy respawns
    bool unSeen = true; // starts unseen; spawns on first tick
};

struct RoomAnimateState
{
    std::array<EnemyInstance, 16> enemies;
    bool initialised = false;
};

struct GameState
{
    size_t roomIndex = 0;
    uint32_t animTick = 0;
    std::unordered_map<ObjectKey, WindowState, ObjectKeyHash> windowStates;
    std::unordered_map<ObjectKey, OutletState, ObjectKeyHash> outletStates;
    std::unordered_map<ObjectKey, DripState, ObjectKeyHash> dripStates;
    std::unordered_map<ObjectKey, BallState, ObjectKeyHash> ballStates;
    std::unordered_map<ObjectKey, FishState, ObjectKeyHash> fishStates;
    std::unordered_map<ObjectKey, ToasterState, ObjectKeyHash> toasterStates;
    std::unordered_map<ObjectKey, TeaKtlState, ObjectKeyHash> teaKtlStates;
    std::unordered_map<ObjectKey, GreaseState, ObjectKeyHash> greaseStates;
    std::unordered_map<size_t, RoomAnimateState> animateStates;
};
