#pragma once

#include "HouseData.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

class HouseReader
{
public:
    HouseReader() = default;
    std::optional<HouseRec> read(const std::string_view& fileName);

private:
    size_t index_ = 0;
    size_t size_ = 0;
    std::unique_ptr<uint8_t[]> data_;

    void skipBytes(size_t value);
    uint8_t readByte();
    uint16_t readU16();
    uint32_t readU32();
    std::string readString(size_t maxSize);
    Point readPoint();
    Rect readRect();
};
