#include "HouseWriter.h"
#include <algorithm>
#include <cstdio>

bool HouseWriter::write(const HouseRec& house, const std::string_view& fileName)
{
    data_.clear();

    writeU16(house.version);
    writeU16(house.numberORooms);
    writeU32(house.timeStamp);

    for (const auto& score : house.hiScores)
    {
        writeU32(score);
    }
    for (const auto& level : house.hiLevel)
    {
        writeU16(level);
    }
    for (const auto& name : house.hiName)
    {
        writeString(name, 26);
    }
    for (const auto& roomName : house.hiRoom)
    {
        writeString(roomName, 26);
    }

    writeString(house.pictFile, 34);
    writeString(house.nextFile, 34);
    writeString(house.firstFile, 34);

    for (const auto& room : house.theRooms)
    {
        writeString(room.roomName, 26);
        writeU16(room.numberOObjects);
        writeU16(room.backPictID);
        for (const auto& tile : room.tileOrder)
        {
            writeU16(tile);
        }
        writeByte(room.leftOpen);
        writeByte(room.rightOpen);
        writeU16(static_cast<uint16_t>(room.animateKind));
        writeU16(room.animateNumber);
        writeU32(room.animateDelay);
        writeU16(static_cast<uint16_t>(room.conditionCode));
        for (const auto& object : room.theObjects)
        {
            writeU16(object.objectIs);
            writeRect(object.boundRect);
            writeU16(object.amount);
            writeU16(object.extra);
            writeByte(object.isOn);
            writeByte(0); // padding byte, mirrors skipBytes(1) in HouseReader
        }
    }

    auto* file = fopen(fileName.data(), "wb");
    if (!file)
    {
        return false;
    }
    fwrite(data_.data(), 1, data_.size(), file);
    fclose(file);
    return true;
}

void HouseWriter::writeByte(const uint8_t value)
{
    data_.push_back(value);
}

void HouseWriter::writeU16(const uint16_t value)
{
    data_.push_back(static_cast<uint8_t>(value >> 8));
    data_.push_back(static_cast<uint8_t>(value & 0xFF));
}

void HouseWriter::writeU32(const uint32_t value)
{
    data_.push_back(static_cast<uint8_t>(value >> 24));
    data_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    data_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    data_.push_back(static_cast<uint8_t>(value & 0xFF));
}

void HouseWriter::writeString(const std::string& str, const size_t maxSize)
{
    // Pascal string: first byte is length (capped at maxSize-1), then characters,
    // then zero-padding to fill exactly maxSize bytes total.
    const size_t length = std::min(str.size(), maxSize - 1);
    data_.push_back(static_cast<uint8_t>(length));
    for (size_t i = 0; i < length; ++i)
    {
        data_.push_back(static_cast<uint8_t>(str[i]));
    }
    for (size_t i = length + 1; i < maxSize; ++i)
    {
        data_.push_back(0);
    }
}

void HouseWriter::writeRect(const Rect& rect)
{
    writeU16(rect.top);
    writeU16(rect.left);
    writeU16(rect.bottom);
    writeU16(rect.right);
}
