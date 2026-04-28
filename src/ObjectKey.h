#pragma once

#include <cstddef>
#include <functional>

struct ObjectKey
{
    size_t roomIndex;
    size_t objectIndex;

    bool operator==(const ObjectKey& other) const
    {
        return roomIndex == other.roomIndex && objectIndex == other.objectIndex;
    }
};

struct ObjectKeyHash
{
    size_t operator()(const ObjectKey& k) const
    {
        return std::hash<size_t>()(k.roomIndex) ^ (std::hash<size_t>()(k.objectIndex) << 16);
    }
};
