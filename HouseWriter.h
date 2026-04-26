#pragma once

#include "HouseData.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class HouseWriter
{
public:
    bool write(const HouseRec& house, const std::string_view& fileName);

private:
    std::vector<uint8_t> data_;

    void writeByte(uint8_t value);
    void writeU16(uint16_t value);
    void writeU32(uint32_t value);
    void writeString(const std::string& str, size_t maxSize);
    void writeRect(const Rect& rect);
};
