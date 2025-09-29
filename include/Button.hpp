#pragma once
#include "Button.hpp"
#include "Rectangle.hpp"
#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

class Button : public Rectangle {
  public:
    Button(const SDL_Rect& R);
    void Render(SDL_Surface* Surface) const;
    void HandleEvent(SDL_Event& E);

  private:
    Rectangle Rect;
};
