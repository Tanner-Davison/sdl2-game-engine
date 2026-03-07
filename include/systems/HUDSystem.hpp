#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <Text.hpp>
#include <cmath>
#include <entt/entt.hpp>
#include <memory>
#include <string>

inline void HUDSystem(entt::registry& reg,
                      SDL_Renderer*   renderer,
                      int             windowW,
                      Text*           healthText,
                      Text*           gravityText,
                      Text*           coinText,
                      int             coinCount,
                      Text*           stompText,
                      int             stompCount) {
    static int prevHealth   = -1;
    static int prevCoin     = -1;
    static int prevStomp    = -1;
    static int prevGravSecs = -1;

    auto view = reg.view<PlayerTag, Health, GravityState>();
    view.each([&](const Health& h, const GravityState& g) {
        constexpr int barW = 200;
        constexpr int barH = 15;
        const int     barX = windowW - barW - 20;
        constexpr int barY = 20;

        // Health bar background
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_FRect bg = {(float)barX, (float)barY, (float)barW, (float)barH};
        SDL_RenderFillRect(renderer, &bg);

        // Health bar fill
        int   fillW = (int)(barW * (h.current / h.max));
        float pct   = h.current / h.max;
        Uint8 red   = (Uint8)(255 * (1.0f - pct));
        Uint8 green = (Uint8)(255 * pct);
        SDL_SetRenderDrawColor(renderer, red, green, 0, 255);
        SDL_FRect fg = {(float)barX, (float)barY, (float)fillW, (float)barH};
        SDL_RenderFillRect(renderer, &fg);

        // Health label
        int curHealth = (int)h.current;
        if (curHealth != prevHealth) {
            std::string label = std::to_string(curHealth) + " / " +
                                std::to_string((int)h.max);
            healthText->SetPosition(barX, barY - 20);
            healthText->CreateSurface(label);
            prevHealth = curHealth;
        }
        if (healthText) healthText->Render(renderer);

        if (coinText) {
            if (coinCount != prevCoin) {
                coinText->SetPosition(barX, barY + barH + 10);
                coinText->CreateSurface("Gold Collected: " + std::to_string(coinCount));
                prevCoin = coinCount;
            }
            coinText->Render(renderer);
        }

        if (stompText) {
            if (stompCount != prevStomp) {
                stompText->SetPosition(barX, barY + barH + 30);
                stompText->CreateSurface("Enemies Stomped: " + std::to_string(stompCount));
                prevStomp = stompCount;
            }
            stompText->Render(renderer);
        }

        if (g.punishmentTimer > 0.0f && gravityText) {
            int secs = (int)std::ceil(g.punishmentTimer);
            if (secs != prevGravSecs) {
                gravityText->SetPosition(windowW / 2 - 160, 20);
                gravityText->CreateSurface("Zero Gravity Activated for " +
                                           std::to_string(secs) + " s");
                prevGravSecs = secs;
            }
            gravityText->Render(renderer);
        } else {
            prevGravSecs = -1;
        }
    });

    // ── Active power-up HUD ─────────────────────────────────────────────────
    // Shows one progress bar per active power-up type, stacked vertically.
    {
        constexpr int BAR_W = 200, BAR_H = 14, BAR_GAP = 22;
        int bx = windowW / 2 - BAR_W / 2;
        int by = 50;

        auto puv = reg.view<PlayerTag, ActivePowerUps>();
        puv.each([&](const ActivePowerUps& aps) {
            for (auto& [key, slot] : aps.slots) {
                auto type = (PowerUpType)key;

                // Background
                SDL_SetRenderDrawColor(renderer, 30, 30, 50, 220);
                SDL_FRect bg = {(float)bx, (float)by, (float)BAR_W, (float)BAR_H};
                SDL_RenderFillRect(renderer, &bg);

                // Fill
                float pct   = slot.duration > 0.f ? (slot.remaining / slot.duration) : 0.f;
                int   fillW = (int)(BAR_W * pct);
                SDL_SetRenderDrawColor(renderer, 160, 40, 255, 240);
                SDL_FRect fg = {(float)bx, (float)by, (float)fillW, (float)BAR_H};
                SDL_RenderFillRect(renderer, &fg);

                // Border
                SDL_SetRenderDrawColor(renderer, 200, 100, 255, 255);
                SDL_FRect br = {(float)bx, (float)by, (float)BAR_W, (float)BAR_H};
                SDL_RenderRect(renderer, &br);

                // Label
                int secs = (int)std::ceil(slot.remaining);
                std::string name = (type == PowerUpType::AntiGravity) ? "Anti-Gravity" : "Power-Up";
                Text lbl(name + "  " + std::to_string(secs) + "s",
                         SDL_Color{230, 180, 255, 255}, bx, by - 18, 13);
                lbl.Render(renderer);

                by += BAR_H + BAR_GAP;
            }
        });
    }

}
