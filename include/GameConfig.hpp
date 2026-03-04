#pragma once
#include <cmath>

// -- Player stats -------------------------------------------------------------
inline constexpr float PLAYER_HIT_DAMAGE        = 15.0f;
inline constexpr float HAZARD_DAMAGE_PER_SEC    = 30.0f;  // HP drained per second while standing on a hazard tile
inline constexpr float PLAYER_INVINCIBILITY = 1.5f;
inline constexpr float PLAYER_MAX_HEALTH    = 100.0f;

// -- Player sprite / frame dimensions (pixels) --------------------------------
inline constexpr int PLAYER_SPRITE_WIDTH  = 120;
inline constexpr int PLAYER_SPRITE_HEIGHT = 160;

// -- Player collider insets ---------------------------------------------------
// These are pixels trimmed from the sprite edges to get the hitbox.
// Measured from the actual 120x160 rendered sprite frame:
//   - Character top-of-head starts ~17px down from the sprite top
//   - Character feet end ~14px from the sprite bottom
//   - Character body is ~16px inset from each side
// Pixel-exact values measured via alpha-channel scan across idle+walk frames
// scaled to the 120x160 render size. Character occupies x=32-85, y=33-133.
// INSET_X uses the left edge (32) so the box never clips the body on either side.
inline constexpr int PLAYER_BODY_INSET_X      = 32;
inline constexpr int PLAYER_BODY_INSET_TOP    = 33;
inline constexpr int PLAYER_BODY_INSET_BOTTOM = 26;

// -- Player collider dimensions (derived) -------------------------------------
inline constexpr int PLAYER_STAND_WIDTH = PLAYER_SPRITE_WIDTH - PLAYER_BODY_INSET_X * 2;
inline constexpr int PLAYER_STAND_HEIGHT =
    PLAYER_SPRITE_HEIGHT - PLAYER_BODY_INSET_TOP - PLAYER_BODY_INSET_BOTTOM;
inline constexpr int PLAYER_DUCK_WIDTH  = PLAYER_STAND_WIDTH;
inline constexpr int PLAYER_DUCK_HEIGHT = PLAYER_STAND_HEIGHT / 2;

// -- Player render offsets (derived) ------------------------------------------
inline constexpr int PLAYER_STAND_ROFF_X = -PLAYER_BODY_INSET_X;
inline constexpr int PLAYER_STAND_ROFF_Y = -PLAYER_BODY_INSET_TOP;
inline constexpr int PLAYER_DUCK_ROFF_X  = -PLAYER_BODY_INSET_X;
inline constexpr int PLAYER_DUCK_ROFF_Y  = -(PLAYER_SPRITE_HEIGHT - PLAYER_DUCK_HEIGHT);

// -- Enemy stats -------------------------------------------------------------
inline constexpr float SLIME_MAX_HEALTH = 30.0f;
inline constexpr float SLASH_DAMAGE     = 30.0f; // damage dealt per sword swing

// -- Sword / attack hitbox ---------------------------------------------------
inline constexpr float SWORD_REACH  = 36.0f; // px the hitbox extends in front of the player
inline constexpr float SWORD_HEIGHT = 0.72f; // fraction of player collider height covered

// -- Enemy sprite dimensions --------------------------------------------------
inline constexpr int SLIME_SPRITE_WIDTH  = 36;
inline constexpr int SLIME_SPRITE_HEIGHT = 26;

// -- Physics ------------------------------------------------------------------
inline constexpr float GRAVITY_DURATION   = 5.0f;
inline constexpr float GRAVITY_FORCE      = 1000.0f;
inline constexpr float JUMP_FORCE         = 600.0f;
inline constexpr float MAX_FALL_SPEED     = 1500.0f;
inline constexpr float PLAYER_SPEED       = 250.0f;
inline constexpr float CLIMB_SPEED        = 350.0f;
inline constexpr float CLIMB_STRAFE_SPEED = 150.0f;

// -- Tile step-up -------------------------------------------------------------
// Matches the editor grid size so the player can walk onto a single tile
// placed at floor level without being laterally blocked.
inline constexpr float STEP_UP_HEIGHT = 48.0f;

// -- Slope ground-stick -------------------------------------------------------
inline constexpr float SLOPE_SNAP_LOOKAHEAD = 40.0f;
inline constexpr float SLOPE_STICK_VELOCITY = 16.0f;

// -- World / spawn counts -----------------------------------------------------
inline constexpr int GRAVITYSLUGSCOUNT = 20;
inline constexpr int COIN_COUNT        = 8;
inline constexpr int COIN_SIZE         = 40;

// -- Camera -------------------------------------------------------------------
inline constexpr float CAM_LERP_SPEED  = 6.0f;   // higher = snappier follow
inline constexpr float CAM_DEADZONE_X  = 80.0f;  // px from center before camera moves horizontally
inline constexpr float CAM_DEADZONE_Y  = 60.0f;  // px from center before camera moves vertically

struct Camera {
    float x = 0.0f;  // current top-left world position shown at screen origin
    float y = 0.0f;

    // Call once per frame after the player has moved.
    // playerCX/CY: centre of the player collider in world space.
    // viewW/viewH:  screen dimensions.
    // levelW/levelH: total world size in pixels (0 = unbounded).
    void Update(float playerCX, float playerCY,
                int viewW, int viewH,
                float levelW, float levelH, float dt) {
        float halfW = viewW * 0.5f;
        float halfH = viewH * 0.5f;

        // Desired camera position centres the player on screen
        float desiredX = playerCX - halfW;
        float desiredY = playerCY - halfH;

        // Deadzone: only move the camera when the player leaves the dead region
        float diffX = desiredX - x;
        float diffY = desiredY - y;
        if (diffX >  CAM_DEADZONE_X) desiredX = x + diffX - CAM_DEADZONE_X;
        else if (diffX < -CAM_DEADZONE_X) desiredX = x + diffX + CAM_DEADZONE_X;
        else                              desiredX = x;
        if (diffY >  CAM_DEADZONE_Y) desiredY = y + diffY - CAM_DEADZONE_Y;
        else if (diffY < -CAM_DEADZONE_Y) desiredY = y + diffY + CAM_DEADZONE_Y;
        else                              desiredY = y;

        // Smooth lerp toward the target
        float t = 1.0f - std::exp(-CAM_LERP_SPEED * dt);
        x += (desiredX - x) * t;
        y += (desiredY - y) * t;

        // Clamp so camera never shows outside the level bounds
        if (levelW > 0.0f) {
            if (x < 0.0f)              x = 0.0f;
            if (x + viewW > levelW)    x = levelW - viewW;
            if (x < 0.0f)             x = 0.0f; // level narrower than viewport
        }
        if (levelH > 0.0f) {
            if (y < 0.0f)              y = 0.0f;
            if (y + viewH > levelH)    y = levelH - viewH;
            if (y < 0.0f)             y = 0.0f;
        }
    }
};
