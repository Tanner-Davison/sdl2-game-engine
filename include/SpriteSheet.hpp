#ifndef SPRITESHEET_HPP
#define SPRITESHEET_HPP

#include <SDL.h>
#include <map>
#include <string>
#include <vector>

class SpriteSheet {
  public:
    SpriteSheet(const std::string& imageFile, const std::string& coordFile);
    ~SpriteSheet();

    SDL_Rect              GetFrame(const std::string& name) const;
    std::vector<SDL_Rect> GetAnimation(const std::string& baseName) const;
    SDL_Surface*          GetSurface() const {
        return surface;
    }

  private:
    SDL_Surface*                    surface;
    std::map<std::string, SDL_Rect> frames;

    void LoadCoordinates(const std::string& coordFile);
    void LoadTextFormat(const std::string& coordFile);
    void LoadXMLFormat(const std::string& coordFile);
};

#endif
