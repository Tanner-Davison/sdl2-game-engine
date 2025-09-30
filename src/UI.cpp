#include "UI.hpp"

void UI::Render(SDL_Surface* Surface) const {
    // A.Render(Surface);
    B.Render(Surface);
    C.Render(Surface);
    SettingsButton.Render(Surface);
    Settings.Render(Surface);
};

void UI::HandleEvent(SDL_Event& E) {
    // A.HandleEvent(E);
    B.HandleEvent(E);
    C.HandleEvent(E);

    SettingsButton.HandleEvent(E);
    Settings.HandleEvent(E);
};
