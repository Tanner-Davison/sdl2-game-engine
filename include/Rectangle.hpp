#pragma once
#include <SDL3/SDL.h>

class Rectangle {
  public:
    Rectangle(const SDL_Rect& _Rect);
    virtual void Render(SDL_Renderer* renderer) const;

    void HandleEvent(SDL_Event& E);

    void      SetColor(const SDL_Color& NewColor);
    SDL_Color GetColor() const;
    void      SetHoverColor(const SDL_Color& NewColor);
    SDL_Color GetHoverColor() const;

    virtual ~Rectangle() = default;

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
    virtual void OnLeftClick();

  protected:
    SDL_Rect Rect;
    SDL_Color Color{255, 0, 0, 255};
    SDL_Color HoverColor{128, 128, 128, 0};

  private:
    bool isPointerHovering{false};
    bool isWithinRect(int x, int y) const;
};
