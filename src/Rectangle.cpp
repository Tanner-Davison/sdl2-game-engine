#include "Rectangle.hpp"
#include "DrawPrimitives.hpp"
#include <SDL3/SDL.h>
#include <algorithm>

Rectangle::Rectangle(const SDL_Rect& _Rect) : Rect(_Rect) {}

void Rectangle::Render(SDL_Renderer* renderer) const {
    const auto& col = isPointerHovering ? HoverColor : Color;
    if (CornerRadius > 0.0f) {
        RenderRounded(renderer, col);
    } else {
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
        SDL_FRect fr = {(float)Rect.x, (float)Rect.y, (float)Rect.w, (float)Rect.h};
        SDL_RenderFillRect(renderer, &fr);
    }
    if (isPointerHovering && OutlineThickness > 0.0f)
        RenderRoundedOutline(renderer);
}

void Rectangle::RenderRounded(SDL_Renderer* renderer, const SDL_Color& col) const {
    SDL_FRect fr = {(float)Rect.x, (float)Rect.y, (float)Rect.w, (float)Rect.h};
    DrawFilledRoundedRect(renderer, fr, col, CornerRadius);
}

void Rectangle::RenderRoundedOutline(SDL_Renderer* renderer) const {
    SDL_FRect fr = {(float)Rect.x, (float)Rect.y, (float)Rect.w, (float)Rect.h};
    DrawRoundedRectOutline(renderer, fr, OutlineColor, OutlineThickness, CornerRadius, OutlineGap);
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

void      Rectangle::SetColor(const SDL_Color& c)       { Color = c; }
void      Rectangle::SetHoverColor(const SDL_Color& c)   { HoverColor = c; }
void      Rectangle::SetCornerRadius(float radius)       { CornerRadius = std::max(0.0f, radius); }
float     Rectangle::GetCornerRadius() const             { return CornerRadius; }
SDL_Color Rectangle::GetColor()      const               { return Color; }
SDL_Color Rectangle::GetHoverColor() const               { return HoverColor; }

void Rectangle::SetHoverOutline(const SDL_Color& color, float thickness, float gap) {
    OutlineColor     = color;
    OutlineThickness = std::max(0.0f, thickness);
    OutlineGap       = std::max(0.0f, gap);
}
