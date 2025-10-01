#pragma once
#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif
#include "Button.hpp"
#include "Rectangle.hpp"
#include "SettingsMenu.hpp"

class UI {
  public:
    void Render(SDL_Surface* Surface) const;
    void HandleEvent(SDL_Event& E);

  private:
    /// Of type SDL_Rect
    Rectangle    A{{350, 50, 50, 50}};
    Rectangle    B{{50, 50, 50, 50}};
    Button       C{*this, {150, 50, 50, 50}};
    Button       SettingsButton{*this, {250, 50, 150, 50}};
    SettingsMenu Settings;
};
