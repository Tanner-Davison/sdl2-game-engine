#include "Rectangle.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>

Rectangle::Rectangle(const SDL_Rect& _Rect) : Rect(_Rect) {};

void Rectangle::Render(SDL_Surface* Surface) const {
    auto [r, g, b, a]{isMousePressed ? PressedColor : isPointerHovering ? HoverColor : Rect_Color};
    SDL_FillRect(Surface, &Rect, SDL_MapRGB(Surface->format, r, g, b));
}

/// Handle Events
void Rectangle::HandleEvent(SDL_Event& E) {
    if (E.type == SDL_MOUSEMOTION) {
        isPointerHovering = isWithinRect(E.motion.x, E.motion.y);
        if (isMousePressed && isPointerHovering) {
            cursorManager.setHandCursor();
            if (E.motion.x != 0 && E.motion.x != 700)
                Rect.x = E.motion.x - (Rect.w / 2);
            Rect.y = E.motion.y - (Rect.h / 2);
        }
    }
    if (E.type == SDL_MOUSEBUTTONDOWN) {
        isMousePressed = true;
    }
    if (E.type == SDL_MOUSEBUTTONUP) {
        isMousePressed = false;
        cursorManager.setDefaultCursor();
    };
}

/// Check if Mouse inside of Rect?
bool Rectangle::isWithinRect(int x, int y) {
    if (x < Rect.x) /// Too far left
        return false;

    if (x > Rect.x + Rect.w) /// Too far right
        return false;

    if (y < Rect.y) /// Too far up
        return false;

    if (y > Rect.y + Rect.h) /// Too far Down
        return false;

    return true;
};

void Rectangle::SetColor(const SDL_Color& NewColor) {
    Rect_Color = NewColor;
};

void Rectangle::SetHoverColor(const SDL_Color& NewColor) {
    Rect_Color = NewColor;
};

SDL_Color Rectangle::GetHoverColor() const {
    return HoverColor;
}

SDL_Color Rectangle::GetColor() const {
    return this->Rect_Color;
};
