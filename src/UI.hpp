#pragma once
#include "GreenRectangle.h"
#include "Rectangle.hpp"
#include <SDL2/SDL.h>
#include <memory>
#include <vector>

class UI {
  public:
    UI(int rowsize, int columnsize);
    UI();
    void Render(SDL_Surface* Surface) const;
    void HandleEvent(SDL_Event& E);

  private:
    std::vector<std::unique_ptr<Rectangle>> Rectangles{};
};
