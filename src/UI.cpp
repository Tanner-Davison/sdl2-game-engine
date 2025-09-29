#include "UI.hpp"
UI::UI(int rowsize, int columnsize) {
    int RowCount{rowsize}, ColCount{columnsize};
    Rectangles.reserve(RowCount * ColCount);
    for (int Row{0}; Row < RowCount; ++Row) {
        for (int Col{0}; Col < ColCount; ++Col) {
            bool useGreen{(Row + Col) % 2 == 0};
            Rectangles.emplace_back(
                useGreen ? std::make_unique<GreenRectangle>(SDL_Rect{60 * Col, 60 * Row, 50, 50})
                         : std::make_unique<Rectangle>(SDL_Rect{60 * Col, 60 * Row, 50, 50}));
        }
    }
}

UI::UI() {
    int RowCount{5}, ColCount{12};
    Rectangles.reserve(RowCount * ColCount);
    for (int Row{0}; Row < RowCount; ++Row) {
        for (int Col{0}; Col < ColCount; ++Col) {
            bool useGreen{(Row + Col) % 2 == 0};
            Rectangles.emplace_back(
                useGreen ? std::make_unique<GreenRectangle>(SDL_Rect{60 * Col, 60 * Row, 50, 50})
                         : std::make_unique<Rectangle>(SDL_Rect{60 * Col, 60 * Row, 50, 50}));
        }
    }
}
void UI::Render(SDL_Surface* Surface) const {
    for (auto& Rectangle : Rectangles) {
        Rectangle->Render(Surface);
    }
};

void UI::HandleEvent(SDL_Event& E) {
    for (auto& Rectangle : Rectangles) {
        Rectangle->HandleEvent(E);
    }
};
