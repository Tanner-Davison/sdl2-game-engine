#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>

class Rectangle {
  public:
    Rectangle(const SDL_Rect& _Rect);

    void Render(SDL_Surface* Surface) const;
    void HandleEvent(SDL_Event& E);

    void SetColor(const SDL_Color& NewColor);
    void SetHoverColor(const SDL_Color& NewColor);

    SDL_Color GetColor() const;
    SDL_Color GetHoverColor() const;

  private:
    SDL_Rect  Rect;
    SDL_Color Color{255, 0, 0, 255};
    SDL_Color HoverColor{0, 0, 255, 255};

    bool isPointerHovering{false};
    bool isWithinRect(int x, int y);
};
