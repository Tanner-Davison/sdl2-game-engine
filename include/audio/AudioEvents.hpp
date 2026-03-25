// audio/AudioEvents.hpp -- Pure data: SFX ID constants and AnimationID mapping.
//
// No SDL_mixer dependency. Safe to include anywhere.
// Defines the canonical string IDs that the rest of the codebase uses
// to refer to sound effects, and provides the mapping from AnimationID
// to the player animation SFX IDs.
#pragma once

#include "Components.hpp" // AnimationID
#include <string_view>

namespace audio {

// ── Player animation SFX IDs ─────────────────────────────────────────────────
// Each player animation slot can optionally have a sound effect associated
// with it. These are the canonical IDs used to register and trigger them.

inline constexpr std::string_view kSfxPlayerIdle  = "player_idle";
inline constexpr std::string_view kSfxPlayerWalk  = "player_walk";
inline constexpr std::string_view kSfxPlayerJump  = "player_jump";
inline constexpr std::string_view kSfxPlayerHurt  = "player_hurt";
inline constexpr std::string_view kSfxPlayerDuck  = "player_duck";
inline constexpr std::string_view kSfxPlayerFall  = "player_fall";
inline constexpr std::string_view kSfxPlayerSlash = "player_slash";
inline constexpr std::string_view kSfxPlayerDeath = "player_death";
inline constexpr std::string_view kSfxPlayerLand  = "player_land";

// ── Game event SFX IDs ───────────────────────────────────────────────────────

inline constexpr std::string_view kSfxEnemyStomp    = "enemy_stomp";
inline constexpr std::string_view kSfxGoalCollect   = "goal_collect";
inline constexpr std::string_view kSfxTileBreak     = "tile_break";
inline constexpr std::string_view kSfxPowerUp       = "powerup_pickup";
inline constexpr std::string_view kSfxLevelComplete = "level_complete";
inline constexpr std::string_view kSfxGameOver      = "game_over";
inline constexpr std::string_view kSfxUIClick       = "ui_click";

// ── Mapping helpers ──────────────────────────────────────────────────────────

/// Map an AnimationID to its canonical player SFX ID.
/// Returns an empty view if no SFX is defined for that animation.
constexpr std::string_view PlayerAnimSfxId(AnimationID id) {
    switch (id) {
        case AnimationID::IDLE:  return kSfxPlayerIdle;
        case AnimationID::WALK:  return kSfxPlayerWalk;
        case AnimationID::JUMP:  return kSfxPlayerJump;
        case AnimationID::HURT:  return kSfxPlayerHurt;
        case AnimationID::DUCK:  return kSfxPlayerDuck;
        case AnimationID::FRONT: return kSfxPlayerFall;
        case AnimationID::SLASH: return kSfxPlayerSlash;
        case AnimationID::DEATH: return kSfxPlayerDeath;
        default:                 return {};
    }
}

/// Map a PlayerAnimSlot index to its canonical player SFX ID.
/// Mirrors PlayerAnimSfxId but accepts the integer slot index used by
/// PlayerProfile serialization.
constexpr std::string_view PlayerSlotSfxId(int slot) {
    // PlayerAnimSlot values: Idle=0, Walk=1, Crouch=2, Jump=3,
    //                        Fall=4, Slash=5, Hurt=6, Death=7
    switch (slot) {
        case 0: return kSfxPlayerIdle;
        case 1: return kSfxPlayerWalk;
        case 2: return kSfxPlayerDuck;
        case 3: return kSfxPlayerJump;
        case 4: return kSfxPlayerFall;
        case 5: return kSfxPlayerSlash;
        case 6: return kSfxPlayerHurt;
        case 7: return kSfxPlayerDeath;
        default: return {};
    }
}

} // namespace audio
