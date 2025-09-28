#include "Rectangle.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>

Rectangle::Rectangle(const SDL_Rect& _Rect) : Rect(_Rect) {};

void Rectangle::Render(SDL_Surface* Surface) const {
    auto [r, g, b, a]{isLocked                              ? Rect_Color
                      : isMousePressed && isPointerHovering ? PressedColor
                      : isPointerHovering                   ? HoverColor
                                                            : Rect_Color};
    SDL_FillRect(Surface, &Rect, SDL_MapRGB(Surface->format, r, g, b));
}

/// Handle Events
void Rectangle::HandleEvent(SDL_Event& E) {
    if (E.type == SDL_MOUSEMOTION) {
        isPointerHovering = isWithinRect(E.motion.x, E.motion.y);

        if (isMousePressed && isPointerHovering && !isLocked) {
            cursorManager.setGrabCursor();
            if (E.motion.x != 0 && E.motion.x != 700)
                Rect.x = E.motion.x - (Rect.w / 2);
            Rect.y = E.motion.y - (Rect.h / 2);
        } else if (isPointerHovering && !isMousePressed && !isLocked) {
            cursorManager.setHandCursor();
        } else {
            cursorManager.setDefaultCursor();
        }
    }
    if (E.type == SDL_MOUSEBUTTONDOWN) {
        if (E.button.button == SDL_BUTTON_LEFT) {
            isMousePressed = true;
        } else if (E.button.button == SDL_BUTTON_RIGHT) {
            if (E.button.state == SDL_PRESSED && E.button.clicks >= 2) {
                isLocked = false;
            } else if (E.button.state == SDL_PRESSED && E.button.clicks <= 1) {
                isLocked = true;
            }
        }
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
