
#pragma once
#include "Scene.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <memory>

class LevelThree : public Scene {
  public:
    void Load(Window& window) override {
        mWindow = &window;
        // TODO: build out level two
        comingSoonText = std::make_unique<Text>("Level 3 - Coming Soon!",
                                                SDL_Color{255, 255, 255, 255},
                                                window.GetWidth() / 2 - 160,
                                                window.GetHeight() / 2 - 20,
                                                48);
    }

    void Unload() override {
        mWindow = nullptr;
    }

    bool HandleEvent(SDL_Event& e) override {
        if (e.type == SDL_EVENT_QUIT)
            return false;
        return true;
    }

    void Update(float dt) override {}

    void Render(Window& window) override {
        window.Render();
        if (comingSoonText)
            comingSoonText->Render(window.GetRenderer());
        window.Update();
    }

  private:
    Window*               mWindow = nullptr;
    std::unique_ptr<Text> comingSoonText;
};
