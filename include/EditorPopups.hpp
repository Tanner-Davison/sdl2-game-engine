#pragma once
// EditorPopups.hpp — state and event handling for every editor modal/popup:
// delete confirmation, import text input, destroy-anim picker, power-up picker,
// moving-platform config. Does NOT render; rendering is in EditorUIRenderer.

#include "AnimatedTile.hpp"
#include "EditorPalette.hpp"
#include "LevelData.hpp"
#include <SDL3/SDL.h>
#include <functional>
#include <string>
#include <vector>

class EditorPopups {
  public:
    struct AnimPickerEntry {
        std::string  path;
        std::string  name;
        SDL_Surface* thumb = nullptr;
    };

    struct PowerUpEntry {
        std::string id;
        std::string label;
        float       defaultDuration = 15.0f;
    };

    struct Ctx {
        Level&        level;
        EditorPalette& palette;

        std::function<void(const std::string&)> setStatus;
        std::function<void()>                   refreshTileView;
        std::function<void()>                   refreshBgPalette;
        std::function<bool(const std::string&)> importPath;
        std::function<SDL_Surface*(const std::string&)> getAnimThumb;

        SDL_Window* sdlWindow = nullptr;

        const char* tileRoot = "game_assets/tiles";
        const char* bgRoot   = "game_assets/backgrounds";
    };

    // --- Delete confirmation popup ---
    bool        delActive  = false;
    std::string delPath;
    bool        delIsDir   = false;
    std::string delName;
    SDL_Rect    delYes{};
    SDL_Rect    delNo{};

    void OpenDeleteConfirm(const std::string& path, bool isDir, const std::string& name);

    // --- Import text input modal ---
    bool        importActive = false;
    std::string importText;

    void OpenImportInput(bool isBgTab, Ctx& ctx);

    // --- Destroy-anim picker ---
    int                      animPickerTile = -1;
    SDL_Rect                 animPickerRect{};
    SDL_Rect                 camShakeToggleRect{};
    std::vector<AnimPickerEntry> animPickerEntries;

    void OpenAnimPicker(int tileIdx, Ctx& ctx);
    void CloseAnimPicker();

    // --- Power-up picker ---
    bool     powerUpOpen    = false;
    int      powerUpTileIdx = -1;
    SDL_Rect powerUpRect{};

    const std::vector<PowerUpEntry>* powerUpRegistry = nullptr;

    void OpenPowerUpPicker(int tileIdx, int screenX, int screenY,
                           int windowW, int windowH, int toolbarH);
    void ClosePowerUpPicker();

    // --- Moving-platform config popup ---
    bool        movPlatOpen       = false;
    bool        movPlatSpeedInput = false;
    std::string movPlatSpeedStr   = "60";
    SDL_Rect    movPlatRect{};

    // Shared with inline tool state in LevelEditorScene.
    // The orchestrator syncs these before HandleEvent() and reads back after.
    float movPlatSpeed   = 60.0f;
    bool  movPlatHoriz   = true;
    bool  movPlatLoop    = false;
    bool  movPlatTrigger = false;
    int   movPlatGroupId = 1;

    // Returns true if the event was fully consumed by a popup.
    bool HandleEvent(const SDL_Event& e,
                     Ctx&             ctx,
                     std::vector<int>& movPlatIndices);

    bool AnyModalOpen() const {
        return delActive || importActive || (animPickerTile >= 0) ||
               (powerUpOpen && powerUpTileIdx >= 0) ||
               (movPlatOpen && movPlatSpeedInput);
    }

  private:
    bool HandleDeleteConfirmEvent(const SDL_Event& e, Ctx& ctx);
    bool HandleImportInputEvent(const SDL_Event& e, Ctx& ctx);
    bool HandleAnimPickerEvent(const SDL_Event& e, Ctx& ctx);
    bool HandlePowerUpPickerEvent(const SDL_Event& e, Ctx& ctx);
    bool HandleMovPlatPopupEvent(const SDL_Event& e, Ctx& ctx,
                                 std::vector<int>& movPlatIndices);

    void CommitSpeedField(Ctx& ctx, std::vector<int>& movPlatIndices);
};
