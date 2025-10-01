#include "Button.hpp"
#include "UI.hpp"
#include "UserEvents.hpp"

Button::Button(UI& UIManager, const SDL_Rect& Rect)
    : UIManager{UIManager}
    , Rectangle{Rect} {
    SetColor({255, 165, 0, 255});
}

void Button::HandleEvent(SDL_Event& E) {
    Rectangle::HandleEvent(E);
    using namespace UserEvents;
    if (E.type == CLOSE_SETTINGS) {
        isSettingsOpen = false;
    } else if (E.type == UserEvents::OPEN_SETTINGS) {
        isSettingsOpen = true;
    }
}
void Button::OnLeftClick() { // override
    using namespace UserEvents;
    SDL_Event Event{isSettingsOpen ? CLOSE_SETTINGS : OPEN_SETTINGS};
    if (Event.type == OPEN_SETTINGS) {
        Event.user.data1 = this;
        SetColor({100, 100, 0, 0});
    } else if (Event.type == CLOSE_SETTINGS) {
        SetColor({0, 200, 0, 255});
    }

    SDL_PushEvent(&Event);
}

std::string Button::GetLocation() {
    return "The main menu";
};
UserEvents::SettingsConfig Button::GetConfig() {
    return Config;
};
