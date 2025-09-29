#pragma once
#include "CursorManager.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>

class Rectangle {
  public:
    Rectangle(const SDL_Rect& _Rect);

    virtual void Render(SDL_Surface* Surface) const;

    void HandleEvent(SDL_Event& E);

    void SetColor(const SDL_Color& NewColor);
    void SetHoverColor(const SDL_Color& NewColor);

    SDL_Color GetColor() const;
    SDL_Color GetHoverColor() const;
    virtual ~Rectangle() = default;

    SDL_Rect Rect;
    bool     isLocked{false};

    SDL_Color Rect_Color{255, 0, 0, 255};
    SDL_Color HoverColor{128, 128, 128, 0};
    SDL_Color PressedColor{96, 96, 96, 0};

    bool isMousePressed{false};
    bool isPointerHovering{false};

  private:
    bool          canStartMoving() const;
    void          startMoving();
    bool          isWithinRect(int x, int y);
    CursorManager cursorManager;
    int           pos_X;
    int           pos_Y;
    bool          isMovingRectangle{false};

    static bool isAnyRectangleMoving;
};
