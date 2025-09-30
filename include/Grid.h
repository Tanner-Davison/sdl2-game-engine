#pragma once
#include "Rectangle.hpp"
#include <SDL.h>
#include <memory>
#include <vector>

class Grid {
  public:
    Grid() {
        int VerticlePosition{65};
        int RowCount{3}, ColCount{12};

        Rectangles.reserve(RowCount * ColCount);
        for (int Row{0}; Row < RowCount; ++Row) {
            for (int Col{0}; Col < ColCount; ++Col) {
                Rectangles.emplace_back(std::make_unique<Rectangle>(
                    SDL_Rect{60 * Col, 60 * Row + VerticlePosition, 50, 50}));
            };
        }
    }
    void Render(SDL_Surface* Surface) const {
        for (auto& Rectangle : Rectangles) {
            Rectangle->Render(Surface);
        }
    }
    void HandleEvent(SDL_Event& E) {
        for (auto& Rectangle : Rectangles) {
            Rectangle->HandleEvent(E);
        }
    }

  private:
    std::vector<std::unique_ptr<Rectangle>> Rectangles;
};
