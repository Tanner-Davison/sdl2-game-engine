
#include "Button.hpp"
#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif
Button::Button(const SDL_Rect& R) : Rect(R) {
    Rect.SetColor({255, 165, 0, 255});
};
void Button::Render(SDL_Surface* Surface) const {
    Rect.Render(Surface);
};
void Button::HandleEvent(SDL_Event& E) {};
