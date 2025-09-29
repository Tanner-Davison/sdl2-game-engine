#include "UI.hpp"

UI::UI() {}

void UI::Render(SDL_Surface* Surface) const {
    A.Render(Surface);
    B.Render(Surface);
};

void UI::HandleEvent(SDL_Event& E) {
    A.HandleEvent(E);
    B.HandleEvent(E);
};
