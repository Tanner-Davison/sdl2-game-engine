#pragma once
#include <SDL2/SDL.h>
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
