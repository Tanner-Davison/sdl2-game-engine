#pragma once
#include <SDL.h>
#include <map>
#include <string>
#include <vector>

struct SpriteFrame {
    SDL_Rect    rect; // x, y, w, h from the coordinate file
    std::string name;
};

class SpriteSheet {
  public:
    SpriteSheet(const std::string& imageFile, const std::string& coordFile);

    // Get a single frame by name
    SDL_Rect GetFrame(const std::string& name) const;

    // Get an animation sequence (e.g., all p1_walk frames)
    std::vector<SDL_Rect> GetAnimation(const std::string& baseName) const;

    SDL_Surface* GetSurface() const {
        return surface;
    }

    ~SpriteSheet();

  private:
    SDL_Surface*                    surface;
    std::map<std::string, SDL_Rect> frames;

    void LoadCoordinates(const std::string& coordFile);
};
