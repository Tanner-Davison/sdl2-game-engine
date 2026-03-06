#pragma once
#include <SDL3/SDL.h>
#include "Button.hpp"
#include "Rectangle.hpp"
#include "SettingsMenu.hpp"

/**
 * @class UI
 * @brief Top-level UI manager that owns and coordinates all UI elements.
 *
 * The UI class serves as the root container for all interface elements —
 * rectangles, buttons, and menus. It forwards SDL events to each child
 * and renders them in order onto the window surface.
 *
 * @par Usage
 * Instantiate one UI per window. Call HandleEvent() inside the SDL event
 * loop and Render() during the render phase each frame.
 *
 * @see Button
 * @see Rectangle
 * @see SettingsMenu
 */
class UI {
  public:
    /**
     * @brief Renders all UI elements to the given surface.
     * @param Surface The window surface to draw UI elements onto.
     */
    void Render(SDL_Renderer* renderer) const;

    /**
     * @brief Forwards an SDL event to all child UI elements.
     *
     * Each element (Rectangle, Button, SettingsMenu) processes the event
     * independently and updates its own state accordingly.
     *
     * @param E The SDL_Event to dispatch.
     */
    void HandleEvent(SDL_Event& E);

  private:
    Rectangle    A{{350, 50, 50, 50}};             ///< A plain rectangle UI element.
    Rectangle    B{{50, 50, 50, 50}};              ///< A plain rectangle UI element.
    Button       C{*this, {150, 50, 50, 50}};      ///< A clickable button.
    Button       SettingsButton{*this, {250, 50, 150, 50}}; ///< Button that opens the settings menu.
    SettingsMenu Settings;                         ///< The settings panel managed by SettingsButton.
};
