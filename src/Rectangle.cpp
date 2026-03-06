#include "Rectangle.hpp"
#include <SDL3/SDL.h>

Rectangle::Rectangle(const SDL_Rect& _Rect) : Rect(_Rect) {}

void Rectangle::Render(SDL_Renderer* renderer) const {
    auto [r, g, b, a] = isPointerHovering ? HoverColor : Color;
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_FRect fr = {(float)Rect.x, (float)Rect.y, (float)Rect.w, (float)Rect.h};
    SDL_RenderFillRect(renderer, &fr);
}

void Rectangle::HandleEvent(SDL_Event& E) {
    if (E.type == SDL_EVENT_MOUSE_MOTION) {
        bool was = isPointerHovering;
        isPointerHovering = isWithinRect((int)E.motion.x, (int)E.motion.y);
        if (!was && isPointerHovering)  OnMouseEnter();
        else if (was && !isPointerHovering) OnMouseExit();
    } else if (E.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
        if (isPointerHovering) OnMouseExit();
        isPointerHovering = false;
    } else if (E.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (isPointerHovering && E.button.button == SDL_BUTTON_LEFT)
            OnLeftClick();
    }
}

void Rectangle::OnMouseEnter() {}
void Rectangle::OnMouseExit()  {}
void Rectangle::OnLeftClick()  {}

bool Rectangle::isWithinRect(int x, int y) const {
    return x >= Rect.x && x <= Rect.x + Rect.w && y >= Rect.y && y <= Rect.y + Rect.h;
}

void      Rectangle::SetColor(const SDL_Color& c)      { Color = c; }
void      Rectangle::SetHoverColor(const SDL_Color& c) { HoverColor = c; }
SDL_Color Rectangle::GetColor()      const             { return Color; }
SDL_Color Rectangle::GetHoverColor() const             { return HoverColor; }
