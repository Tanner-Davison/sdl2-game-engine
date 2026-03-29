// engine/SceneManager.hpp -- canonical location.
#pragma once
#include "Components.hpp"
#include "Scene.hpp"
#include "audio/AudioEngine.hpp"
#include <SDL3/SDL.h>
#include <engine/Scene.hpp>
#include <entt/entt.hpp>
#include <memory>

class Window;

class SceneManager {
  public:
    bool InitAudio() { return mAudio.Init(); }

    audio::AudioEngine& Audio() { return mAudio; }
    const audio::AudioEngine& Audio() const { return mAudio; }

    void SetScene(std::unique_ptr<Scene> scene, Window& window) {
        if (mCurrent)
            mCurrent->Unload();
        mCurrent = std::move(scene);
        mCurrent->SetAudio(&mAudio);
        mCurrent->Load(window);
    }

    bool HandleEvent(SDL_Event& e) {
        if (!mCurrent)
            return false;
        return mCurrent->HandleEvent(e);
    }

    // Snapshots PrevTransform then ticks the scene. Called once per fixed physics step.
    void Update(float dt, Window& window) {
        if (!mCurrent)
            return;

        entt::registry* reg = mCurrent->GetRegistry();
        if (reg) {
            auto view = reg->view<Transform, PrevTransform>();
            view.each([](const Transform& t, PrevTransform& p) {
                p.x = t.x;
                p.y = t.y;
            });
        }

        mCurrent->Update(dt);

        auto next = mCurrent->NextScene();
        if (next) {
            mCurrent->Unload();
            mCurrent = std::move(next);
            mCurrent->SetAudio(&mAudio);
            mCurrent->Load(window);
        }
    }

    void Render(Window& window, float alpha = 1.0f) {
        if (mCurrent)
            mCurrent->Render(window, alpha);
    }

    void Shutdown() {
        if (mCurrent) {
            mCurrent->Unload();
            mCurrent.reset();
        }
        mAudio.Shutdown();
    }

    bool ShouldQuit() const {
        return mCurrent ? mCurrent->ShouldQuit() : true;
    }

  private:
    std::unique_ptr<Scene> mCurrent;
    audio::AudioEngine     mAudio;
};
