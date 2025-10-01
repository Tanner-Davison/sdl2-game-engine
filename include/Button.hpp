#pragma once
#include "Rectangle.hpp"
#include "UserEvents.hpp"
#include <SDL.h>
#include <string>

class UI;

class Button : public Rectangle {
  public:
    Button(UI& UIManager, const SDL_Rect& Rect);

    void                       OnLeftClick() override;
    void                       HandleEvent(SDL_Event& E);
    std::string                GetLocation();
    UserEvents::SettingsConfig GetConfig();

  private:
    UserEvents::SettingsConfig Config{
        UserEvents::SettingsPage::GAMEPLAY, Rect.x, (Rect.y + Rect.h)};

    UI&  UIManager;
    bool isSettingsOpen{false};
};
