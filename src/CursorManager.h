
#include <SDL2/SDL_mouse.h>
class CursorManager {
  private:
    SDL_Cursor* defaultCursor;
    SDL_Cursor* handCursor;

  public:
    CursorManager() {
        defaultCursor = SDL_GetDefaultCursor();
        handCursor    = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    }

    ~CursorManager() {
        SDL_FreeCursor(defaultCursor);
        SDL_FreeCursor(handCursor);
    }

    void setHandCursor() {
        SDL_SetCursor(handCursor);
    }

    void setDefaultCursor() {
        SDL_SetCursor(defaultCursor);
    }
};
