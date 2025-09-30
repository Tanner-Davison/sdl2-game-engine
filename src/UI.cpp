#include "UI.hpp"

void UI::Render(SDL_Surface* Surface) const {
    TopMenu.Render(Surface);
    Rectangles.Render(Surface);
    BottomMenu.Render(Surface);
};

void UI::HandleEvent(SDL_Event& E) {
    TopMenu.HandleEvent(E);
    Rectangles.HandleEvent(E);
    BottomMenu.HandleEvent(E);
};
