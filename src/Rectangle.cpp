#include "Rectangle.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>

Rectangle::Rectangle(const SDL_Rect& _Rect) : Rect(_Rect) {};

void Rectangle::Render(SDL_Surface* Surface) const {
    auto [r, g, b, a]{isPointerHovering ? HoverColor : Color};
    SDL_FillRect(Surface, &Rect, SDL_MapRGB(Surface->format, r, g, b));
}

/// Handle Events
void Rectangle::HandleEvent(SDL_Event& E) {
    if (E.type == SDL_MOUSEMOTION) {
        isPointerHovering = isWithinRect(E.motion.x, E.motion.y);
    }
};

/// Check if Mouse inside of Rect?
bool Rectangle::isWithinRect(int x, int y) {
    // too far left
    if (x < Rect.x)
        return false;

    /// too far right
    if (x > Rect.x + Rect.w)
        return false;

    /// too far up
    if (y < Rect.y)
        return false;

    /// Too far Down
    if (y > Rect.y + Rect.h)
        return false;

    return true;
};
/// Set Color
void Rectangle::SetColor(const SDL_Color& NewColor) {
    Color = NewColor;
};

void Rectangle::SetHoverColor(const SDL_Color& NewColor) {
    Color = NewColor;
};
SDL_Color Rectangle::GetHoverColor() const {
    return HoverColor;
}

/// Get SDL_Color Color
SDL_Color Rectangle::GetColor() const {
    return this->Color;
};
