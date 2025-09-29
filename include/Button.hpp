#pragma once
#include "Rectangle.hpp"
#include <SDL2/SDL.h>
#include <iostream>

class Button : public Rectangle {
  public:
    Button(const SDL_Rect& Rect) : Rectangle{Rect} {
        SetColor({255, 165, 0, 255}); // Orange

        std::cout << "Button constructed at " << this << std::endl;
    }

    void Render(SDL_Surface* Surface) const override {
        Rectangle::Render(Surface);
        // Render the rest of the button...
    }
    void OnMouseEnter() override {
        std::cout << "[Hovering]-> Rectangle::Button" << std::endl;
    }
    void OnMouseExit() override {
        std::cout << "[Mouse Left]-> Rectangle::Button " << std::endl;
    }
    void OnLeftClick() override {
        std::cout << "[Left Clicked]-> Rectangle::Button" << std::endl;
    };
    void OnRightClick() override {
        std::cout << "[Right Clicked]-> Rectangle::Button" << std::endl;
    }
};
