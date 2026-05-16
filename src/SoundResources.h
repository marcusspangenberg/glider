#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>
#include <unordered_map>

struct Mix_Chunk;

class SoundResources
{
public:
    enum class Sound : int32_t
    {
        pop = 1,
        beamIn = 2,
        toastJump = 3,
        toastDrop = 4,
        greaseFall = 5,
        drip = 6,
        bounce = 7,
        hey = 8,
        teaKettle = 9,
        blowerOn = 11,
        energize = 10,
        lightning = 12,
        extra = 13,
        guitar = 14,
        zap = 15,
        shredder = 16,
        lightning2 = 17,
        lightsOn = 18,
        push = 19,
        yow = 20,
        goodMove = 21,
        musicBite = 22,
        aww = 23,
        getBand = 24,
        crunch = 25,
        fireBand = 26,
        tick = 27,
        clock = 28,
        bass = 30,
    };

    enum class Channel : int32_t
    {
        soundEffect = 0,
        music = 1,
    };

    explicit SoundResources(std::string_view directoryName);
    ~SoundResources();

    void play(Sound sound, Channel = Channel::soundEffect) const;
    void playTimed(Sound sound, std::chrono::milliseconds duration) const;

private:
    static constexpr int32_t effectsGroup = 1;
    std::unordered_map<Sound, Mix_Chunk*> sounds_;
    bool opened_ = false;

    static int32_t getEffectChannelIndex();
};
