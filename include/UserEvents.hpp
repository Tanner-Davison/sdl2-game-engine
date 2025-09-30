#pragma once
#include <SDL.h>
#include <SDL_events.h>

namespace UserEvents {
const inline Uint32 OPEN_SETTINGS{SDL_RegisterEvents(1)};
const inline Uint32 CLOSE_SETTINGS{SDL_RegisterEvents(1)};

enum class SettingsPage { GAMEPLAY, GRAPHICS, AUDIO };

struct SettingsConfig {
    SettingsPage Page;
    int          x;
    int          y;
};
} // namespace UserEvents
