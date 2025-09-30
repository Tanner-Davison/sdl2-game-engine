#include "Rectangle.hpp"
#include <iostream>
#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

Rectangle::Rectangle(const SDL_Rect& _Rect) : Rect(_Rect) {};

void Rectangle::Render(SDL_Surface* Surface) const {
    auto [r, g, b, a]{isPointerHovering ? HoverColor : Color};
    SDL_FillRect(Surface, &Rect, SDL_MapRGB(Surface->format, r, g, b));
}
void Rectangle::HandleEvent(SDL_Event& E) {
    if (E.type == SDL_MOUSEMOTION) {
        bool wasPointerHovering{isPointerHovering};
        isPointerHovering = isWithinRect(E.motion.x, E.motion.y);
        if (!wasPointerHovering && isPointerHovering) {
            OnMouseEnter();
        } else if (wasPointerHovering && !isPointerHovering) {
            OnMouseExit();
        }
    } else if (E.type == SDL_WINDOWEVENT &&
               E.window.event == SDL_WINDOWEVENT_LEAVE) {
        if (isPointerHovering)
            OnMouseExit();
        isPointerHovering = false;
    } else if (E.type == SDL_MOUSEBUTTONDOWN) {
        if (isPointerHovering && E.button.button == SDL_BUTTON_LEFT) {
            OnLeftClick();
        }
    }
}

void Rectangle::OnMouseEnter() {}

void Rectangle::OnMouseExit() {}

void Rectangle::OnLeftClick() {}

bool Rectangle::isWithinRect(int x, int y) const {
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
    Color = NewColor;
};

void Rectangle::SetHoverColor(const SDL_Color& NewColor) {
    Color = NewColor;
};

SDL_Color Rectangle::GetHoverColor() const {
    return HoverColor;
}

SDL_Color Rectangle::GetColor() const {
    return this->Color;
};
