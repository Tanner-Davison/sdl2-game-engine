#include "Rectangle.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

static constexpr int   kArcSegments = 8;
static constexpr int   kPerimPts    = 4 * (kArcSegments + 1);
static constexpr int   kVertCount   = 1 + 2 * kPerimPts;
static constexpr int   kIdxCount    = kPerimPts * 9;
static constexpr float kFeather     = 1.0f;
static constexpr float kPi          = 3.14159265358979323846f;
static constexpr float kHalfPi      = kPi * 0.5f;

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
}

void Rectangle::RenderRounded(SDL_Renderer* renderer, const SDL_Color& col) const {
    const float x = (float)Rect.x;
    const float y = (float)Rect.y;
    const float w = (float)Rect.w;
    const float h = (float)Rect.h;
    const float r = std::min(CornerRadius, std::min(w, h) * 0.5f);

    const SDL_FColor fill = {col.r / 255.0f, col.g / 255.0f,
                             col.b / 255.0f, col.a / 255.0f};
    const SDL_FColor edge = {fill.r, fill.g, fill.b, 0.0f};

    struct Arc { float cx, cy, startAngle; };
    const Arc arcs[4] = {
        {x + r,     y + r,     kPi},
        {x + w - r, y + r,     kPi * 1.5f},
        {x + w - r, y + h - r, 0.0f},
        {x + r,     y + h - r, kHalfPi},
    };

    SDL_Vertex verts[kVertCount];
    int        indices[kIdxCount];

    verts[0] = {{x + w * 0.5f, y + h * 0.5f}, fill, {0, 0}};

    int vi = 0;
    for (const auto& [cx, cy, a0] : arcs) {
        for (int s = 0; s <= kArcSegments; ++s, ++vi) {
            float angle = a0 + s * kHalfPi / kArcSegments;
            float ca = std::cos(angle), sa = std::sin(angle);
            verts[1 + vi]             = {{cx + r * ca,              cy + r * sa},              fill, {0, 0}};
            verts[1 + kPerimPts + vi] = {{cx + (r + kFeather) * ca, cy + (r + kFeather) * sa}, edge, {0, 0}};
        }
    }

    int ti = 0;
    for (int i = 0; i < kPerimPts; ++i) {
        int nxt  = (i + 1) % kPerimPts;
        int in0  = 1 + i,              in1  = 1 + nxt;
        int out0 = 1 + kPerimPts + i,  out1 = 1 + kPerimPts + nxt;

        indices[ti++] = 0;    indices[ti++] = in0;  indices[ti++] = in1;
        indices[ti++] = in0;  indices[ti++] = out0; indices[ti++] = in1;
        indices[ti++] = in1;  indices[ti++] = out0; indices[ti++] = out1;
    }

    SDL_RenderGeometry(renderer, nullptr, verts, kVertCount, indices, kIdxCount);
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
void      Rectangle::SetHoverColor(const SDL_Color& c)  { HoverColor = c; }
void      Rectangle::SetCornerRadius(float radius)      { CornerRadius = std::max(0.0f, radius); }
float     Rectangle::GetCornerRadius() const            { return CornerRadius; }
SDL_Color Rectangle::GetColor()      const              { return Color; }
SDL_Color Rectangle::GetHoverColor() const              { return HoverColor; }
