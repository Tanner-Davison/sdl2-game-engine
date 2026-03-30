#pragma once
#include <SDL3/SDL.h>

// Filled rounded rectangle with 1px anti-aliased feathering.
// Single SDL_RenderGeometry call. radius is clamped to half the smallest dimension.
void DrawFilledRoundedRect(SDL_Renderer* renderer,
                           const SDL_FRect& rect,
                           const SDL_Color& color,
                           float radius);

// Rounded rectangle stroke with 1px AA feathering on the outer edge.
// `gap` is the space between the rect boundary and the stroke inner edge.
// Single SDL_RenderGeometry call.
void DrawRoundedRectOutline(SDL_Renderer* renderer,
                            const SDL_FRect& rect,
                            const SDL_Color& color,
                            float thickness,
                            float radius,
                            float gap = 0.0f);
