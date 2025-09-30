#pragma once
#pragma once
#include "Rectangle.hpp"
#include <SDL.h>

class UI;

class Button : public Rectangle {
  public:
    Button(UI& UIManager, const SDL_Rect& Rect);

    UI& UIManager;
};
