#include "UI.hpp"

void UI::Render(SDL_Renderer* renderer) const {
    B.Render(renderer);
    C.Render(renderer);
    SettingsButton.Render(renderer);
    Settings.Render(renderer);
}

void UI::HandleEvent(SDL_Event& E) {
    B.HandleEvent(E);
    C.HandleEvent(E);
    SettingsButton.HandleEvent(E);
    Settings.HandleEvent(E);
}
