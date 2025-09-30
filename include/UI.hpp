#pragma once
#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif
#include "Button.hpp"
#include "Rectangle.hpp"

class UI {
  public:
    void Render(SDL_Surface* Surface) const;
    void HandleEvent(SDL_Event& E);

  private:
    /// Of type SDL_Rect
    Rectangle A{{50, 50, 50, 50}};
    Rectangle B{{150, 50, 50, 50}};
    Button    C{*this, {250, 50, 50, 50}};
};
