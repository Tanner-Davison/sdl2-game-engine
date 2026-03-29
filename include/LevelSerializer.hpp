#pragma once
#include "LevelData.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <print>
#include <string>

using json = nlohmann::json;

inline bool SaveLevel(const Level& level, const std::string& path) {
    json j;
    j["name"]        = level.name;
    j["background"]  = level.background;
    j["bgFitMode"]   = level.bgFitMode;
    j["bgRepeat"]    = level.bgRepeat;
    j["gravityMode"] = (level.gravityMode == GravityMode::WallRun)  ? "wallrun"
                      : (level.gravityMode == GravityMode::OpenWorld) ? "openworld"
                                                                       : "platformer";
    j["player"]      = {{"x", level.player.x}, {"y", level.player.y}};

    // Audio
    if (!level.musicPath.empty()) {
        j["music"]       = level.musicPath;
        j["musicVolume"] = level.musicVolume;
    }

    j["enemies"] = json::array();
    for (const auto& e : level.enemies) {
        json ej = {{"x", e.x}, {"y", e.y}, {"speed", e.speed},
                   {"antiGravity", e.antiGravity},
                   {"startLeft", e.startLeft}};
        if (!e.enemyType.empty())
            ej["enemyType"] = e.enemyType;
        j["enemies"].push_back(std::move(ej));
    }

    j["parallaxLayers"] = json::array();
    for (const auto& pl : level.parallaxLayers) {
        j["parallaxLayers"].push_back({
            {"img",          pl.imagePath},
            {"scrollFactor", pl.scrollFactor},
            {"yOffset",      pl.yOffset},
        });
    }

    j["tiles"] = json::array();
    for (const auto& t : level.tiles) {
        // Slope
        std::string slopeStr = "none";
        SlopeType   slopeType = t.GetSlopeType();
        if (slopeType == SlopeType::DiagUpRight) slopeStr = "diagupright";
        if (slopeType == SlopeType::DiagUpLeft)  slopeStr = "diagupleft";

        json tile = {
            {"x", t.x}, {"y", t.y}, {"w", t.w}, {"h", t.h},
            {"img",         t.imagePath},
            {"rotation",    t.rotation},
            {"prop",        t.prop},
            {"ladder",      t.ladder},
            {"hazard",      t.hazard},
            {"antiGravity", t.antiGravity},
            {"goal",        t.goal},
            // Action
            {"action",           t.HasAction()},
            {"actionGroup",      t.HasAction() ? t.action->group          : 0},
            {"actionHits",       t.HasAction() ? t.action->hitsRequired   : 1},
            {"actionDestroyAnim",t.HasAction() ? t.action->destroyAnimPath : std::string{}},
            {"actionCamShake",   t.HasAction() ? t.action->cameraShake    : false},
            // Slope
            {"slope",            slopeStr},
            {"slopeHeightFrac",  t.HasSlope() ? t.slope->heightFrac : 1.0f},
            // Hitbox
            {"hitboxOffX",  t.HasHitbox() ? t.hitbox->offX : 0},
            {"hitboxOffY",  t.HasHitbox() ? t.hitbox->offY : 0},
            {"hitboxW",     t.HasHitbox() ? t.hitbox->w    : 0},
            {"hitboxH",     t.HasHitbox() ? t.hitbox->h    : 0},
            // Moving platform
            {"moving",      t.HasMoving()},
            {"moveHoriz",   t.HasMoving() ? t.moving->horiz   : true},
            {"moveRange",   t.HasMoving() ? t.moving->range   : 96.0f},
            {"moveSpeed",   t.HasMoving() ? t.moving->speed   : 60.0f},
            {"moveGroupId", t.HasMoving() ? t.moving->groupId : 0},
            {"moveLoop",    t.HasMoving() ? t.moving->loop    : false},
            {"moveTrigger", t.HasMoving() ? t.moving->trigger : false},
            {"movePhase",   t.HasMoving() ? t.moving->phase   : 0.0f},
            {"moveLoopDir", t.HasMoving() ? t.moving->loopDir : 1},
            // Power-up
            {"powerUp",         t.HasPowerUp()},
            {"powerUpType",     t.HasPowerUp() ? t.powerUp->type     : std::string{}},
            {"powerUpDuration", t.HasPowerUp() ? t.powerUp->duration : 15.0f},
            {"powerUpFireRate", t.HasPowerUp() ? t.powerUp->fireRate : 3.0f},
            {"powerUpSfx",      t.HasPowerUp() ? t.powerUp->sfxPath  : std::string{}},
            // Shooter
            {"shooter",         t.HasShooter()},
            {"shooterSide",     t.HasShooter() ? static_cast<int>(t.shooter->side) : 0},
            {"shooterRange",    t.HasShooter() ? t.shooter->range       : 300.0f},
            {"shooterFireRate", t.HasShooter() ? t.shooter->fireRate    : 1.5f},
            {"shooterBulletSpd",t.HasShooter() ? t.shooter->bulletSpeed : 200.0f},
            {"shooterDamage",   t.HasShooter() ? t.shooter->damage      : 10.0f},
            {"shooterSfx",     t.HasShooter() ? t.shooter->sfxPath    : ""},
            // Shield
            {"shield",          t.HasShield()},
            {"shieldDuration",  t.HasShield() ? t.shield->duration : 20.0f},
        };
        j["tiles"].push_back(std::move(tile));
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        std::print("Failed to save level: {}\n", path);
        return false;
    }
    file << j.dump(4);
    std::print("Level saved: {}\n", path);
    return true;
}

inline bool LoadLevel(const std::string& path, Level& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::print("Failed to load level: {}\n", path);
        return false;
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        std::print("JSON parse error in {}: {}\n", path, e.what());
        return false;
    }

    out.name        = j.value("name", "Untitled");
    out.background  = j.value("background", "game_assets/backgrounds/deepspace_scene.png");
    out.bgFitMode   = j.value("bgFitMode", "cover");
    out.bgRepeat    = j.value("bgRepeat", false);
    {
        std::string gm = j.value("gravityMode", "platformer");
        out.gravityMode = (gm == "wallrun")   ? GravityMode::WallRun
                        : (gm == "openworld") ? GravityMode::OpenWorld
                                              : GravityMode::Platformer;
    }

    if (j.contains("player")) {
        out.player.x = j["player"].value("x", 0.0f);
        out.player.y = j["player"].value("y", 0.0f);
    }

    out.enemies.clear();
    for (const auto& e : j.value("enemies", json::array())) {
        EnemySpawn es;
        es.x           = e.value("x", 0.0f);
        es.y           = e.value("y", 0.0f);
        es.speed       = e.value("speed", 120.0f);
        es.antiGravity = e.value("antiGravity", false);
        es.startLeft   = e.value("startLeft", false);
        es.enemyType   = e.value("enemyType", std::string{});
        out.enemies.push_back(std::move(es));
    }

    // Audio
    out.musicPath   = j.value("music", std::string{});
    out.musicVolume = j.value("musicVolume", 1.0f);

    out.parallaxLayers.clear();
    for (const auto& pl : j.value("parallaxLayers", json::array())) {
        ParallaxLayer layer;
        layer.imagePath    = pl.value("img", std::string{});
        layer.scrollFactor = pl.value("scrollFactor", 0.5f);
        layer.yOffset      = pl.value("yOffset", 0.0f);
        if (!layer.imagePath.empty())
            out.parallaxLayers.push_back(std::move(layer));
    }

    out.tiles.clear();
    for (const auto& t : j.value("tiles", json::array())) {
        TileSpawn ts;
        ts.x          = t.value("x", 0.0f);
        ts.y          = t.value("y", 0.0f);
        ts.w          = t.value("w", 40);
        ts.h          = t.value("h", 40);
        ts.imagePath  = t.value("img", std::string{});
        ts.rotation   = t.value("rotation", 0);
        ts.prop       = t.value("prop", false);
        ts.ladder     = t.value("ladder", false);
        ts.hazard     = t.value("hazard", false);
        ts.antiGravity = t.value("antiGravity", false);
        ts.goal        = t.value("goal", false);

        // Action
        if (t.value("action", false)) {
            ActionData ad;
            ad.group          = t.value("actionGroup", 0);
            ad.hitsRequired   = t.value("actionHits", 1);
            ad.destroyAnimPath = t.value("actionDestroyAnim", std::string{});
            ad.cameraShake     = t.value("actionCamShake", false);
            ts.action = ad;
        }

        // Slope
        {
            std::string slopeStr = t.value("slope", std::string{"none"});
            SlopeType slopeType = SlopeType::None;
            if (slopeStr == "diagupright") slopeType = SlopeType::DiagUpRight;
            if (slopeStr == "diagupleft")  slopeType = SlopeType::DiagUpLeft;
            if (slopeType != SlopeType::None) {
                SlopeData sd;
                sd.type       = slopeType;
                sd.heightFrac = t.value("slopeHeightFrac", 1.0f);
                ts.slope = sd;
            }
        }

        // Hitbox
        {
            int hbOffX = t.value("hitboxOffX", 0);
            int hbOffY = t.value("hitboxOffY", 0);
            int hbW    = t.value("hitboxW", 0);
            int hbH    = t.value("hitboxH", 0);
            if (hbW > 0 || hbH > 0) {
                ts.hitbox = HitboxData{hbOffX, hbOffY, hbW, hbH};
            }
        }

        // Moving platform
        if (t.value("moving", false)) {
            MovingPlatformData mp;
            mp.horiz   = t.value("moveHoriz", true);
            mp.range   = t.value("moveRange", 96.0f);
            mp.speed   = t.value("moveSpeed", 60.0f);
            mp.groupId = t.value("moveGroupId", 0);
            mp.loop    = t.value("moveLoop", false);
            mp.trigger = t.value("moveTrigger", false);
            mp.phase   = t.value("movePhase", 0.0f);
            mp.loopDir = t.value("moveLoopDir", 1);
            ts.moving = mp;
        }

        // Power-up
        if (t.value("powerUp", false)) {
            PowerUpData pu;
            pu.type     = t.value("powerUpType", std::string{});
            pu.duration = t.value("powerUpDuration", 15.0f);
            pu.fireRate = t.value("powerUpFireRate", 3.0f);
            pu.sfxPath  = t.value("powerUpSfx", std::string{});
            ts.powerUp = pu;
        }

        // Shooter
        if (t.value("shooter", false)) {
            ShooterData sd;
            sd.side        = static_cast<ShooterSide>(t.value("shooterSide", 0));
            sd.range       = t.value("shooterRange", 300.0f);
            sd.fireRate    = t.value("shooterFireRate", 1.5f);
            sd.bulletSpeed = t.value("shooterBulletSpd", 200.0f);
            sd.damage      = t.value("shooterDamage", 10.0f);
            sd.sfxPath     = t.value("shooterSfx", std::string(""));
            ts.shooter = sd;
        }

        // Shield
        if (t.value("shield", false)) {
            ShieldData sd;
            sd.duration = t.value("shieldDuration", 20.0f);
            ts.shield = sd;
        }

        out.tiles.push_back(std::move(ts));
    }

    std::print("Level loaded: {} ({} enemies, {} tiles)\n",
               out.name, out.enemies.size(), out.tiles.size());
    return true;
}
