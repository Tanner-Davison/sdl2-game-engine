#pragma once

#include "Rectangle.hpp"
#include "UserEvents.hpp"
#include <SDL.h>
#include <string>

class UI;

/**
 * @class Button
 * @brief A clickable UI button component that inherits from Rectangle
 *
 * The Button class represents an interactive UI element that can respond to
 * mouse clicks and manage settings configuration. It extends the Rectangle
 * class to provide geometric bounds and click detection capabilities.
 *
 * @details
 * Buttons maintain a reference to the UI manager and track their own settings
 * state. When clicked, they can toggle settings panels and provide
 * configuration information about their position and associated settings page.
 *
 * @see Rectangle
 * @see UI
 * @see UserEvents::SettingsConfig
 */
class Button : public Rectangle {
  public:
    /**
     * @brief Constructs a new Button object
     *
     * @param UIManager Reference to the UI manager that controls this button
     * @param Rect SDL_Rect defining the button's position and dimensions
     *
     * @details
     * Initializes the button with its geometric bounds and establishes a
     * connection to the UI manager. The settings configuration is automatically
     * initialized based on the provided rectangle's position.
     */
    Button(UI& UIManager, const SDL_Rect& Rect);

    /**
     * @brief Handles left mouse button click events
     *
     * @details
     * Overrides the base Rectangle class method to provide button-specific
     * click behavior. This method is called when the button is clicked and
     * typically triggers UI state changes or actions.
     *
     * @note This method overrides Rectangle::OnLeftClick()
     */
    void OnLeftClick() override;

    /**
     * @brief Processes SDL events for this button
     *
     * @param E Reference to the SDL_Event to be processed
     *
     * @details
     * Handles incoming SDL events such as mouse clicks, hover states, or
     * other input events relevant to the button. This method delegates
     * event processing and updates button state accordingly.
     */
    void HandleEvent(SDL_Event& E);

    /**
     * @brief Retrieves the button's location as a formatted string
     *
     * @return std::string A string representation of the button's position
     *
     * @details
     * Returns a human-readable string describing the button's current location,
     * typically used for debugging or UI layout information.
     */
    std::string GetLocation();

    /**
     * @brief Retrieves the button's settings configuration
     *
     * @return UserEvents::SettingsConfig The current settings configuration
     *
     * @details
     * Returns the configuration object that defines which settings page this
     * button is associated with and its positioning information for the
     * settings panel.
     */
    UserEvents::SettingsConfig GetConfig();

  private:
    /**
     * @brief Settings configuration for this button
     *
     * @details
     * Stores the settings page association and position information for
     * displaying the settings panel. Initialized with GAMEPLAY settings page
     * and positioned relative to the button's rectangle.
     */
    UserEvents::SettingsConfig Config{
        UserEvents::SettingsPage::GAMEPLAY, Rect.x, (Rect.y + Rect.h)};

    /**
     * @brief Reference to the parent UI manager
     *
     * @details
     * Maintains a reference to the UI manager for communication and state
     * updates. This allows the button to trigger UI-level actions and
     * coordinate with other UI components.
     */
    UI& UIManager;

    /**
     * @brief Tracks whether the settings panel is currently open
     *
     * @details
     * Boolean flag indicating the current state of the settings panel
     * associated with this button. Used to toggle between open and closed
     * states when the button is clicked.
     */
    bool isSettingsOpen{false};
};
