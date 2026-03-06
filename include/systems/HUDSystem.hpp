#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <Text.hpp>
#include <cmath>
#include <entt/entt.hpp>
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
}
