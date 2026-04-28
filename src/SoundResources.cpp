#include "SoundResources.h"

#include <cstdio>
#include <ranges>
#include <SDL2/SDL.h>
#include <SDL_mixer.h>
#include <string>
#include <unordered_map>

namespace
{

const std::unordered_map<SoundResources::Sound, std::string_view> soundFiles = {
    {SoundResources::Sound::pop, "pop.wav"},
    {SoundResources::Sound::beamIn, "beamin.wav"},
    {SoundResources::Sound::toastJump, "toastjump.wav"},
    {SoundResources::Sound::toastDrop, "toastdrop.wav"},
    {SoundResources::Sound::greaseFall, "greasefall.wav"},
    {SoundResources::Sound::drip, "drip.wav"},
    {SoundResources::Sound::bounce, "bounce.wav"},
    {SoundResources::Sound::hey, "hey.wav"},
    {SoundResources::Sound::teaKettle, "teakettle.wav"},
    {SoundResources::Sound::energize, "energize.wav"},
    {SoundResources::Sound::blowerOn, "bloweron.wav"},
    {SoundResources::Sound::lightning, "lightning.wav"},
    {SoundResources::Sound::extra, "extra.wav"},
    {SoundResources::Sound::guitar, "guitar.wav"},
    {SoundResources::Sound::zap, "zap.wav"},
    {SoundResources::Sound::shredder, "shredder.wav"},
    {SoundResources::Sound::lightning2, "lightning2.wav"},
    {SoundResources::Sound::lightsOn, "lightson.wav"},
    {SoundResources::Sound::push, "push.wav"},
    {SoundResources::Sound::yow, "yow.wav"},
    {SoundResources::Sound::goodMove, "goodmove.wav"},
    {SoundResources::Sound::musicBite, "musicbite.wav"},
    {SoundResources::Sound::aww, "aww.wav"},
    {SoundResources::Sound::getBand, "getband.wav"},
    {SoundResources::Sound::crunch, "crunch.wav"},
    {SoundResources::Sound::fireBand, "fireband.wav"},
    {SoundResources::Sound::tick, "tick.wav"},
    {SoundResources::Sound::clock, "clock.wav"},
    {SoundResources::Sound::bass, "bass.wav"},
};

} // namespace

SoundResources::SoundResources(std::string_view directoryName)
{
    int32_t frequency = 0;
    uint16_t format = 0;
    int32_t channels = 0;
    if (Mix_QuerySpec(&frequency, &format, &channels) == 0)
    {
        return;
    }
    for (const auto& [soundId, fileName] : soundFiles)
    {
        std::string path;
        path.reserve(directoryName.size() + fileName.size());
        path.append(directoryName);
        path.append(fileName);
        if (auto* chunk = Mix_LoadWAV(path.c_str()))
        {
            sounds_.emplace(soundId, chunk);
        }
        else
        {
            std::fprintf(stderr, "SoundResources: failed to load %s: %s\n", path.c_str(), Mix_GetError());
        }
    }
}

SoundResources::~SoundResources()
{
    for (const auto& chunk : sounds_ | std::views::values)
    {
        Mix_FreeChunk(chunk);
    }
}

void SoundResources::play(const Sound sound) const
{
    if (const auto it = sounds_.find(sound); it != sounds_.end())
    {
        Mix_PlayChannel(-1, it->second, 0);
    }
}

void SoundResources::playTimed(const Sound sound, const std::chrono::milliseconds duration) const
{
    if (const auto it = sounds_.find(sound); it != sounds_.end())
    {
        Mix_PlayChannelTimed(-1, it->second, -1, static_cast<int32_t>(duration.count()));
    }
}
