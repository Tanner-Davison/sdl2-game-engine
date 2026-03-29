#pragma once
#include <SDL3/SDL.h>
#include <iostream>

/// Define this to enable SDL error logging to stderr.
#define ERROR_LOGGING

/// Logs and clears any pending SDL error when ERROR_LOGGING is defined.
inline void CheckSDLError(const std::string& Action) {
#ifdef ERROR_LOGGING
    const char* Error(SDL_GetError());
    if (*Error != '\0') {
        std::cerr << Action << " Error:" << Error << "\n";
        SDL_ClearError();
    }
#endif
}
