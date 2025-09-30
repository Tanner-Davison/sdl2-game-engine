// Header.h
#pragma once
#include "Rectangle.hpp"
#include <SDL.h>

class Header {
  public:
    Header() {
        Background.SetColor({100, 100, 100, 255});
    }
    void Render(SDL_Surface* Surface) const {
        Background.Render(Surface);
    }

    void HandleEvent(SDL_Event& E) {
        Background.HandleEvent(E);
    }

  private:
    Rectangle Background{SDL_Rect{0, 0, 700, 50}};
};
