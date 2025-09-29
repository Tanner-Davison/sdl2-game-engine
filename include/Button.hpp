#pragma once
#include "Rectangle.hpp"
#include <SDL2/SDL.h>
#include <iostream>

class Button : public Rectangle {
  public:
    Button(const SDL_Rect& Rect) : Rectangle{Rect} {
        SetColor({255, 165, 0, 255}); // Orange
    }

    void Render(SDL_Surface* Surface) const override {
        Rectangle::Render(Surface);
        // Render the rest of the button...
    }
    void onMouseEnter() override {
        std::cout << "Hello Mouse" << std::endl;
    }
    void OnMouseExit() override {
        std::cout << "Mouse Left!" << std::endl;
    }
};
