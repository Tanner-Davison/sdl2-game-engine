#pragma once
// EditorCamera.hpp
// ---------------------------------------------------------------------------
// Encapsulates editor camera state: position, zoom, pan/drag mechanics,
// and coordinate-space conversions (screen <-> world, snap-to-grid).
//
// Extracted from LevelEditorScene as part of the modular refactor (Prompt #2).
// The orchestrator (LevelEditorScene) owns an EditorCamera instance and
// delegates all camera queries and pan/zoom input to it.
// ---------------------------------------------------------------------------

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>

class EditorCamera {
  public:
    EditorCamera() = default;

    // ── Zoom constants ────────────────────────────────────────────────────────
    static constexpr float ZOOM_MIN  = 0.25f;
    static constexpr float ZOOM_MAX  = 4.0f;
    static constexpr float ZOOM_STEP = 0.1f;

    // ── Read-only accessors ───────────────────────────────────────────────────
    float X() const { return mCamX; }
    float Y() const { return mCamY; }
    float Zoom() const { return mZoom; }
    bool  IsPanning() const { return mIsPanning; }

    // ── Direct setters (used by Load on level-load reset, etc.) ───────────────
    void SetPosition(float x, float y) {
        mCamX = x;
        mCamY = y;
    }
    void Reset() {
        mCamX = mCamY = 0.0f;
        mZoom         = 1.0f;
        mIsPanning    = false;
    }

    // ── Coordinate conversions ────────────────────────────────────────────────

    // Convert a screen-space canvas point to world space (accounts for zoom).
    SDL_Point ScreenToWorld(int sx, int sy) const {
        return {(int)(sx / mZoom + mCamX), (int)(sy / mZoom + mCamY)};
    }

    // Convert a world-space point to screen space (accounts for zoom).
    SDL_Point WorldToScreen(float wx, float wy) const {
        return {(int)((wx - mCamX) * mZoom), (int)((wy - mCamY) * mZoom)};
    }

    // Convert screen coords to world coords, then snap to the nearest grid cell.
    // Uses std::floor so the result is always the grid cell that visually
    // contains the cursor, regardless of zoom level or sign.
    SDL_Point SnapToGrid(int sx, int sy, int grid) const {
        float wx = sx / mZoom + mCamX;
        float wy = sy / mZoom + mCamY;
        int   cx = (int)std::floor(wx / grid) * grid;
        int   cy = (int)std::floor(wy / grid) * grid;
        if (cy < 0)
            cy = 0;
        return {cx, cy};
    }

    // ── Pan (drag) mechanics ──────────────────────────────────────────────────

    // Begin a pan operation. Records the mouse position and current camera
    // offset so that UpdatePan can compute deltas from absolute coords.
    void StartPan(int mouseX, int mouseY) {
        mIsPanning    = true;
        mPanStartX    = mouseX;
        mPanStartY    = mouseY;
        mPanCamStartX = mCamX;
        mPanCamStartY = mCamY;
        SDL_CaptureMouse(true);
    }

    // End the current pan operation.
    void StopPan() {
        mIsPanning = false;
        SDL_CaptureMouse(false);
    }

    // Update camera position from current mouse position during a pan.
    // Uses absolute position minus the recorded start so coalesced motion
    // events on macOS always land exactly where the mouse is now.
    void UpdatePan(int mouseX, int mouseY) {
        if (!mIsPanning)
            return;
        mCamX = mPanCamStartX - (mouseX - mPanStartX) / mZoom;
        mCamY = mPanCamStartY - (mouseY - mPanStartY) / mZoom;
        if (mCamX < 0.0f)
            mCamX = 0.0f;
        if (mCamY < 0.0f)
            mCamY = 0.0f;
    }

    // ── Zoom mechanics ────────────────────────────────────────────────────────

    // Apply a zoom step anchored to the given screen-space mouse position.
    // Returns true if the zoom actually changed (useful for status messages).
    bool ApplyZoom(float wheelY, int mouseX, int mouseY) {
        float oldZoom = mZoom;
        float newZoom = std::clamp(mZoom + wheelY * ZOOM_STEP, ZOOM_MIN, ZOOM_MAX);
        if (newZoom == oldZoom)
            return false;

        // Anchor zoom to mouse position: keep world point under cursor fixed.
        float worldX = mouseX / oldZoom + mCamX;
        float worldY = mouseY / oldZoom + mCamY;
        mZoom        = newZoom;
        mCamX        = worldX - mouseX / mZoom;
        mCamY        = worldY - mouseY / mZoom;

        // Clamp so we don't go into negative world space.
        if (mCamX < 0.0f)
            mCamX = 0.0f;
        if (mCamY < 0.0f)
            mCamY = 0.0f;
        return true;
    }

    // Current zoom as a percentage integer (e.g. 100, 150, 50).
    int ZoomPercent() const { return (int)(mZoom * 100); }

  private:
    // World-space camera offset.
    float mCamX = 0.0f;
    float mCamY = 0.0f;

    // Canvas zoom: 1.0 = 100%, 0.5 = 50%, 2.0 = 200%.
    float mZoom = 1.0f;

    // Pan drag state.
    bool  mIsPanning    = false;
    int   mPanStartX    = 0;
    int   mPanStartY    = 0;
    float mPanCamStartX = 0.0f;
    float mPanCamStartY = 0.0f;
};
