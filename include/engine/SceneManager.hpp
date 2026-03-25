// engine/SceneManager.hpp -- canonical location.
#pragma once
#include "Components.hpp"
#include "Scene.hpp"
#include "audio/AudioEngine.hpp"
#include <SDL3/SDL.h>
#include <engine/Scene.hpp>
#include <entt/entt.hpp>
#include <memory>

// Forward declare to avoid pulling in Window.hpp here unnecessarily
class Window;

class SceneManager {
  public:
    // ── Audio engine lifecycle ────────────────────────────────────────────
    // SceneManager owns the AudioEngine. main() calls InitAudio() once
    // after construction, and ShutdownAudio() is called in Shutdown().

    /// Open the audio device. Call once in main() after SDL_Init.
    bool InitAudio() { return mAudio.Init(); }

    /// Direct access to the audio engine (e.g. for main() to pre-load SFX).
    audio::AudioEngine& Audio() { return mAudio; }
    const audio::AudioEngine& Audio() const { return mAudio; }

    // ── Scene management ─────────────────────────────────────────────────

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

    // Snapshot PrevTransform for every entity that has both Transform and
    // PrevTransform, then tick the scene by exactly dt seconds.
    // Called once per fixed physics step from main's accumulator loop.
    void Update(float dt, Window& window) {
        if (!mCurrent)
            return;

        // Snapshot current positions into PrevTransform before the tick.
        // RenderSystem uses these to interpolate the draw position between
        // physics steps, giving smooth motion at any render frame rate.
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

    // alpha: sub-step interpolation factor in [0, 1).
    // Pass (accumulator / FIXED_DT) from the main loop so RenderSystem can
    // lerp between PrevTransform and Transform for perfectly smooth motion.
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
