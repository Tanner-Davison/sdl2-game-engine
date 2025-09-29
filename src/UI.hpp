#pragma once
#include "GreenRectangle.h"
#include "Rectangle.hpp"
#include <SDL2/SDL.h>
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
