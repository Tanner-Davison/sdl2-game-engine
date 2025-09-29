#pragma once
#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif
#include <iostream>
#define ERROR_LOGGING

void CheckSDLError(const std::string& Action) {
#ifdef ERROR_LOGGING
    const char* Error(SDL_GetError());
    if (*Error != '\0') {
        std::cerr << Action << " Error:" << Error << "\n";
        SDL_ClearError();
    }
#endif
}
