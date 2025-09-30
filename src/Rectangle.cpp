#include "Rectangle.hpp"
#include <iostream>
#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif
bool Rectangle::isAnyRectangleMoving = false;

Rectangle::Rectangle(const SDL_Rect& _Rect) : Rect(_Rect) {};

void Rectangle::Render(SDL_Surface* Surface) const {
    auto [r, g, b, a]{isLocked                              ? Rect_Color
                      : isMousePressed && isPointerHovering ? PressedColor
                      : isPointerHovering                   ? HoverColor
                                                            : Rect_Color};
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
        if (canStartMoving()) {
            startMoving();
        } else if (isMovingRectangle) {
            if (E.motion.x != 0 && E.motion.x != 700)
                Rect.x = E.motion.x - (Rect.w / 2);
            Rect.y = E.motion.y - (Rect.h / 2);
        } else if (isPointerHovering && !isMousePressed && !isLocked) {
            cursorManager.setHandCursor();
        } else if (wasPointerHovering && !isPointerHovering &&
                   !isAnyRectangleMoving) {
            cursorManager.setDefaultCursor();
        }
    } else if (E.type == SDL_WINDOWEVENT &&
               E.window.event == SDL_WINDOWEVENT_LEAVE) {
        if (isPointerHovering)
            OnMouseExit();
        isPointerHovering = false;
    } else if (E.type == SDL_MOUSEBUTTONDOWN) {
        if (E.button.button == SDL_BUTTON_LEFT && isPointerHovering) {
            isMousePressed = true;
            OnLeftClick();
        } else if (E.button.button == SDL_BUTTON_RIGHT && isPointerHovering) {
            isLocked = (E.button.clicks <= 1); // Locks Rectangle
            OnRightClick();
        }
    } else if (E.type == SDL_MOUSEBUTTONUP) {
        isMousePressed = false;
        if (isMovingRectangle) {
            isMovingRectangle    = false;
            isAnyRectangleMoving = false;
        }
        cursorManager.setDefaultCursor();
    }
}

void Rectangle::OnMouseEnter() {}

void Rectangle::OnMouseExit() {}

void Rectangle::OnLeftClick() {}

void Rectangle::OnRightClick() {}

bool Rectangle::canStartMoving() const {
    return isMousePressed && isPointerHovering && !isLocked &&
           !isAnyRectangleMoving;
}
void Rectangle::startMoving() {
    isMovingRectangle    = true;
    isAnyRectangleMoving = true;
    cursorManager.setGrabCursor();
}

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
