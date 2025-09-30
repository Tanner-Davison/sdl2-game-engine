#include "Button.hpp"
#include "UI.hpp"

Button::Button(UI& UIManager, const SDL_Rect& Rect)
    : UIManager{UIManager}
    , Rectangle{Rect} {
    SetColor({255, 165, 0, 255}); // Orange
    std::cout << "Button constructed at " << this << std::endl;
}
void Button::OnLeftClick() {
    std::cout << "[Left Clicked]-> Rectangle::Button" << std::endl;
}
