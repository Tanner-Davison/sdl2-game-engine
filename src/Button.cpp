#include "Button.hpp"
#include "UI.hpp"

Button::Button(UI& UIManager, const SDL_Rect& Rect)
    : UIManager{UIManager}
    , Rectangle{Rect} {
    SetColor({255, 165, 0, 255});
}
