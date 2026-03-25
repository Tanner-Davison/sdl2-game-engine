// engine/Scene.hpp -- canonical location.
// The root include/Scene.hpp forwards here for backward compatibility.
#pragma once
#include <SDL3/SDL.h>
#include <entt/entt.hpp>
#include <memory>

class Window;
namespace audio { class AudioEngine; }

class Scene {
  public:
    virtual ~Scene() = default;

    // Called once when the scene becomes active
    virtual void Load(Window& window) = 0;

    // Called once when the scene is replaced or popped
    virtual void Unload() = 0;

    // Handle a single SDL event -- return false to quit the app
    virtual bool HandleEvent(SDL_Event& e) = 0;

    // Update game logic (fixed dt from accumulator loop)
    virtual void Update(float dt) = 0;

    // Render to window.
    // alpha: sub-step interpolation factor in [0,1) -- passed down to
    // RenderSystem so it can lerp between PrevTransform and Transform.
    virtual void Render(Window& window, float alpha = 1.0f) = 0;

    // Returns a pointer to this scene's EnTT registry, or nullptr if the
    // scene has no registry (UI-only scenes, title screen, etc.).
    // SceneManager uses this to snapshot PrevTransform before each tick.
    virtual entt::registry* GetRegistry() { return nullptr; }

    // If this returns non-null, SceneManager will switch to it next frame
    virtual std::unique_ptr<Scene> NextScene() { return nullptr; }

    // If this returns true, SceneManager will quit the app
    virtual bool ShouldQuit() const { return false; }

    // ── Audio engine access ──────────────────────────────────────────────
    // Non-owning pointer injected by SceneManager before Load().
    // Scenes that don't use audio can ignore it (remains nullptr).
    void SetAudio(audio::AudioEngine* engine) { mAudio = engine; }
    audio::AudioEngine* Audio() const { return mAudio; }

  private:
    audio::AudioEngine* mAudio = nullptr;
};
