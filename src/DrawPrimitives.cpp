#include "DrawPrimitives.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

static constexpr int   kSegments  = 8;
static constexpr int   kPerimeter = 4 * (kSegments + 1);
static constexpr float kFeather   = 1.0f;
static constexpr float kPi        = std::numbers::pi_v<float>;
static constexpr float kHalfPi    = kPi * 0.5f;

static SDL_FColor ToFColor(const SDL_Color& c) {
    return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
}

static void BuildRing(SDL_FPoint out[],
                      float x, float y, float w, float h,
                      float cornerR, float ringR) {
    const struct { float cx, cy, a0; } arcs[4] = {
        {x + cornerR,     y + cornerR,     kPi},
        {x + w - cornerR, y + cornerR,     kPi * 1.5f},
        {x + w - cornerR, y + h - cornerR, 0.0f},
        {x + cornerR,     y + h - cornerR, kHalfPi},
    };
    int i = 0;
    for (const auto& [cx, cy, a0] : arcs)
        for (int s = 0; s <= kSegments; ++s, ++i) {
            float a = a0 + s * kHalfPi / kSegments;
            out[i]  = {cx + ringR * std::cos(a), cy + ringR * std::sin(a)};
        }
}

static void EmitStrip(int indices[], int& ti, int kPts,
                      int baseA, int baseB) {
    for (int i = 0; i < kPts; ++i) {
        int n  = (i + 1) % kPts;
        int a0 = baseA + i, a1 = baseA + n;
        int b0 = baseB + i, b1 = baseB + n;
        indices[ti++] = a0; indices[ti++] = b0; indices[ti++] = a1;
        indices[ti++] = a1; indices[ti++] = b0; indices[ti++] = b1;
    }
}

// ---------------------------------------------------------------------------

void DrawFilledRoundedRect(SDL_Renderer* renderer,
                           const SDL_FRect& rect,
                           const SDL_Color& color,
                           float radius) {
    const float x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    const float r = std::min(radius, std::min(w, h) * 0.5f);

    SDL_FPoint inner[kPerimeter], outer[kPerimeter];
    BuildRing(inner, x, y, w, h, r, r);
    BuildRing(outer, x, y, w, h, r, r + kFeather);

    const SDL_FColor fill = ToFColor(color);
    const SDL_FColor edge = {fill.r, fill.g, fill.b, 0.0f};

    constexpr int V = 1 + 2 * kPerimeter;
    constexpr int I = kPerimeter * 3 * 3;

    SDL_Vertex verts[V];
    int        indices[I];

    verts[0] = {{x + w * 0.5f, y + h * 0.5f}, fill, {0, 0}};
    for (int i = 0; i < kPerimeter; ++i) {
        verts[1 + i]              = {inner[i], fill, {0, 0}};
        verts[1 + kPerimeter + i] = {outer[i], edge, {0, 0}};
    }

    int ti = 0;
    for (int i = 0; i < kPerimeter; ++i) {
        int n = (i + 1) % kPerimeter;
        indices[ti++] = 0;
        indices[ti++] = 1 + i;
        indices[ti++] = 1 + n;
    }
    EmitStrip(indices, ti, kPerimeter, 1, 1 + kPerimeter);

    SDL_RenderGeometry(renderer, nullptr, verts, V, indices, I);
}

void DrawRoundedRectOutline(SDL_Renderer* renderer,
                            const SDL_FRect& rect,
                            const SDL_Color& color,
                            float thickness,
                            float radius,
                            float gap) {
    const float x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    const float r = std::min(radius, std::min(w, h) * 0.5f);

    SDL_FPoint ring0[kPerimeter], ring1[kPerimeter], ring2[kPerimeter];
    BuildRing(ring0, x, y, w, h, r, r + gap);
    BuildRing(ring1, x, y, w, h, r, r + gap + thickness);
    BuildRing(ring2, x, y, w, h, r, r + gap + thickness + kFeather);

    const SDL_FColor solid = ToFColor(color);
    const SDL_FColor clear = {solid.r, solid.g, solid.b, 0.0f};

    constexpr int V = 3 * kPerimeter;
    constexpr int I = 2 * kPerimeter * 6;

    SDL_Vertex verts[V];
    int        indices[I];

    for (int i = 0; i < kPerimeter; ++i) {
        verts[i]                  = {ring0[i], solid, {0, 0}};
        verts[kPerimeter + i]     = {ring1[i], solid, {0, 0}};
        verts[2 * kPerimeter + i] = {ring2[i], clear, {0, 0}};
    }

    int ti = 0;
    EmitStrip(indices, ti, kPerimeter, 0,         kPerimeter);
    EmitStrip(indices, ti, kPerimeter, kPerimeter, 2 * kPerimeter);

    SDL_RenderGeometry(renderer, nullptr, verts, V, indices, I);
}
