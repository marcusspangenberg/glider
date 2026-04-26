#pragma once

#include "Rect.h"
#include <array>
#include <cstdint>
#include <string>

enum class ConditionCode : uint16_t
{
    NONE = 0,
    AIR_OUT = 1,
    LIGHTS_OUT = 2
};

enum class AnimateKind : uint16_t
{
    DART = 0,
    COPTER = 1,
    BALOON = 2
};

struct Point
{
    uint16_t v = 0;
    uint16_t h = 0;
};

struct ObjectData
{
    uint16_t objectIs = 0;
    Rect boundRect {0, 0, 0, 0};
    uint16_t amount = 0;
    uint16_t extra = 0;
    uint8_t isOn = 0;
};

struct RoomData
{
    static constexpr size_t numObjects = 16;

    std::string roomName;
    uint16_t numberOObjects = 0;
    uint16_t backPictID = 200;
    std::array<uint16_t, 8> tileOrder {};
    uint8_t leftOpen = 1;
    uint8_t rightOpen = 1;
    AnimateKind animateKind = AnimateKind::DART;
    uint16_t animateNumber = 0;
    uint32_t animateDelay = 0;
    ConditionCode conditionCode = ConditionCode::NONE;
    std::array<ObjectData, numObjects> theObjects {};
    std::array<Rect, numObjects> eventRects {};
};

struct HouseRec
{
    static constexpr size_t numRooms = 40;
    static constexpr size_t numHighScores = 20;

    uint16_t version = 2;
    uint16_t numberORooms = 0;
    uint32_t timeStamp = 1;
    std::array<uint32_t, numHighScores> hiScores {};
    std::array<uint16_t, numHighScores> hiLevel {};
    std::array<std::string, numHighScores> hiName {};
    std::array<std::string, numHighScores> hiRoom {};
    std::string pictFile;
    std::string nextFile;
    std::string firstFile;
    std::array<RoomData, numRooms> theRooms {};
};
