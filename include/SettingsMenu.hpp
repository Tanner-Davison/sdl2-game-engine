#pragma once
#include "Button.hpp"
#include "UserEvents.hpp"
#include <SDL3/SDL.h>
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

            auto* Instigator{static_cast<Button*>(E.data1)};

            // std::cout << "I was Opened From a Button: "
            //           << Instigator->GetLocation() << "\n";

            Rect.x = Instigator->GetConfig().x;
            Rect.y = Instigator->GetConfig().y;

            if (Instigator->GetConfig().Page == SettingsPage::GAMEPLAY) {
                std::cout << "Page: Gameplay Settings\n";
            }
        } else if (E.type == CLOSE_SETTINGS) {
            isOpen = false;
        }
    }
    void Render(SDL_Renderer* renderer) const {
        if (!isOpen) return;
        SDL_SetRenderDrawColor(renderer, Color.r, Color.g, Color.b, Color.a);
        SDL_FRect fr = {(float)Rect.x, (float)Rect.y, (float)Rect.w, (float)Rect.h};
        SDL_RenderFillRect(renderer, &fr);
    }

  private:
    bool      isOpen{false};
    SDL_Rect  Rect{50, 100, 200, 200};
    SDL_Color Color{150, 150, 150, 255};
};
