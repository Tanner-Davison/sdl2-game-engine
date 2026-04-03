#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <vector>

// =============================================================================
//  BloodParticleSystem
// =============================================================================
//
//  Nidhogg-inspired chunky blood physics — particles squirt from hit points,
//  arc under gravity, fade out, and leave persistent stains on the ground.
//
//  Designed for maximum extensibility:
//    • Each hit type (slash, stomp, player hit, death…) has its own
//      BloodConfig that fully controls particle count, speed, spread,
//      colours, stain behaviour, and physics constants.
//    • Configs live in a flat array indexed by BloodEventType.
//      Adding a new event type = add an enum value + call SetConfig().
//    • BloodEmitParams is the only caller-facing struct; everything else
//      is internal.
//
//  Performance:
//    • Fixed-size ring-buffer pool (MAX_PARTICLES slots, overwrite oldest).
//    • Stain pool is also capped (MAX_STAINS, evict oldest).
//    • No heap allocation after construction.
//
// =============================================================================

// ---------------------------------------------------------------------------
//  BloodEventType
//  Add new values freely — then register a BloodConfig via SetConfig().
// ---------------------------------------------------------------------------
enum class BloodEventType : int {
    EnemyHit        = 0,   // enemy wounded but survives
    EnemyDeath      = 1,   // enemy dies (any cause)
    EnemyStomp      = 2,   // player lands on enemy from above
    EnemySlash      = 3,   // sword swipe connects
    PlayerHit       = 4,   // player takes a hit
    PlayerDeath     = 5,   // player dies — maximum carnage
    BulletHitEnemy  = 6,   // bullet wounds an enemy
    BulletKillEnemy = 7,   // bullet kills an enemy
    Count           = 8    // sentinel — keep last
};

// ---------------------------------------------------------------------------
//  BloodConfig
//  All visual + physics parameters for one event type.
//  Every field has a sensible default so partial overrides are safe.
// ---------------------------------------------------------------------------
struct BloodConfig {
    // Particle count (scaled by BloodEmitParams::intensity)
    int   minParticles   =  8;
    int   maxParticles   = 16;

    // Launch speed (px / s)
    float minSpeed       = 100.f;
    float maxSpeed       = 300.f;

    // Lifetime (seconds)
    float minLife        = 0.35f;
    float maxLife        = 0.85f;

    // Rendered square size (px)
    float minSize        = 2.f;
    float maxSize        = 6.f;

    // Full cone spread angle (degrees).
    // 360 = omnidirectional burst; 90 = tight arc in dirX/Y.
    float spreadAngleDeg = 180.f;

    // Downward acceleration (px/s²)
    float gravity        = 580.f;

    // Velocity retained per logical 60 fps tick.
    // 0.98 feels slightly sticky; 0.96 feels airy.
    float drag           = 0.978f;

    // Colour lerp endpoints (randomised per particle)
    SDL_Color colorA     = {190,  5,  5, 255};   // deep red
    SDL_Color colorB     = {255, 45, 45, 255};   // bright scarlet

    // Stain settings
    bool  leaveStains    = true;
    float stainLife      = 5.f;        // how long the blob persists (seconds)
    float stainWidthMul  = 1.8f;       // stain width  = particle size * mul
    float stainHeightMul = 0.7f;       // stain height = particle size * mul (flattened)
};

// ---------------------------------------------------------------------------
//  BloodEmitParams
//  Everything a caller needs to supply when triggering blood.
// ---------------------------------------------------------------------------
struct BloodEmitParams {
    float          x         = 0.f;
    float          y         = 0.f;
    float          dirX      = 0.f;   // dominant spray direction (need not be normalised)
    float          dirY      = -1.f;  // default: upward squirt
    BloodEventType type      = BloodEventType::EnemyHit;
    float          intensity = 1.f;   // scales particle count (0.5 = half, 2.0 = double)
    float          speedMul  = 1.f;   // scales launch speed — higher = particles travel farther
};

// ---------------------------------------------------------------------------
//  Internal: BloodParticle
// ---------------------------------------------------------------------------
struct BloodParticle {
    float x, y;
    float vx, vy;
    float life, maxLife;
    float size;
    float gravity;
    float drag;
    float stainLife;
    float stainWidthMul;
    float stainHeightMul;
    Uint8 r, g, b;
    bool  alive      = false;
    bool  leaveStain = false;
    bool  grounded   = false;  // true after snapping to a tile surface
};

// ---------------------------------------------------------------------------
//  Internal: BloodStain
// ---------------------------------------------------------------------------
struct BloodStain {
    float x, y;
    float life, maxLife;
    float w, h;
    Uint8 r, g, b;
};

// =============================================================================
//  TileSurfaceQuery
//
//  Callback supplied by GameScene so the particle system can resolve tile
//  surfaces without taking an ECS dependency.
//
//  Signature:
//    bool query(float px, float pyBottom, float& outTileTopY)
//
//  The caller checks whether the point (px, pyBottom) — the particle's bottom
//  centre in world space — is inside a solid tile.  If so it sets outTileTopY
//  to the Y coordinate of that tile's top surface and returns true.
//  The particle system uses this to snap falling particles onto tile tops.
// =============================================================================
using TileSurfaceQuery = std::function<bool(float px, float pyBottom, float& outTileTopY)>;

// =============================================================================
//  TileMoveQuery
//
//  Optional second callback that lets stains and grounded particles "ride"
//  moving platforms.  GameScene queries its spatial grid for any moving
//  platform tile at (probeX, probeY) and fills (dvx, dvy) with the
//  world-space displacement that tile moved THIS frame.  Returning true means
//  a moving platform was found and the output values are valid.
//
//  Signature:
//    bool query(float probeX, float probeY, float& dvx, float& dvy)
// =============================================================================
using TileMoveQuery = std::function<bool(float probeX, float probeY, float& dvx, float& dvy)>;

// =============================================================================
//  BloodParticleSystem
// =============================================================================
class BloodParticleSystem {
public:
    static constexpr int MAX_PARTICLES = 2048;
    static constexpr int MAX_STAINS    =  512;

    BloodParticleSystem() {
        mParticles.resize(MAX_PARTICLES);
        mStains.reserve(MAX_STAINS);
        BuildDefaultConfigs();
    }

    // -------------------------------------------------------------------------
    //  Configuration API — call at any point to change how an event looks.
    // -------------------------------------------------------------------------
    void SetConfig(BloodEventType type, const BloodConfig& cfg) {
        mConfigs[static_cast<int>(type)] = cfg;
    }

    const BloodConfig& GetConfig(BloodEventType type) const {
        return mConfigs[static_cast<int>(type)];
    }

    // -------------------------------------------------------------------------
    //  Emit — spawns particles for one hit event.
    // -------------------------------------------------------------------------
    void Emit(const BloodEmitParams& params) {
        const BloodConfig& cfg = mConfigs[static_cast<int>(params.type)];

        // Normalise direction
        float dx = params.dirX, dy = params.dirY;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) { dx = 0.f; dy = -1.f; }
        else              { dx /= len; dy /= len; }

        float baseAngle  = std::atan2(dy, dx);
        float halfSpread = cfg.spreadAngleDeg * 0.5f * (3.14159265f / 180.f);

        // Particle count, clamped so we can never overflow the pool in one burst
        int count = static_cast<int>(
            RandRange(static_cast<float>(cfg.minParticles),
                      static_cast<float>(cfg.maxParticles)) * params.intensity + 0.5f);
        count = std::clamp(count, 1, MAX_PARTICLES / 4);

        for (int i = 0; i < count; ++i) {
            BloodParticle& p = AllocParticle();

            float angle = baseAngle + RandRange(-halfSpread, halfSpread);
            float speed = RandRange(cfg.minSpeed, cfg.maxSpeed) * params.speedMul;

            p.x            = params.x + RandRange(-2.f, 2.f);
            p.y            = params.y + RandRange(-2.f, 2.f);
            p.vx           = std::cos(angle) * speed;
            p.vy           = std::sin(angle) * speed;
            p.life         = RandRange(cfg.minLife, cfg.maxLife);
            p.maxLife      = p.life;
            p.size         = RandRange(cfg.minSize, cfg.maxSize);
            p.gravity      = cfg.gravity;
            p.drag         = cfg.drag;
            p.leaveStain   = cfg.leaveStains;
            p.stainLife    = cfg.stainLife;
            p.stainWidthMul  = cfg.stainWidthMul;
            p.stainHeightMul = cfg.stainHeightMul;
            p.alive        = true;
            p.grounded     = false;

            // Per-particle colour variation
            float t = RandFloat();
            p.r = Lerp8(cfg.colorA.r, cfg.colorB.r, t);
            p.g = Lerp8(cfg.colorA.g, cfg.colorB.g, t);
            p.b = Lerp8(cfg.colorA.b, cfg.colorB.b, t);
        }
    }

    // -------------------------------------------------------------------------
    //  Update — advance physics for all live particles and stains.
    //
    //  tileQuery (optional): supply a TileSurfaceQuery so particles collide
    //  with solid tiles instead of passing through them.  When a falling
    //  particle's bottom centre enters a tile the particle is snapped to the
    //  tile's top surface, friction is applied, and a stain is spawned
    //  immediately at the landing position.
    // -------------------------------------------------------------------------
    void Update(float dt,
                const TileSurfaceQuery& tileQuery    = nullptr,
                const TileMoveQuery&    tileMoveQuery = nullptr) {
        // Tick stains — fade out and, if on a moving platform, follow it.
        for (auto it = mStains.begin(); it != mStains.end(); ) {
            it->life -= dt;
            if (it->life <= 0.f) {
                it = mStains.erase(it);
            } else {
                // Ride moving platforms: probe just below the stain's bottom edge.
                if (tileMoveQuery) {
                    float dvx = 0.f, dvy = 0.f;
                    float probeX = it->x + it->w * 0.5f;
                    float probeY = it->y + it->h + 2.f;
                    if (tileMoveQuery(probeX, probeY, dvx, dvy)) {
                        it->x += dvx;
                        it->y += dvy;
                    }
                }
                ++it;
            }
        }

        // Tick particles
        for (auto& p : mParticles) {
            if (!p.alive) continue;

            p.life -= dt;
            if (p.life <= 0.f) {
                // Lifetime expired in mid-air — only spawn a stain when there
                // is no tile query (i.e. the caller doesn't want floor snapping).
                // When tile snapping is active, stains are spawned on landing.
                if (p.leaveStain && !tileQuery)
                    SpawnStain(p);
                p.alive = false;
                continue;
            }

            // Drag — linear approximation of pow(drag, dt*60), avoids std::pow.
            float retain = 1.f - (1.f - p.drag) * std::min(dt * 60.f, 2.f);
            p.vx *= retain;
            p.vy *= retain;

            // Gravity — only pull down while airborne (grounded flag set below).
            if (!p.grounded)
                p.vy += p.gravity * dt;

            // Integrate position
            p.x += p.vx * dt;
            p.y += p.vy * dt;

            // ---- Tile surface collision ----
            if (tileQuery && !p.grounded && p.vy >= 0.f) {
                float halfSize   = p.size * 0.5f;
                float pyBottom   = p.y + halfSize;
                float tileTopY   = 0.f;

                if (tileQuery(p.x, pyBottom, tileTopY)) {
                    // Snap particle to tile surface
                    p.y      = tileTopY - halfSize;
                    p.vy     = 0.f;
                    p.vx    *= 0.15f;   // ground friction kills horizontal momentum
                    p.grounded = true;

                    // Spawn a landing stain immediately — wider and flatter
                    // than a mid-air stain to look like a splat on the floor.
                    if (p.leaveStain) {
                        SpawnLandingStain(p);
                        p.leaveStain = false;  // prevent duplicate on lifetime end
                    }

                    // Bleed out remaining life quickly; grounded particles just
                    // sit there and fade, they don't need a long lifetime.
                    p.life = std::min(p.life, 0.25f);
                }
            }

            // ---- Moving-platform carry (grounded particles only) ----
            // Probe the point just below the particle; if it sits on a moving
            // tile, shift the particle by that tile's frame displacement so it
            // slides along with the platform rather than hanging in mid-air.
            if (p.grounded && tileMoveQuery) {
                float dvx = 0.f, dvy = 0.f;
                float probeY = p.y + p.size * 0.5f + 2.f;
                if (tileMoveQuery(p.x, probeY, dvx, dvy)) {
                    p.x += dvx;
                    p.y += dvy;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    //  Render — draw stains (behind) then particles (front).
    //  Call after RenderSystem, before the HUD.
    // -------------------------------------------------------------------------
    void Render(SDL_Renderer* ren, float camX, float camY) const {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        // ---- Stains (flat blobs, drawn first so particles sit on top) ----
        for (const auto& s : mStains) {
            float frac = s.life / s.maxLife;
            // Fade in sharply then linger; gives a "wet then drying" look.
            float alpha = frac < 0.15f
                          ? (frac / 0.15f) * 190.f
                          : 190.f * frac;
            Uint8 a = static_cast<Uint8>(std::clamp(alpha, 0.f, 190.f));
            SDL_SetRenderDrawColor(ren, s.r, s.g, s.b, a);
            SDL_FRect dst = {
                s.x - s.w * 0.5f - camX,
                s.y - s.h * 0.5f - camY,
                s.w, s.h
            };
            SDL_RenderFillRect(ren, &dst);
        }

        // ---- Particles ----
        for (const auto& p : mParticles) {
            if (!p.alive) continue;

            float frac = p.life / p.maxLife;
            Uint8  a   = static_cast<Uint8>(frac * 255.f);
            SDL_SetRenderDrawColor(ren, p.r, p.g, p.b, a);

            // Slightly elongate particle in the direction of travel
            // for a Nidhogg-style "streak" effect at high speed.
            float spd   = std::sqrt(p.vx * p.vx + p.vy * p.vy);
            float stretchW = p.size;
            float stretchH = p.size;
            if (spd > 80.f) {
                float sFactor = std::min(1.f + spd / 400.f, 3.f);
                // Elongate along dominant axis
                if (std::abs(p.vx) >= std::abs(p.vy))
                    stretchW = p.size * sFactor;
                else
                    stretchH = p.size * sFactor;
            }

            SDL_FRect dst = {
                p.x - stretchW * 0.5f - camX,
                p.y - stretchH * 0.5f - camY,
                stretchW, stretchH
            };
            SDL_RenderFillRect(ren, &dst);
        }
    }

    // -------------------------------------------------------------------------
    //  Clear — remove all particles and stains (e.g. on level reload).
    // -------------------------------------------------------------------------
    void Clear() {
        for (auto& p : mParticles) p.alive = false;
        mStains.clear();
        mNextSlot = 0;
    }

    // -------------------------------------------------------------------------
    //  Stats (optional — useful for debug overlays)
    // -------------------------------------------------------------------------
    int ActiveParticleCount() const {
        int n = 0;
        for (const auto& p : mParticles) n += p.alive ? 1 : 0;
        return n;
    }
    int ActiveStainCount() const { return static_cast<int>(mStains.size()); }

private:
    // ---- Storage -----------------------------------------------------------
    std::array<BloodConfig, static_cast<int>(BloodEventType::Count)> mConfigs{};
    std::vector<BloodParticle> mParticles;
    std::vector<BloodStain>    mStains;
    int                        mNextSlot = 0;

    // ---- Ring-buffer allocation --------------------------------------------
    BloodParticle& AllocParticle() {
        // Walk forward from mNextSlot to find a dead slot; if none, overwrite.
        int start = mNextSlot;
        do {
            BloodParticle& p = mParticles[mNextSlot];
            mNextSlot = (mNextSlot + 1) % MAX_PARTICLES;
            if (!p.alive) return p;
        } while (mNextSlot != start);
        // Pool is full — overwrite the current slot (oldest approximate).
        BloodParticle& p = mParticles[mNextSlot];
        mNextSlot = (mNextSlot + 1) % MAX_PARTICLES;
        return p;
    }

    // ---- Stain spawning ----------------------------------------------------

    // Mid-air lifetime expiry stain (smaller, used when no tile query is active)
    void SpawnStain(const BloodParticle& p) {
        SpawnStainInternal(p, p.stainWidthMul, p.stainHeightMul, p.stainLife);
    }

    // Landing stain — wider and flatter to look like a floor splat
    void SpawnLandingStain(const BloodParticle& p) {
        // Landing splat: 2.5× wider, 0.4× height (very flat), longer life
        SpawnStainInternal(p, p.stainWidthMul * 2.5f, p.stainHeightMul * 0.4f,
                           p.stainLife * 1.4f);
    }

    void SpawnStainInternal(const BloodParticle& p, float wMul, float hMul, float life) {
        if (static_cast<int>(mStains.size()) >= MAX_STAINS)
            mStains.erase(mStains.begin());  // evict oldest

        BloodStain s;
        s.x       = p.x + RandRange(-2.f, 2.f);
        s.y       = p.y;
        s.life    = life;
        s.maxLife = life;
        s.w       = std::max(p.size * wMul + RandRange(-1.f, 3.f), 1.f);
        s.h       = std::max(p.size * hMul + RandRange(-0.5f, 1.f), 1.f);
        // Stains slightly darker than the particle that made them
        s.r = static_cast<Uint8>(std::max(0, (int)p.r - 30));
        s.g = static_cast<Uint8>(std::max(0, (int)p.g -  5));
        s.b = static_cast<Uint8>(std::max(0, (int)p.b -  5));
        mStains.push_back(s);
    }

    // ---- Helpers -----------------------------------------------------------
    static float RandFloat() {
        return static_cast<float>(rand() % 100001) / 100000.f;
    }
    static float RandRange(float lo, float hi) {
        return lo + RandFloat() * (hi - lo);
    }
    static Uint8 Lerp8(Uint8 a, Uint8 b, float t) {
        return static_cast<Uint8>(static_cast<int>(a) +
               static_cast<int>(t * static_cast<float>(static_cast<int>(b) - static_cast<int>(a))));
    }

    // ---- Default configs for every event type ------------------------------
    void BuildDefaultConfigs() {
        using BET = BloodEventType;

        // ---- EnemyHit: modest spray, enemy takes damage but lives ----
        mConfigs[(int)BET::EnemyHit] = BloodConfig{
            .minParticles   =  6,
            .maxParticles   = 12,
            .minSpeed       =  80.f,
            .maxSpeed       = 240.f,
            .minLife        = 0.30f,
            .maxLife        = 0.70f,
            .minSize        =  2.f,
            .maxSize        =  5.f,
            .spreadAngleDeg = 150.f,
            .gravity        = 560.f,
            .drag           = 0.978f,
            .colorA         = {185,  6,  6, 255},
            .colorB         = {255, 38, 38, 255},
            .leaveStains    = true,
            .stainLife      = 4.f,
            .stainWidthMul  = 1.6f,
            .stainHeightMul = 0.65f,
        };

        // ---- EnemyDeath: full omnidirectional gore burst ----
        mConfigs[(int)BET::EnemyDeath] = BloodConfig{
            .minParticles   = 20,
            .maxParticles   = 32,
            .minSpeed       = 130.f,
            .maxSpeed       = 400.f,
            .minLife        =  0.5f,
            .maxLife        =  1.1f,
            .minSize        =  3.f,
            .maxSize        =  8.f,
            .spreadAngleDeg = 360.f,
            .gravity        = 580.f,
            .drag           = 0.974f,
            .colorA         = {160,  0,  0, 255},
            .colorB         = {255, 55, 55, 255},
            .leaveStains    = true,
            .stainLife      = 7.f,
            .stainWidthMul  = 2.2f,
            .stainHeightMul = 0.85f,
        };

        // ---- EnemyStomp: upward squirt from under the player's feet ----
        mConfigs[(int)BET::EnemyStomp] = BloodConfig{
            .minParticles   = 12,
            .maxParticles   = 20,
            .minSpeed       = 150.f,
            .maxSpeed       = 340.f,
            .minLife        = 0.40f,
            .maxLife        = 0.90f,
            .minSize        =  3.f,
            .maxSize        =  7.f,
            .spreadAngleDeg = 110.f,   // concentrated upward arc
            .gravity        = 640.f,
            .drag           = 0.970f,
            .colorA         = {180,  5,  5, 255},
            .colorB         = {255, 55, 55, 255},
            .leaveStains    = true,
            .stainLife      = 5.f,
            .stainWidthMul  = 1.9f,
            .stainHeightMul = 0.70f,
        };

        // ---- EnemySlash: directional arc along sword swing ----
        mConfigs[(int)BET::EnemySlash] = BloodConfig{
            .minParticles   =  8,
            .maxParticles   = 16,
            .minSpeed       = 160.f,
            .maxSpeed       = 360.f,
            .minLife        = 0.32f,
            .maxLife        = 0.72f,
            .minSize        =  2.f,
            .maxSize        =  6.f,
            .spreadAngleDeg =  80.f,   // tight cone — blood flies with the blade
            .gravity        = 520.f,
            .drag           = 0.972f,
            .colorA         = {195,  8,  8, 255},
            .colorB         = {255, 42, 42, 255},
            .leaveStains    = true,
            .stainLife      = 4.5f,
            .stainWidthMul  = 1.7f,
            .stainHeightMul = 0.60f,
        };

        // ---- PlayerHit: blood spurts off the player ----
        mConfigs[(int)BET::PlayerHit] = BloodConfig{
            .minParticles   =  8,
            .maxParticles   = 14,
            .minSpeed       = 100.f,
            .maxSpeed       = 270.f,
            .minLife        = 0.30f,
            .maxLife        = 0.75f,
            .minSize        =  2.f,
            .maxSize        =  5.f,
            .spreadAngleDeg = 160.f,
            .gravity        = 540.f,
            .drag           = 0.980f,
            .colorA         = {200,  5,  5, 255},
            .colorB         = {255, 30, 30, 255},
            .leaveStains    = true,
            .stainLife      = 4.f,
            .stainWidthMul  = 1.5f,
            .stainHeightMul = 0.60f,
        };

        // ---- PlayerDeath: maximum carnage — explosive omnidirectional burst ----
        mConfigs[(int)BET::PlayerDeath] = BloodConfig{
            .minParticles   = 32,
            .maxParticles   = 48,
            .minSpeed       =  80.f,
            .maxSpeed       = 450.f,
            .minLife        =  0.6f,
            .maxLife        =  1.5f,
            .minSize        =  3.f,
            .maxSize        = 10.f,
            .spreadAngleDeg = 360.f,
            .gravity        = 550.f,
            .drag           = 0.971f,
            .colorA         = {150,  0,  0, 255},
            .colorB         = {255, 65, 65, 255},
            .leaveStains    = true,
            .stainLife      = 12.f,
            .stainWidthMul  = 2.6f,
            .stainHeightMul = 1.0f,
        };

        // ---- BulletHitEnemy: quick small puff on bullet impact ----
        mConfigs[(int)BET::BulletHitEnemy] = BloodConfig{
            .minParticles   =  4,
            .maxParticles   =  8,
            .minSpeed       =  60.f,
            .maxSpeed       = 180.f,
            .minLife        = 0.22f,
            .maxLife        = 0.50f,
            .minSize        =  1.f,
            .maxSize        =  4.f,
            .spreadAngleDeg = 130.f,
            .gravity        = 500.f,
            .drag           = 0.982f,
            .colorA         = {185,  6,  6, 255},
            .colorB         = {245, 35, 35, 255},
            .leaveStains    = false,
            .stainLife      = 0.f,
            .stainWidthMul  = 0.f,
            .stainHeightMul = 0.f,
        };

        // ---- BulletKillEnemy: bigger burst when bullet kills ----
        mConfigs[(int)BET::BulletKillEnemy] = BloodConfig{
            .minParticles   = 14,
            .maxParticles   = 24,
            .minSpeed       = 110.f,
            .maxSpeed       = 340.f,
            .minLife        = 0.45f,
            .maxLife        = 1.00f,
            .minSize        =  2.f,
            .maxSize        =  7.f,
            .spreadAngleDeg = 200.f,
            .gravity        = 570.f,
            .drag           = 0.974f,
            .colorA         = {165,  3,  3, 255},
            .colorB         = {255, 50, 50, 255},
            .leaveStains    = true,
            .stainLife      = 6.f,
            .stainWidthMul  = 2.0f,
            .stainHeightMul = 0.75f,
        };
    }
};
