// audio/AudioEvents.hpp -- Pure data: SFX ID constants and AnimationID mapping.
//
// No SDL_mixer dependency. Safe to include anywhere.
// Defines the canonical string IDs that the rest of the codebase uses
// to refer to sound effects, and provides the mapping from AnimationID
// to the player animation SFX IDs.
#pragma once

#include "Components.hpp" // AnimationID
#include <string>
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

// ── Enemy animation SFX IDs ──────────────────────────────────────────────────
// Per-enemy-type SFX are registered as "<enemyName>_<slot>" at runtime.
// These helpers build the ID strings dynamically since each enemy type has
// its own set. The constants below are the slot suffixes.

inline constexpr std::string_view kSfxEnemySlotIdle   = "idle";
inline constexpr std::string_view kSfxEnemySlotMove   = "move";
inline constexpr std::string_view kSfxEnemySlotAttack = "attack";
inline constexpr std::string_view kSfxEnemySlotHurt   = "hurt";
inline constexpr std::string_view kSfxEnemySlotDead   = "dead";

/// Build a per-enemy-type indexed SFX ID: "<enemyName>_<slot>_<fileIndex>"
inline std::string EnemySfxId(const std::string& enemyName, int slot, int fileIndex = 0) {
    std::string_view suffix;
    switch (slot) {
        case 0: suffix = kSfxEnemySlotIdle;   break;
        case 1: suffix = kSfxEnemySlotMove;   break;
        case 2: suffix = kSfxEnemySlotAttack; break;
        case 3: suffix = kSfxEnemySlotHurt;   break;
        case 4: suffix = kSfxEnemySlotDead;   break;
        default: return {};
    }
    return enemyName + "_" + std::string(suffix) + "_" + std::to_string(fileIndex);
}

/// Map an EnemyAnimSlot index to its slot suffix.
constexpr std::string_view EnemySlotSfxSuffix(int slot) {
    switch (slot) {
        case 0: return kSfxEnemySlotIdle;
        case 1: return kSfxEnemySlotMove;
        case 2: return kSfxEnemySlotAttack;
        case 3: return kSfxEnemySlotHurt;
        case 4: return kSfxEnemySlotDead;
        default: return {};
    }
}

// ── Game event SFX IDs ───────────────────────────────────────────────────────

inline constexpr std::string_view kSfxEnemyStomp    = "enemy_stomp";
inline constexpr std::string_view kSfxGoalCollect   = "goal_collect";
inline constexpr std::string_view kSfxTileBreak     = "tile_break";
inline constexpr std::string_view kSfxPowerUp       = "powerup_pickup";
inline constexpr std::string_view kSfxLevelComplete = "level_complete";
inline constexpr std::string_view kSfxGameOver      = "game_over";
inline constexpr std::string_view kSfxUIClick       = "ui_click";

// ── Mapping helpers ──────────────────────────────────────────────────────────

/// Map an AnimationID to its base player SFX ID (no index suffix).
constexpr std::string_view PlayerAnimSfxBase(AnimationID id) {
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

/// Build an indexed player SFX ID: "player_walk_0", "player_walk_1", etc.
inline std::string PlayerAnimSfxId(AnimationID id, int fileIndex = 0) {
    auto base = PlayerAnimSfxBase(id);
    if (base.empty()) return {};
    return std::string(base) + "_" + std::to_string(fileIndex);
}

/// Map a PlayerAnimSlot index to its base SFX ID.
constexpr std::string_view PlayerSlotSfxBase(int slot) {
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

/// Build an indexed player slot SFX ID: "player_walk_0", "player_walk_1", etc.
inline std::string PlayerSlotSfxId(int slot, int fileIndex = 0) {
    auto base = PlayerSlotSfxBase(slot);
    if (base.empty()) return {};
    return std::string(base) + "_" + std::to_string(fileIndex);
}

} // namespace audio
