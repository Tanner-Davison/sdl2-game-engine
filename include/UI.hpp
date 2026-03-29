#pragma once
#include <SDL3/SDL.h>
#include "Button.hpp"
#include "Rectangle.hpp"
#include "SettingsMenu.hpp"

class UI {
  public:
    void Render(SDL_Renderer* renderer) const;
    void HandleEvent(SDL_Event& E);

  private:
    Rectangle    A{{350, 50, 50, 50}};
    Rectangle    B{{50, 50, 50, 50}};
    Button       C{*this, {150, 50, 50, 50}};
    Button       SettingsButton{*this, {250, 50, 150, 50}};
    SettingsMenu Settings;
};
