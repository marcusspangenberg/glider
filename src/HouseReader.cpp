#include "HouseReader.h"
#include "Constants.h"
#include "ObjectType.h"
#include <cassert>
#include <cstdio>
#include <cstring>

namespace
{

Rect getEventRect(const ObjectData& objectData)
{
    switch (static_cast<ObjectType>(objectData.objectIs))
    {
    case ObjectType::floorVent:
    {
        const auto tempInt = (objectData.boundRect.right + objectData.boundRect.left) / 2;
        return {static_cast<uint16_t>(tempInt - 8),
            objectData.amount,
            static_cast<uint16_t>(tempInt + 8),
            constants::floorVert};
    }
    case ObjectType::ceilingVent:
    {
        const auto tempInt = (objectData.boundRect.right + objectData.boundRect.left) / 2;
        return {static_cast<uint16_t>(tempInt - 8),
            constants::ceilingVert,
            static_cast<uint16_t>(tempInt + 8),
            objectData.amount};
    }
    case ObjectType::ceilingDuct:
    {
        if (objectData.isOn)
        {
            const auto tempInt = (objectData.boundRect.right + objectData.boundRect.left) / 2;
            return {static_cast<uint16_t>(tempInt - 8),
                constants::ceilingVert,
                static_cast<uint16_t>(tempInt + 8),
                objectData.amount};
        }
        auto eventRect = objectData.boundRect;
        eventRect.bottom = eventRect.top + 8;
        return eventRect;
    }
    default:
        return objectData.boundRect;
    }
}

} // namespace

std::optional<HouseRec> HouseReader::read(const std::string_view& fileName)
{
    const auto houseFile = fopen(fileName.data(), "r");
    if (!houseFile)
    {
        return std::nullopt;
    }
    fseek(houseFile, 0, SEEK_END);
    size_ = ftell(houseFile);
    fseek(houseFile, 0, SEEK_SET);

    data_ = std::make_unique<uint8_t[]>(size_);
    [[maybe_unused]] const auto readBytes = fread(data_.get(), 1, size_, houseFile);
    fclose(houseFile);

    HouseRec house {};
    house.version = readU16();
    house.numberORooms = readU16();
    house.timeStamp = readU32();
    for (auto& hiScore : house.hiScores)
    {
        hiScore = readU32();
    }
    for (auto& hiLevel : house.hiLevel)
    {
        hiLevel = readU16();
    }
    for (auto& hiName : house.hiName)
    {
        hiName = readString(26);
    }
    for (auto& hiRoom : house.hiRoom)
    {
        hiRoom = readString(26);
    }

    house.pictFile = readString(34);
    house.nextFile = readString(34);
    house.firstFile = readString(34);

    for (auto& room : house.theRooms)
    {
        room.roomName = readString(26);
        room.numberOObjects = readU16();
        room.backPictID = readU16();
        for (auto& tileOrder : room.tileOrder)
        {
            tileOrder = readU16();
        }
        room.leftOpen = readByte();
        room.rightOpen = readByte();
        room.animateKind = static_cast<AnimateKind>(readU16());
        room.animateNumber = readU16();
        room.animateDelay = readU32();
        room.conditionCode = static_cast<ConditionCode>(readU16());

        for (auto& theObject : room.theObjects)
        {
            theObject.objectIs = readU16();
            theObject.boundRect = readRect();
            theObject.amount = readU16();
            theObject.extra = readU16();
            theObject.isOn = readByte();
            skipBytes(1);
        }

        for (size_t i = 0; i < RoomData::numObjects; ++i)
        {
            room.eventRects[i] = getEventRect(room.theObjects[i]);
        }
    }
    return house;
}

void HouseReader::skipBytes(const size_t value)
{
    index_ += value;
}

uint8_t HouseReader::readByte()
{
    assert(index_ < size_);
    const uint8_t result = data_[index_];
    ++index_;
    return result;
}

uint16_t HouseReader::readU16()
{
    assert(index_ + 1 < size_);
    const uint16_t result = static_cast<uint16_t>(data_[index_]) << 8 | static_cast<uint16_t>(data_[index_ + 1]);
    index_ += 2;
    return result;
}

uint32_t HouseReader::readU32()
{
    assert(index_ + 3 < size_);
    const uint32_t result = static_cast<uint32_t>(data_[index_]) << 24 | static_cast<uint32_t>(data_[index_ + 1]) << 16
        | static_cast<uint32_t>(data_[index_ + 2]) << 8 | static_cast<uint32_t>(data_[index_ + 3]);
    index_ += 4;
    return result;
}

std::string HouseReader::readString(const size_t maxSize)
{
    assert(maxSize <= 256);
    assert(index_ + maxSize < size_);

    std::array<char, 256> pascalString {};
    const size_t pascalStringLength = data_[index_];
    memcpy(pascalString.data(), &data_[index_ + 1], pascalStringLength);
    index_ += maxSize;

    return pascalString.data();
}

Point HouseReader::readPoint()
{
    assert(index_ + 3 < size_);
    Point point {};
    point.v = readU16();
    point.h = readU16();
    return point;
}

Rect HouseReader::readRect()
{
    Rect rect {};
    rect.top = readU16();
    rect.left = readU16();
    rect.bottom = readU16();
    rect.right = readU16();
    return rect;
}
