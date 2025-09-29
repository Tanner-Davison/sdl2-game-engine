#pragma once
#include "CursorManager.hpp"

#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif

class Rectangle {
  public:
    Rectangle(const SDL_Rect& _Rect);
    virtual void Render(SDL_Surface* Surface) const;

    void      HandleEvent(SDL_Event& E);
    void      SetColor(const SDL_Color& NewColor);
    void      SetHoverColor(const SDL_Color& NewColor);
    SDL_Color GetColor() const;
    SDL_Color GetHoverColor() const;

    virtual ~Rectangle() = default;

    SDL_Rect  Rect;
    SDL_Color Rect_Color{255, 0, 0, 255};
    SDL_Color HoverColor{128, 128, 128, 0};
    SDL_Color PressedColor{96, 96, 96, 0};

  protected:
    bool         isMousePressed{false};
    bool         isPointerHovering{false};
    bool         isLocked{false};
    virtual void OnMouseEnter();
    virtual void OnMouseExit();
    virtual void OnLeftClick();
    virtual void OnRightClick();

  private:
    /**
     * @brief Checks if  the Rectangle can begin moving based on input and state conditions
     *
     * - isMousePressed
     * - isPointerHovering
     * - !isLocked
     * - !isAnyRectangleMoving
     *
     * @return true if all conditions are met
     */
    bool canStartMoving() const;
    /**
     * @brief Toggles Conditional varibales to enable movement
     * - isMovingRectangle -> true
     * - isAnyRectangleMoving -> true
     * - Cursor State -> setGrabCursor()
     * @return --VOID--
     */
    void startMoving();
    /**
     * @brief Checks if a point is within the rectangle's bounds
     *
     * @param x The x-coordinate to test
     * @param y The y-coordinate to test
     * @return true if the point is inside the rectangle, false otherwise
     */
    bool isWithinRect(int x, int y) const;

    CursorManager cursorManager;
    int           pos_X;
    int           pos_Y;
    bool          isMovingRectangle{false};
    static bool   isAnyRectangleMoving;
};
