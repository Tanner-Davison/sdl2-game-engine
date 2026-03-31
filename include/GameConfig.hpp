#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>

// --- Player stats ---

inline constexpr float PLAYER_HIT_DAMAGE     = 15.0f;
inline constexpr float HAZARD_DAMAGE_PER_SEC = 30.0f;
inline constexpr float PLAYER_INVINCIBILITY  = 1.5f;
inline constexpr float PLAYER_MAX_HEALTH     = 100.0f;

inline constexpr int PLAYER_SPRITE_WIDTH  = 120;
inline constexpr int PLAYER_SPRITE_HEIGHT = 160;

// Collider insets: pixels trimmed from the 120x160 sprite edges.
// Measured via alpha-channel scan across idle+walk frames.
// Character occupies x=32-85, y=33-133 at render size.
inline constexpr int PLAYER_BODY_INSET_X      = 32;
inline constexpr int PLAYER_BODY_INSET_TOP    = 33;
inline constexpr int PLAYER_BODY_INSET_BOTTOM = 26;

inline constexpr int PLAYER_STAND_WIDTH = PLAYER_SPRITE_WIDTH - PLAYER_BODY_INSET_X * 2;
inline constexpr int PLAYER_STAND_HEIGHT =
    PLAYER_SPRITE_HEIGHT - PLAYER_BODY_INSET_TOP - PLAYER_BODY_INSET_BOTTOM;
inline constexpr int PLAYER_DUCK_WIDTH  = PLAYER_STAND_WIDTH;
inline constexpr int PLAYER_DUCK_HEIGHT = PLAYER_STAND_HEIGHT / 2;

inline constexpr int PLAYER_STAND_ROFF_X = -PLAYER_BODY_INSET_X;
inline constexpr int PLAYER_STAND_ROFF_Y = -PLAYER_BODY_INSET_TOP;
inline constexpr int PLAYER_DUCK_ROFF_X  = -PLAYER_BODY_INSET_X;
inline constexpr int PLAYER_DUCK_ROFF_Y  = -(PLAYER_SPRITE_HEIGHT - PLAYER_DUCK_HEIGHT);

// --- Enemy stats ---

inline constexpr float SLIME_MAX_HEALTH  = 30.0f;
inline constexpr float SLASH_DAMAGE      = 30.0f;
inline constexpr float STOMP_DAMAGE_FRAC = 0.15f;

inline constexpr float SWORD_REACH  = 56.0f;
inline constexpr float SWORD_HEIGHT = 0.80f;

inline constexpr int SLIME_SPRITE_WIDTH  = 36;
inline constexpr int SLIME_SPRITE_HEIGHT = 26;

// --- Physics ---

inline constexpr float GRAVITY_DURATION   = 5.0f;
inline constexpr float GRAVITY_FORCE      = 1000.0f;
inline constexpr float JUMP_FORCE         = 500.0f;
inline constexpr float MAX_FALL_SPEED     = 1500.0f;
inline constexpr float PLAYER_SPEED       = 250.0f;
inline constexpr float CLIMB_SPEED        = 350.0f;
inline constexpr float CLIMB_STRAFE_SPEED = 220.0f;
inline constexpr float SPRINT_MULTIPLIER  = 1.2f;
inline constexpr float CROUCH_FRICTION    = 3.0f;

inline constexpr float DASH_SPEED      = 800.0f;
inline constexpr float DASH_DURATION   = 0.35f;
inline constexpr float DASH_COOLDOWN   = 0.45f;
inline constexpr float DASH_TAP_WINDOW = 0.25f;

// Matches the editor grid size so the player can walk onto a single tile.
inline constexpr float STEP_UP_HEIGHT = 48.0f;

inline constexpr float SLOPE_SNAP_LOOKAHEAD = 40.0f;
inline constexpr float SLOPE_STICK_VELOCITY = 16.0f;

// --- Camera ---

inline constexpr float CAM_LERP_SPEED = 12.0f;
inline constexpr float CAM_DEADZONE_X = 80.0f;
inline constexpr float CAM_DEADZONE_Y = 60.0f;

struct Camera {
    float x = 0.0f;
    float y = 0.0f;

    float shakeTimer     = 0.0f;
    float shakeDuration  = 0.0f;
    float shakeIntensity = 0.0f;
    float shakeOffX      = 0.0f;
    float shakeOffY      = 0.0f;

    void StartShake(float intensity, float duration) {
        if (intensity >= shakeIntensity || shakeTimer <= 0.0f) {
            shakeIntensity = intensity;
            shakeTimer     = duration;
            shakeDuration  = duration;
        }
    }

    float punchTimer     = 0.0f;
    float punchDuration  = 0.0f;
    float punchIntensity = 0.0f;
    float punchDirX      = 0.0f;
    float punchDirY      = 0.0f;
    float punchOffX      = 0.0f;
    float punchOffY      = 0.0f;

    void StartPunch(float dirX, float dirY, float intensity, float duration) {
        punchDirX      = dirX;
        punchDirY      = dirY;
        punchIntensity = intensity;
        punchTimer     = duration;
        punchDuration  = duration;
    }

    void TickShake(float dt) {
        if (shakeTimer > 0.0f) {
            shakeTimer -= dt;
            float factor = std::max(shakeTimer, 0.0f) / shakeDuration;
            float amp    = shakeIntensity * factor;
            shakeOffX    = ((rand() % 201 - 100) / 100.0f) * amp;
            shakeOffY    = ((rand() % 201 - 100) / 100.0f) * amp;
        } else {
            shakeOffX = 0.0f;
            shakeOffY = 0.0f;
        }

        if (punchTimer > 0.0f) {
            punchTimer -= dt;
            float t   = std::max(punchTimer, 0.0f) / punchDuration;
            float amp = punchIntensity * t;
            punchOffX = punchDirX * amp;
            punchOffY = punchDirY * amp;
        } else {
            punchOffX = 0.0f;
            punchOffY = 0.0f;
        }
    }

    void Update(float playerCX,
                float playerCY,
                int   viewW,
                int   viewH,
                float levelW,
                float levelH,
                float dt) {
        float halfW = viewW * 0.5f;
        float halfH = viewH * 0.5f;

        float desiredX = playerCX - halfW;
        float desiredY = playerCY - halfH;

        float diffX = desiredX - x;
        float diffY = desiredY - y;
        if (diffX > CAM_DEADZONE_X)
            desiredX = x + diffX - CAM_DEADZONE_X;
        else if (diffX < -CAM_DEADZONE_X)
            desiredX = x + diffX + CAM_DEADZONE_X;
        else
            desiredX = x;
        if (diffY > CAM_DEADZONE_Y)
            desiredY = y + diffY - CAM_DEADZONE_Y;
        else if (diffY < -CAM_DEADZONE_Y)
            desiredY = y + diffY + CAM_DEADZONE_Y;
        else
            desiredY = y;

        float t = 1.0f - std::exp(-CAM_LERP_SPEED * dt);
        x += (desiredX - x) * t;
        y += (desiredY - y) * t;

        if (levelW > 0.0f) {
            if (x < 0.0f)
                x = 0.0f;
            if (x + viewW > levelW)
                x = levelW - viewW;
            if (x < 0.0f)
                x = 0.0f;
        }
        if (levelH > 0.0f) {
            if (y < 0.0f)
                y = 0.0f;
            if (y + viewH > levelH)
                y = levelH - viewH;
            if (y < 0.0f)
                y = 0.0f;
        }
    }
};
