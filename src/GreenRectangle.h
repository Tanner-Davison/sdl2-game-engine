#pragma once
#include "Rectangle.hpp"
#include <SDL2/SDL.h>

class GreenRectangle : public Rectangle {
  public:
    GreenRectangle(const SDL_Rect& Rect) : Rectangle{Rect} {
        SetColor({0, 255, 0, 255});
    };
};
