#pragma once
#include "Rectangle.hpp"
#include <SDL2/SDL.h>
#include <iostream>

class UI;

class Button : public Rectangle {
  public:
    Button(UI& UIManager, const SDL_Rect& Rect);
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

    void OnLeftClick() override;

    void OnRightClick() override {
        std::cout << "[Right Clicked]-> Rectangle::Button" << std::endl;
    }

  private:
    UI& UIManager;
};
