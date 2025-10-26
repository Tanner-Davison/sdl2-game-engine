#include "Sprite.hpp"
#include <iostream>

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
    , isPlaying(false)
    , isLooping(true)
    , currentState(SpriteState::IDLE) {
    LoadFrames(framePaths, format);
}

Sprite::Sprite(SDL_Surface*                 spriteSheet,
               const std::vector<SDL_Rect>& frameRects,
               SDL_PixelFormat*             format,
               float                        x,
               float                        y)
    : currentFrame(0)
    , totalFrames(0)
    , positionX(x)
    , positionY(y)
    , frameWidth(0)
    , frameHeight(0)
    , scaleX(1.0f)
    , scaleY(1.0f)
    , rotation(0.0)
    , flip(SDL_FLIP_NONE)
    , animationSpeed(1.0f)
    , frameTimer(0.0f)
    , isPlaying(true)
    , isLooping(true)
    , currentState(SpriteState::IDLE) {
    LoadFramesFromSheet(spriteSheet, frameRects, format);
}

// Add this new method for Sprite sheet loading
void Sprite::LoadFramesFromSheet(SDL_Surface*                 spriteSheet,
                                 const std::vector<SDL_Rect>& frameRects,
                                 SDL_PixelFormat*             format) {
    frames.clear();

    if (frameRects.empty()) {
        std::cout << "No frame rects provided!" << std::endl;
        return;
    }

    // Get frame dimensions from first frame
    frameWidth  = frameRects[0].w;
    frameHeight = frameRects[0].h;

    for (const auto& srcRect : frameRects) {
        // Create a surface for this frame with RGBA format
        SDL_Surface* frameSurface = SDL_CreateRGBSurfaceWithFormat(
            0,
            srcRect.w,
            srcRect.h,
            32,
            SDL_PIXELFORMAT_RGBA8888); // Use RGBA8888

        if (!frameSurface) {
            std::cout << "Failed to create frame surface" << std::endl;
            continue;
        }

        SDL_SetSurfaceBlendMode(frameSurface, SDL_BLENDMODE_BLEND);

        // Copy this region from the sprite sheet
        SDL_BlitSurface(spriteSheet, &srcRect, frameSurface, nullptr);

        // No need to convert again since we already created it in RGBA format

        // Create an Image from this surface
        frames.push_back(
            std::make_unique<Image>(frameSurface, FitMode::SRCSIZE));
    }

    totalFrames  = frames.size();
    currentFrame = 0;

    // std::cout << "Loaded " << totalFrames << " frames from sprite sheet"
    //           << std::endl;
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
    // Check if any movement key is pressed
    bool isMoving = movingUp || movingDown || movingLeft || movingRight;

    // Automatically flip sprite based on horizontal movement
    if (movingLeft) {
        SetFlipHorizontal(true);
    } else if (movingRight) {
        SetFlipHorizontal(false);
    }

    // Only animate when moving
    if (isMoving && totalFrames > 1) {
        const float maxDeltaTime = 0.1f;
        deltaTime                = std::min(deltaTime, maxDeltaTime);

        float frameInterval = 1.0f / animationSpeed;
        frameTimer += deltaTime;

        while (frameTimer >= frameInterval) {
            AdvanceFrame();
            frameTimer -= frameInterval;
        }
    } else {
        // Reset to first frame when idle
        currentFrame = 0;
        frameTimer   = 0.0f;
    }

    // Movement update
    float moveDistance = moveSpeed * deltaTime;

    if (movingUp)
        positionY -= moveDistance;
    if (movingDown)
        positionY += moveDistance;
    if (movingLeft)
        positionX -= moveDistance;
    if (movingRight)
        positionX += moveDistance;
}
void Sprite::SetMoveSpeed(float speed) {
    moveSpeed = speed;
}

void Sprite::SetFlipHorizontal(bool flip) {
    for (auto& frame : frames) {
        frame->SetFlipHorizontal(flip);
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

    SDL_Rect destRect = {
        static_cast<int>(positionX), // Cast to int for rendering
        static_cast<int>(positionY), // Cast to int for rendering
        static_cast<int>(frameWidth * scaleX),
        static_cast<int>(frameHeight * scaleY)};

    frames[currentFrame]->SetDestinationRectangle(destRect);
    frames[currentFrame]->Render(surface);
}

void Sprite::SetPosition(float x, float y) {
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
    return {static_cast<int>(positionX),
            static_cast<int>(positionY),
            static_cast<int>(frameWidth * scaleX),
            static_cast<int>(frameHeight * scaleY)};
}
void Sprite::HandleEvent(SDL_Event& event) {
    // Mouse click toggle
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            if (IsPointInside(event.button.x, event.button.y)) {
                if (isPlaying) {
                    Pause();
                } else {
                    Play();
                }
            }
        }
    }

    // Key press - start moving
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_w:
                movingUp = true;
                break;
            case SDLK_a:
                movingLeft = true;
                break;
            case SDLK_s:
                movingDown = true;
                break;
            case SDLK_d:
                movingRight = true;
                break;
        }
    }

    // Key release - stop moving
    if (event.type == SDL_KEYUP) {
        switch (event.key.keysym.sym) {
            case SDLK_w:
                movingUp = false;
                break;
            case SDLK_a:
                movingLeft = false;
                break;
            case SDLK_s:
                movingDown = false;
                break;
            case SDLK_d:
                movingRight = false;
                break;
        }
    }
}
bool Sprite::IsPointInside(int x, int y) const {
    SDL_Rect rect = GetRect();
    return (x >= rect.x && x <= rect.x + rect.w && y >= rect.y &&
            y <= rect.y + rect.h);
}
