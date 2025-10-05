#ifndef SPRITE_HPP
#define SPRITE_HPP

#include <SDL.h>
#include <vector>
#include <string>
#include <memory>

#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

#include "Image.hpp"

enum class SpriteState {
    IDLE,
    WALKING,
    JUMPING,
    ATTACKING,
    DEAD
};

class Sprite {
public:
    Sprite(const std::vector<std::string>& framePaths,
           SDL_PixelFormat* format,
           int frameWidth, int frameHeight,
           int x = 0, int y = 0);

    ~Sprite() = default;

    void Update(float deltaTime);
    void Render(SDL_Surface* surface);

    void SetPosition(int x, int y);
    void SetScale(float scaleX, float scaleY);
    void SetRotation(double angle);
    void SetFlip(SDL_RendererFlip flip);
    void SetAnimationSpeed(float speed);
    void SetLooping(bool loop);

    void Play();
    void Pause();
    void Stop();
    void Reset();

    void SetState(SpriteState newState);
    SpriteState GetState() const { return currentState; }

    SDL_Rect GetRect() const;
    int GetX() const { return positionX; }
    int GetY() const { return positionY; }
    int GetWidth() const { return frameWidth * scaleX; }
    int GetHeight() const { return frameHeight * scaleY; }

    bool IsPlaying() const { return isPlaying; }
    bool IsLooping() const { return isLooping; }

    void HandleEvent(SDL_Event& event);
    bool IsPointInside(int x, int y) const;

private:
    std::vector<std::unique_ptr<Image>> frames;
    int currentFrame;
    int totalFrames;

    int positionX;
    int positionY;
    int frameWidth;
    int frameHeight;

    float scaleX;
    float scaleY;
    double rotation;
    SDL_RendererFlip flip;

    float animationSpeed;
    float frameTimer;
    bool isPlaying;
    bool isLooping;

    SpriteState currentState;

    void LoadFrames(const std::vector<std::string>& framePaths, SDL_PixelFormat* format);
    void AdvanceFrame();
};

#endif // SPRITE_HPP

