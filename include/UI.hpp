#pragma once
#include "Rectangle.hpp"

#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif
#include <memory>
#include <vector>

class UI {
  public:
    UI();
    void Render(SDL_Surface* Surface) const;
    void HandleEvent(SDL_Event& E);

  private:
    Rectangle A{SDL_Rect{50, 50, 50, 50}};
    Rectangle B{SDL_Rect{150, 50, 50, 50}};
};
