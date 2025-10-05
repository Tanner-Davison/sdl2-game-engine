#include "Sprite.hpp"

Sprite::Sprite(const std::vector<std::string>& framePaths,
               SDL_PixelFormat*                format,
               int                             frameWidth,
               int                             frameHeight,
               int                             x,
               int                             y)
    : currentFrame(0)
    , totalFrames(0)
    , positionX(x)
    , positionY(y)
    , frameWidth(frameWidth)
    , frameHeight(frameHeight)
    , scaleX(1.0f)
    , scaleY(1.0f)
    , rotation(0.0)
    , flip(SDL_FLIP_NONE)
    , animationSpeed(1.0f)
    , frameTimer(0.0f)
    , isPlaying(true)
    , isLooping(true)
    , currentState(SpriteState::IDLE) {
    LoadFrames(framePaths, format);
}

void Sprite::LoadFrames(const std::vector<std::string>& framePaths,
                        SDL_PixelFormat*                format) {
    frames.clear();
    for (const auto& path : framePaths) {
        frames.push_back(
            std::make_unique<Image>(path, format, FitMode::SRCSIZE));
    }
    totalFrames  = frames.size();
    currentFrame = 0;
}
void Sprite::Update(float deltaTime) {
    if (!isPlaying || totalFrames <= 1)
        return;

    // Cap deltaTime to prevent huge jumps during loads
    const float maxDeltaTime = 0.1f;
    deltaTime                = std::min(deltaTime, maxDeltaTime);

    // animationSpeed is now frames per second
    float frameInterval = 1.0f / animationSpeed;
    frameTimer += deltaTime;

    while (frameTimer >= frameInterval) {
        AdvanceFrame();
        frameTimer -= frameInterval;
    }
}

void Sprite::AdvanceFrame() {
    currentFrame++;

    if (currentFrame >= totalFrames) {
        if (isLooping) {
            currentFrame = 0;
        } else {
            currentFrame = totalFrames - 1;
            isPlaying    = false;
        }
    }
}

void Sprite::Render(SDL_Surface* surface) {
    if (frames.empty() || currentFrame >= totalFrames)
        return;

    SDL_Rect destRect = {positionX,
                         positionY,
                         static_cast<int>(frameWidth * scaleX),
                         static_cast<int>(frameHeight * scaleY)};

    frames[currentFrame]->SetDestinationRectangle(destRect);
    frames[currentFrame]->Render(surface);
}

void Sprite::SetPosition(int x, int y) {
    positionX = x;
    positionY = y;
}

void Sprite::SetScale(float scaleX, float scaleY) {
    this->scaleX = scaleX;
    this->scaleY = scaleY;
}

void Sprite::SetRotation(double angle) {
    rotation = angle;
}

void Sprite::SetFlip(SDL_RendererFlip flip) {
    this->flip = flip;
}

void Sprite::SetAnimationSpeed(float speed) {
    animationSpeed = speed;
}

void Sprite::SetLooping(bool loop) {
    isLooping = loop;
}

void Sprite::Play() {
    isPlaying = true;
}

void Sprite::Pause() {
    isPlaying = false;
}

void Sprite::Stop() {
    isPlaying = false;
    Reset();
}

void Sprite::Reset() {
    currentFrame = 0;
    frameTimer   = 0.0f;
}

void Sprite::SetState(SpriteState newState) {
    currentState = newState;
}

SDL_Rect Sprite::GetRect() const {
    return {positionX,
            positionY,
            static_cast<int>(frameWidth * scaleX),
            static_cast<int>(frameHeight * scaleY)};
}

void Sprite::HandleEvent(SDL_Event& event) {
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            if (IsPointInside(event.button.x, event.button.y)) {
                // Handle click on sprite
                if (isPlaying) {
                    Pause();
                } else {
                    Play();
                }
            }
        }
    }
}

bool Sprite::IsPointInside(int x, int y) const {
    SDL_Rect rect = GetRect();
    return (x >= rect.x && x <= rect.x + rect.w && y >= rect.y &&
            y <= rect.y + rect.h);
}
