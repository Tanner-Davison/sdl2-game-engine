#pragma once
#include "UserEvents.hpp"
#include <SDL_events.h>
#pragma once
#include "Rectangle.hpp"
#include <SDL.h>

class UI;

class Button : public Rectangle {
  public:
    Button(UI& UIManager, const SDL_Rect& Rect);

    void OnLeftClick() override;
    void HandleEvent(SDL_Event& E);

  private:
    UI&  UIManager;
    bool isSettingsOpen{false};

    UserEvents::SettingsConfig Config{
        UserEvents::SettingsPage::GAMEPLAY, 50, 50};
};
