// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#include <Shared/WorldGeneration/WorldCameraController.h>

#include <algorithm>
#include <cmath>

namespace CoW {
namespace WorldGeneration {

using Dia::Maths::Vector2D;

void WorldCameraController::FitToGrid(float viewportW, float viewportH, Vector2D boundsMin, Vector2D boundsMax, float margin)
{
    mViewW = viewportW;
    mViewH = viewportH;
    mBoundsMin = boundsMin;
    mBoundsMax = boundsMax;
    mMargin = margin;

    const float bw = (std::max)(boundsMax.x - boundsMin.x, 1e-3f);
    const float bh = (std::max)(boundsMax.y - boundsMin.y, 1e-3f);
    const float fit = (1.0f - margin) * (std::min)(viewportW / bw, viewportH / bh);
    mState.zoom    = (std::min)((std::max)(fit, kMinZoom), kMaxZoom);
    mState.centreX = (boundsMin.x + boundsMax.x) * 0.5f;
    mState.centreY = (boundsMin.y + boundsMax.y) * 0.5f;
}

void WorldCameraController::ClampCentre()
{
    const float m = kBoundsMarginPx / mState.zoom;
    mState.centreX = (std::min)((std::max)(mState.centreX, mBoundsMin.x - m), mBoundsMax.x + m);
    mState.centreY = (std::min)((std::max)(mState.centreY, mBoundsMin.y - m), mBoundsMax.y + m);
}

void WorldCameraController::Pan(float dx, float dy)
{
    if (!std::isfinite(dx) || !std::isfinite(dy))
        return;
    mState.centreX += dx;
    mState.centreY += dy;
    ClampCentre();
}

Vector2D WorldCameraController::ScreenToWorld(float sx, float sy) const
{
    return Vector2D(mState.centreX + (sx - mViewW * 0.5f) / mState.zoom,
                    mState.centreY + (sy - mViewH * 0.5f) / mState.zoom);
}

void WorldCameraController::ZoomBy(float factor, float anchorScreenX, float anchorScreenY)
{
    if (!std::isfinite(anchorScreenX) || !std::isfinite(anchorScreenY) || std::isnan(factor))
        return;   // a NaN would stick in the state forever (std::min/max pass NaN through)
    const Vector2D anchor = ScreenToWorld(anchorScreenX, anchorScreenY);
    mState.zoom = (std::min)((std::max)(mState.zoom * factor, kMinZoom), kMaxZoom);
    mState.centreX = anchor.x - (anchorScreenX - mViewW * 0.5f) / mState.zoom;
    mState.centreY = anchor.y - (anchorScreenY - mViewH * 0.5f) / mState.zoom;
    ClampCentre();
}

void WorldCameraController::Update(const CameraInput& raw)
{
    CameraInput in = raw;
    auto clean = [](float& v) { if (!std::isfinite(v)) v = 0.0f; };
    clean(in.dragDeltaX); clean(in.dragDeltaY); clean(in.dtSeconds);
    if (std::isnan(in.wheelDelta)) in.wheelDelta = 0.0f;   // +-inf is clamped by ZoomBy
    if (!std::isfinite(in.mouseX) || !std::isfinite(in.mouseY)) { in.mouseX = mViewW * 0.5f; in.mouseY = mViewH * 0.5f; }

    if (in.keyHome)
    {
        Reset();
        return;
    }

    // Zoom first so pan speed this frame uses the new zoom consistently.
    if (in.wheelDelta != 0.0f)
        ZoomBy(std::pow(kWheelZoomStep, in.wheelDelta), in.mouseX, in.mouseY);

    const float zoomIn  = (in.keyPlus  ? 1.0f : 0.0f) + (in.keyE ? 1.0f : 0.0f);
    const float zoomOut = (in.keyMinus ? 1.0f : 0.0f) + (in.keyQ ? 1.0f : 0.0f);
    const float net = zoomIn - zoomOut;
    if (net != 0.0f && in.dtSeconds > 0.0f)
        ZoomBy(std::pow(kKeyZoomRate, net * in.dtSeconds), mViewW * 0.5f, mViewH * 0.5f);

    float kx = 0.0f, ky = 0.0f;
    if (in.keyD || in.keyRight) kx += 1.0f;
    if (in.keyA || in.keyLeft)  kx -= 1.0f;
    if (in.keyS || in.keyDown)  ky += 1.0f;
    if (in.keyW || in.keyUp)    ky -= 1.0f;
    float dx = 0.0f, dy = 0.0f;
    if ((kx != 0.0f || ky != 0.0f) && in.dtSeconds > 0.0f)
    {
        const float step = kPanSpeed * in.dtSeconds / mState.zoom;
        dx += kx * step;
        dy += ky * step;
    }
    if (in.dragActive)
    {
        dx -= in.dragDeltaX / mState.zoom;
        dy -= in.dragDeltaY / mState.zoom;
    }
    if (dx != 0.0f || dy != 0.0f)
        Pan(dx, dy);
}

} // namespace WorldGeneration
} // namespace CoW
