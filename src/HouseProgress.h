#pragma once

#include "HouseData.h"
#include "ObjectKey.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

struct HouseProgress
{
    std::unordered_map<size_t, ConditionCode> roomConditionOverrides;
    std::unordered_map<ObjectKey, uint8_t, ObjectKeyHash> objectIsOnOverrides;
    std::unordered_map<ObjectKey, uint16_t, ObjectKeyHash> objectAmountOverrides;
    std::unordered_set<size_t> roomBonusAwarded;

    ConditionCode effectiveConditionCode(const size_t roomIndex, const ConditionCode loaded) const
    {
        const auto it = roomConditionOverrides.find(roomIndex);
        return it != roomConditionOverrides.end() ? it->second : loaded;
    }

    uint8_t effectiveIsOn(const size_t roomIndex, const size_t objectIndex, const uint8_t loaded) const
    {
        const auto it = objectIsOnOverrides.find({roomIndex, objectIndex});
        return it != objectIsOnOverrides.end() ? it->second : loaded;
    }

    uint16_t effectiveAmount(const size_t roomIndex, const size_t objectIndex, const uint16_t loaded) const
    {
        const auto it = objectAmountOverrides.find({roomIndex, objectIndex});
        return it != objectAmountOverrides.end() ? it->second : loaded;
    }
};
