#pragma once
#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif
class CursorManager {
  private:
    SDL_Cursor* defaultCursor;
    SDL_Cursor* GrabCursor;
    SDL_Cursor* HandCursor;
    SDL_Cursor* BlockedCursor;

  public:
    CursorManager() {
        defaultCursor = SDL_GetDefaultCursor();
        GrabCursor    = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
        HandCursor    = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
        BlockedCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);
    }

    ~CursorManager() {
        SDL_FreeCursor(defaultCursor);
        SDL_FreeCursor(GrabCursor);
        SDL_FreeCursor(HandCursor);
        SDL_FreeCursor(BlockedCursor);
    }

    void setGrabCursor() {
        SDL_SetCursor(GrabCursor);
    }

    void setDefaultCursor() {
        SDL_SetCursor(defaultCursor);
    }
    void setHandCursor() {
        SDL_SetCursor(HandCursor);
    }

    void setBlockedCursor() {
        SDL_SetCursor(BlockedCursor);
    }
};
