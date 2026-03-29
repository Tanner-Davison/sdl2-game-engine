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

    virtual void Load(Window& window) = 0;
    virtual void Unload() = 0;
    virtual bool HandleEvent(SDL_Event& e) = 0;
    virtual void Update(float dt) = 0;
    virtual void Render(Window& window, float alpha = 1.0f) = 0;

    virtual entt::registry* GetRegistry() { return nullptr; }
    virtual std::unique_ptr<Scene> NextScene() { return nullptr; }
    virtual bool ShouldQuit() const { return false; }

    void SetAudio(audio::AudioEngine* engine) { mAudio = engine; }
    audio::AudioEngine* Audio() const { return mAudio; }

  private:
    audio::AudioEngine* mAudio = nullptr;
};
