#pragma once
// EditorPopups.hpp
//
// Owns the state and event handling for every modal/popup in the level editor:
//
//   1. Delete confirmation    — "Delete 'foo.png'? [Yes] [No]"
//   2. Import text input      — keyboard path entry (I key)
//   3. Destroy-anim picker    — grid of animation thumbnails for Action tiles
//   4. Power-up picker        — list of power-up types for PowerUp tiles
//   5. Moving-platform config — speed field, H/V toggle, loop/trigger checkboxes
//
// EditorPopups does NOT render — it only manages state and event dispatch.
// Rendering is handled by EditorUIRenderer (popups are already wired up there).
// After calling EditorPopups::HandleEvent(), the orchestrator reads back the
// state via accessors and feeds it into EditorUIRenderer::Render() as before.
//
// The class also exposes the AnimPickerEntry and PowerUpEntry types so the
// orchestrator can forward them without re-declaring them.

#include "AnimatedTile.hpp"
#include "EditorPalette.hpp"
#include "LevelData.hpp"
#include <SDL3/SDL.h>
#include <functional>
#include <string>
#include <vector>

class EditorPopups {
  public:
    // ── Shared entry types (same fields as EditorUIRenderer's mirror types) ──
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

    // ── Context: callbacks that popups need to call back into the orchestrator ─
    struct Ctx {
        Level&        level;
        EditorPalette& palette;

        std::function<void(const std::string&)> setStatus;
        std::function<void()>                   refreshTileView;   // after delete
        std::function<void()>                   refreshBgPalette;  // after delete
        std::function<bool(const std::string&)> importPath;        // import modal confirm
        std::function<SDL_Surface*(const std::string&)> getAnimThumb; // for picker

        SDL_Window* sdlWindow = nullptr;  // for StartTextInput / StopTextInput

        const char* tileRoot = "game_assets/tiles";
        const char* bgRoot   = "game_assets/backgrounds";
    };

    // ── Delete confirmation popup ─────────────────────────────────────────────
    bool        delActive  = false;
    std::string delPath;
    bool        delIsDir   = false;
    std::string delName;
    SDL_Rect    delYes{};
    SDL_Rect    delNo{};

    // Called by the orchestrator when user clicks a delete button in the palette.
    void OpenDeleteConfirm(const std::string& path, bool isDir, const std::string& name);

    // ── Import text input modal ───────────────────────────────────────────────
    bool        importActive = false;
    std::string importText;

    // Called by the orchestrator (I key handler).
    void OpenImportInput(bool isBgTab, Ctx& ctx);

    // ── Destroy-anim picker ───────────────────────────────────────────────────
    int                      animPickerTile = -1;  // -1 = closed
    SDL_Rect                 animPickerRect{};
    SDL_Rect                 camShakeToggleRect{};
    std::vector<AnimPickerEntry> animPickerEntries;

    void OpenAnimPicker(int tileIdx, Ctx& ctx);
    void CloseAnimPicker();

    // ── Power-up picker ───────────────────────────────────────────────────────
    bool     powerUpOpen    = false;
    int      powerUpTileIdx = -1;
    SDL_Rect powerUpRect{};

    // Registry is static / constant — managed externally and pointed to from here.
    const std::vector<PowerUpEntry>* powerUpRegistry = nullptr;

    void OpenPowerUpPicker(int tileIdx, int screenX, int screenY,
                           int windowW, int windowH, int toolbarH);
    void ClosePowerUpPicker();

    // ── Moving-platform config popup ─────────────────────────────────────────
    bool        movPlatOpen       = false;
    bool        movPlatSpeedInput = false;
    std::string movPlatSpeedStr   = "60";
    SDL_Rect    movPlatRect{};

    // Moving-platform parameter state (shared with the tool inline state in
    // LevelEditorScene). Kept here so the popup can read/write it atomically.
    // The orchestrator syncs these from its own mMovPlat* members before calling
    // HandleEvent(), and reads them back after.
    float movPlatSpeed   = 60.0f;
    bool  movPlatHoriz   = true;
    bool  movPlatLoop    = false;
    bool  movPlatTrigger = false;
    int   movPlatGroupId = 1;

    // ── Main event dispatcher ─────────────────────────────────────────────────
    // Call this before the orchestrator's own event dispatch. Returns true if
    // the event was fully consumed by a popup and must not propagate further.
    //
    // The orchestrator is responsible for:
    //   - Passing correct `mMovPlatIndices` and `mLevel.tiles` via the ctx Level&
    //     (modifications are written back in-place through ctx.level).
    //   - Syncing movPlat* fields into this object before calling HandleEvent.
    //   - Reading movPlat* fields back after for the Render pass.
    bool HandleEvent(const SDL_Event& e,
                     Ctx&             ctx,
                     std::vector<int>& movPlatIndices);

    // Convenience: is ANY popup currently blocking all other input?
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

    // Commit the speed text field value to tiles and the movPlatSpeed member.
    // Called on Enter/Escape/Tab/close-button for the movplat config popup.
    void CommitSpeedField(Ctx& ctx, std::vector<int>& movPlatIndices);
};
