#include "Preferences.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <SDL2/SDL.h>
#include <string>

namespace
{

std::string prefFilePath()
{
    std::unique_ptr<char, decltype(&SDL_free)> dir(SDL_GetPrefPath("glider", "glider"), SDL_free);
    if (!dir)
    {
        return {};
    }
    return std::string(dir.get()) + "preferences.ini";
}

} // namespace

Preferences Preferences::load()
{
    Preferences prefs;
    const std::string path = prefFilePath();
    if (path.empty())
    {
        return prefs;
    }

    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "r");
    if (!rw)
    {
        return prefs;
    }

    char line[256];
    while (SDL_RWread(rw, line, 1, 0) == 0)
    {
        break;
    }

    const auto size = SDL_RWsize(rw);
    SDL_RWseek(rw, 0, RW_SEEK_SET);

    if (size <= 0)
    {
        SDL_RWclose(rw);
        return prefs;
    }

    std::string content(static_cast<size_t>(size), '\0');
    SDL_RWread(rw, content.data(), 1, static_cast<size_t>(size));
    SDL_RWclose(rw);

    size_t pos = 0;
    while (pos < content.size())
    {
        size_t end = content.find('\n', pos);
        if (end == std::string::npos)
        {
            end = content.size();
        }
        std::string line2 = content.substr(pos, end - pos);
        if (!line2.empty() && line2.back() == '\r')
        {
            line2.pop_back();
        }
        pos = end + 1;

        const auto eq = line2.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        auto key = line2.substr(0, eq);
        auto value = line2.substr(eq + 1);

        if (key == "airflow")
        {
            prefs.showAirflow = (value == "1");
        }
        else if (key == "music")
        {
            prefs.musicEnabled = (value != "0");
        }
        else if (key == "keyLeft")
        {
            const auto sc = std::stoi(value);
            if (sc > 0 && sc < SDL_NUM_SCANCODES)
            {
                prefs.keyLeft = static_cast<SDL_Scancode>(sc);
            }
        }
        else if (key == "keyRight")
        {
            const auto sc = std::stoi(value);
            if (sc > 0 && sc < SDL_NUM_SCANCODES)
            {
                prefs.keyRight = static_cast<SDL_Scancode>(sc);
            }
        }
        else if (key == "keyThrust")
        {
            const auto sc = std::stoi(value);
            if (sc > 0 && sc < SDL_NUM_SCANCODES)
            {
                prefs.keyThrust = static_cast<SDL_Scancode>(sc);
            }
        }
        else if (key == "keyFireBand")
        {
            const auto sc = std::stoi(value);
            if (sc > 0 && sc < SDL_NUM_SCANCODES)
            {
                prefs.keyFireBand = static_cast<SDL_Scancode>(sc);
            }
        }
    }

    return prefs;
}

void Preferences::save() const
{
    const auto path = prefFilePath();
    if (path.empty())
    {
        return;
    }

    auto* rw = SDL_RWFromFile(path.c_str(), "w");
    if (!rw)
    {
        return;
    }

    char buf[256];
    const auto length = std::snprintf(buf,
        sizeof(buf),
        "airflow=%d\nmusic=%d\nkeyLeft=%d\nkeyRight=%d\nkeyThrust=%d\nkeyFireBand=%d\n",
        showAirflow ? 1 : 0,
        musicEnabled ? 1 : 0,
        static_cast<int32_t>(keyLeft),
        static_cast<int32_t>(keyRight),
        static_cast<int32_t>(keyThrust),
        static_cast<int32_t>(keyFireBand));
    SDL_RWwrite(rw, buf, 1, static_cast<size_t>(length));
    SDL_RWclose(rw);
}
