// S-1 World Generation map view: pure 2D camera controller (fit, pan, zoom, input mapping).
// No Camera2DModule dependency; the stage module copies State to the active Camera2D each frame.
// Zoom convention matches Dia::Camera2D: >1 = zoomed in, screen px per world unit.
// Screen space: x right, y down, origin top-left; the camera centre maps to the viewport centre.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#pragma once

#include <DiaMaths/Vector/Vector2D.h>

namespace CoW {
namespace WorldGeneration {

struct CameraState
{
    float centreX = 0.0f;
    float centreY = 0.0f;
    float zoom    = 1.0f;
};

// One frame of camera input.
struct CameraInput
{
    bool  keyW = false, keyA = false, keyS = false, keyD = false;
    bool  keyUp = false, keyDown = false, keyLeft = false, keyRight = false;
    bool  keyPlus = false, keyMinus = false, keyQ = false, keyE = false;   // zoom in: Plus/E, out: Minus/Q
    bool  keyHome = false;
    bool  dragActive = false;                     // middle or right button held this frame
    float dragDeltaX = 0.0f, dragDeltaY = 0.0f;   // screen pixels moved since last frame
    float wheelDelta = 0.0f;                      // positive zooms in
    float mouseX = 0.0f, mouseY = 0.0f;           // screen position, the zoom anchor
    float dtSeconds = 0.0f;
};

class WorldCameraController
{
public:
    static constexpr float kMinZoom         = 0.1f;
    static constexpr float kMaxZoom         = 8.0f;
    static constexpr float kPanSpeed        = 600.0f;   // screen px per second at any zoom
    static constexpr float kKeyZoomRate     = 1.5f;     // zoom multiplier per second of key hold
    static constexpr float kWheelZoomStep   = 1.15f;    // multiplier per wheel tick
    static constexpr float kBoundsMarginPx  = 100.0f;   // how far past the grid the view centre may go, screen px

    // Sets the viewport (pixels) and world bounds, then fits. margin is a fraction of the viewport left
    // empty around the grid (0.1 = 10%).
    void FitToGrid(float viewportW, float viewportH, Dia::Maths::Vector2D boundsMin, Dia::Maths::Vector2D boundsMax, float margin);
    void Reset() { FitToGrid(mViewW, mViewH, mBoundsMin, mBoundsMax, mMargin); }

    // Pan in world units, clamped so the view centre cannot leave the bounds by more than the margin.
    void Pan(float dx, float dy);
    // Zoom by factor around a screen point; the world point under it stays fixed (until clamped).
    void ZoomBy(float factor, float anchorScreenX, float anchorScreenY);
    // Applies one frame of input.
    void Update(const CameraInput& in);

    const CameraState& State() const { return mState; }
    Dia::Maths::Vector2D ScreenToWorld(float sx, float sy) const;

private:
    void ClampCentre();

    CameraState          mState;
    float                mViewW = 1280.0f, mViewH = 720.0f;
    Dia::Maths::Vector2D mBoundsMin{ 0.0f, 0.0f }, mBoundsMax{ 1.0f, 1.0f };
    float                mMargin = 0.1f;
};

} // namespace WorldGeneration
} // namespace CoW
