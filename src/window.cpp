#include "window.hpp"
#include "SDL_video.h"
#include <stdexcept>

SDLWindow::SDLWindow(int width, int height, const std::string &title)
    : SCREEN_WIDTH(width), SCREEN_HEIGHT(height), gWindow(nullptr),
      gScreenSurface(nullptr), gHelloWorld(nullptr), gRenderer(nullptr) {
  if (!init()) {
    throw std::runtime_error("Failed to initialize SDL window.");
  }
}

SDLWindow::~SDLWindow() { close(); }

bool SDLWindow::init() {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return false;
  }

  gWindow = SDL_CreateWindow("SDL Window", SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH,
                             SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
  if (gWindow == nullptr) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return false;
  }

  gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED);
  if (gRenderer == nullptr) {
    printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
    return false;
  }

  gScreenSurface = SDL_GetWindowSurface(gWindow);
  return true;
}

bool SDLWindow::loadMedia(const char *path) { // UNUSED
  gHelloWorld = SDL_LoadBMP(path);
  if (gHelloWorld == nullptr) {
    printf("Unable to load image %s! SDL Error: %s\n", path, SDL_GetError());
    return false;
  }
  return true;
}

void SDLWindow::close() {
  if (gHelloWorld != nullptr) {
    SDL_FreeSurface(gHelloWorld);
    gHelloWorld = nullptr;
  }

  if (gRenderer != nullptr) {
    SDL_DestroyRenderer(gRenderer);
    gRenderer = nullptr;
  }

  if (gWindow != nullptr) {
    SDL_DestroyWindow(gWindow);
    gWindow = nullptr;
  }
  if (gScreenSurface != nullptr) {
    SDL_FreeSurface(gScreenSurface);
    gScreenSurface = nullptr;
  }
  SDL_Quit();
}
