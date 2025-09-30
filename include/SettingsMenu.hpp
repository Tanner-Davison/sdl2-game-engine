#pragma once
#include "UserEvents.hpp"
#include <SDL.h>
#include <SDL_pixels.h>
#include <iostream>

class SettingsMenu {
  public:
    void HandleEvent(SDL_Event& E) {
        if (E.type == UserEvents::OPEN_SETTINGS ||
            E.type == UserEvents::CLOSE_SETTINGS) {
            HandleUserEvent(E.user);
        }
    }

    void HandleUserEvent(SDL_UserEvent& E) {
        using namespace UserEvents;
        if (E.type == OPEN_SETTINGS) {
            isOpen = true;

            std::cout << "That's a user  event\n";
            auto* Config{static_cast<SettingsConfig*>(E.data1)};
            Rect.x = Config->x;
            Rect.y = Config->y;
            if (Config->Page == SettingsPage::GAMEPLAY) {
                std::cout << "Page: Gameplay Settings\n";
            }
        } else if (E.type == CLOSE_SETTINGS) {
            isOpen = false;
        }
    }
    void Render(SDL_Surface* Surface) const {
        if (!isOpen)
            return;
        SDL_FillRect(Surface,
                     &Rect,
                     SDL_MapRGB(Surface->format, Color.r, Color.g, Color.b));
    }

  private:
    bool      isOpen{false};
    SDL_Rect  Rect{100, 50, 200, 200};
    SDL_Color Color{150, 150, 150, 255};
};
