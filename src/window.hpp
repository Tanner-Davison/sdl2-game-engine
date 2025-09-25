#define SDL_MAIN_HANDLED
// window.hpp
#pragma once
#ifdef __linux__
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif
#include <string>

class SDLWindow {
 public:
	 SDLWindow(int width, int height, const std::string &title);
	 ~SDLWindow();

	 bool init();                       // Starts up SDL and creates the window
	 bool loadMedia(const char *path);  // Loads media
	 void close();                      // Frees media and shuts down SDL

	 SDL_Window *getWindow() const { return gWindow; }
	 SDL_Renderer *getRenderer() const { return gRenderer; }
	 SDL_Surface *getScreenSurface() const { return gScreenSurface; }

 private:
	 int SCREEN_WIDTH;
	 int SCREEN_HEIGHT;

	 SDL_Window *gWindow;          // Destroyed Window;
	 SDL_Surface *gScreenSurface;  // Freed surface;
	 SDL_Surface *gHelloWorld;     // Freed Surface;
	 SDL_Renderer *gRenderer;      // Destroyed Renderer;
};
