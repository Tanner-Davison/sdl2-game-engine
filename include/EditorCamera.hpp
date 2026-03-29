#pragma once
// EditorCamera.hpp — camera state, pan/drag, zoom, coordinate conversions.

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>

class EditorCamera {
  public:
    EditorCamera() = default;

    static constexpr float ZOOM_MIN  = 0.25f;
    static constexpr float ZOOM_MAX  = 4.0f;
    static constexpr float ZOOM_STEP = 0.1f;

    float X() const {
        return mCamX;
    }
    float Y() const {
        return mCamY;
    }
    float Zoom() const {
        return mZoom;
    }
    bool IsPanning() const {
        return mIsPanning;
    }

    void SetPosition(float x, float y) {
        mCamX = x;
        mCamY = y;
    }
    void Reset() {
        mCamX = mCamY = 0.0f;
        mZoom         = 1.0f;
        mIsPanning    = false;
    }

    SDL_Point ScreenToWorld(int sx, int sy) const {
        return {(int)(sx / mZoom + mCamX), (int)(sy / mZoom + mCamY)};
    }

    SDL_Point WorldToScreen(float wx, float wy) const {
        return {(int)((wx - mCamX) * mZoom), (int)((wy - mCamY) * mZoom)};
    }

    // floor() ensures the result is always the grid cell visually containing
    // the cursor, regardless of zoom level or sign.
    SDL_Point SnapToGrid(int sx, int sy, int grid) const {
        float wx = sx / mZoom + mCamX;
        float wy = sy / mZoom + mCamY;
        int   cx = (int)std::floor(wx / grid) * grid;
        int   cy = (int)std::floor(wy / grid) * grid;
        if (cy < 0)
            cy = 0;
        return {cx, cy};
    }

    void StartPan(int mouseX, int mouseY) {
        mIsPanning    = true;
        mPanStartX    = mouseX;
        mPanStartY    = mouseY;
        mPanCamStartX = mCamX;
        mPanCamStartY = mCamY;
        SDL_CaptureMouse(true);
    }

    void StopPan() {
        mIsPanning = false;
        SDL_CaptureMouse(false);
    }

    // Absolute position delta avoids jitter from coalesced motion events on macOS.
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

        if (mCamX < 0.0f)
            mCamX = 0.0f;
        if (mCamY < 0.0f)
            mCamY = 0.0f;
        return true;
    }

    int ZoomPercent() const {
        return (int)(mZoom * 100);
    }

  private:
    float mCamX = 0.0f;
    float mCamY = 0.0f;
    float mZoom = 1.0f;

    bool  mIsPanning    = false;
    int   mPanStartX    = 0;
    int   mPanStartY    = 0;
    float mPanCamStartX = 0.0f;
    float mPanCamStartY = 0.0f;
};
