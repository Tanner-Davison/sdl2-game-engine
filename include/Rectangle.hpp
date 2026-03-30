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
    void      SetCornerRadius(float radius);
    float     GetCornerRadius() const;
    void      SetHoverOutline(const SDL_Color& color, float thickness, float gap = 2.0f);

    virtual ~Rectangle() = default;

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
    virtual void OnLeftClick();

  protected:
    SDL_Rect  Rect;
    SDL_Color Color{255, 0, 0, 255};
    SDL_Color HoverColor{128, 128, 128, 0};
    float     CornerRadius{0.0f};
    SDL_Color OutlineColor{0, 0, 0, 0};
    float     OutlineThickness{0.0f};
    float     OutlineGap{2.0f};

  private:
    bool isPointerHovering{false};
    bool isWithinRect(int x, int y) const;
    void RenderRounded(SDL_Renderer* renderer, const SDL_Color& color) const;
    void RenderRoundedOutline(SDL_Renderer* renderer) const;
};
