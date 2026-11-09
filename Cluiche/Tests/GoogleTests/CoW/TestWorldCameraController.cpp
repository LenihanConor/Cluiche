// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/WorldCameraController.h>
#include <Shared/WorldGeneration/WorldHexGrid.h>

#include <algorithm>
#include <cmath>

using namespace CoW::WorldGeneration;
using Dia::Maths::Vector2D;

namespace
{
const float kW = 1280.0f, kH = 720.0f;

struct Fixture
{
    WorldHexGrid grid{ { 66, 42 } };
    WorldCameraController cam;
    Fixture() { cam.FitToGrid(kW, kH, grid.BoundsMin(), grid.BoundsMax(), 0.1f); }
};
}

TEST(WorldCameraController, FitFramesWholeSmallGridWithMargin)
{
    Fixture f;
    const CameraState& s = f.cam.State();
    const float bw = f.grid.BoundsMax().x - f.grid.BoundsMin().x;
    const float bh = f.grid.BoundsMax().y - f.grid.BoundsMin().y;
    EXPECT_NEAR(s.centreX, (f.grid.BoundsMin().x + f.grid.BoundsMax().x) * 0.5f, 1e-2f);
    EXPECT_NEAR(s.centreY, (f.grid.BoundsMin().y + f.grid.BoundsMax().y) * 0.5f, 1e-2f);
    EXPECT_LE(bw * s.zoom, kW * 0.9f + 0.01f);
    EXPECT_LE(bh * s.zoom, kH * 0.9f + 0.01f);
    // Tight in at least one axis.
    EXPECT_NEAR((std::max)(bw * s.zoom / kW, bh * s.zoom / kH), 0.9f, 1e-3f);
    // Corners of the grid are on screen.
    const Vector2D tl = f.cam.ScreenToWorld(0, 0);
    EXPECT_LT(tl.x, f.grid.BoundsMin().x);
    EXPECT_LT(tl.y, f.grid.BoundsMin().y);
}

TEST(WorldCameraController, PanStopsAtBounds)
{
    Fixture f;
    f.cam.Pan(1e6f, 1e6f);
    const float maxX = f.grid.BoundsMax().x + WorldCameraController::kBoundsMarginPx / f.cam.State().zoom;
    EXPECT_NEAR(f.cam.State().centreX, maxX, 1e-2f);
    const float maxY = f.grid.BoundsMax().y + WorldCameraController::kBoundsMarginPx / f.cam.State().zoom;
    EXPECT_NEAR(f.cam.State().centreY, maxY, 1e-2f);
    f.cam.Pan(-1e7f, -1e7f);
    const float minX = f.grid.BoundsMin().x - WorldCameraController::kBoundsMarginPx / f.cam.State().zoom;
    const float minY = f.grid.BoundsMin().y - WorldCameraController::kBoundsMarginPx / f.cam.State().zoom;
    EXPECT_NEAR(f.cam.State().centreX, minX, 1e-2f);
    EXPECT_NEAR(f.cam.State().centreY, minY, 1e-2f);
}

TEST(WorldCameraController, ZoomClampsAtBothEnds)
{
    Fixture f;
    f.cam.ZoomBy(1e6f, kW * 0.5f, kH * 0.5f);
    EXPECT_FLOAT_EQ(f.cam.State().zoom, WorldCameraController::kMaxZoom);
    f.cam.ZoomBy(1e-9f, kW * 0.5f, kH * 0.5f);
    EXPECT_FLOAT_EQ(f.cam.State().zoom, WorldCameraController::kMinZoom);
}

TEST(WorldCameraController, ZoomAroundAnchorKeepsWorldPointFixed)
{
    Fixture f;
    const float ax = 300.0f, ay = 200.0f;
    const Vector2D before = f.cam.ScreenToWorld(ax, ay);
    f.cam.ZoomBy(2.0f, ax, ay);
    const Vector2D after = f.cam.ScreenToWorld(ax, ay);
    EXPECT_NEAR(before.x, after.x, 1e-2f);
    EXPECT_NEAR(before.y, after.y, 1e-2f);
}

TEST(WorldCameraController, HomeRefits)
{
    Fixture f;
    const CameraState fitted = f.cam.State();
    f.cam.ZoomBy(3.0f, 100, 100);
    f.cam.Pan(500, 500);
    CameraInput in; in.keyHome = true;
    f.cam.Update(in);
    EXPECT_FLOAT_EQ(f.cam.State().zoom, fitted.zoom);
    EXPECT_FLOAT_EQ(f.cam.State().centreX, fitted.centreX);
    EXPECT_FLOAT_EQ(f.cam.State().centreY, fitted.centreY);
}

TEST(WorldCameraController, ResetRefits)
{
    Fixture f;
    const CameraState fitted = f.cam.State();
    f.cam.ZoomBy(3.0f, 100, 100);
    f.cam.Reset();
    EXPECT_FLOAT_EQ(f.cam.State().zoom, fitted.zoom);
}

TEST(WorldCameraController, WheelPositiveZoomsInNegativeOut)
{
    Fixture f;
    const float z0 = f.cam.State().zoom;
    CameraInput in; in.wheelDelta = 1.0f; in.mouseX = kW * 0.5f; in.mouseY = kH * 0.5f;
    f.cam.Update(in);
    EXPECT_GT(f.cam.State().zoom, z0);
    in.wheelDelta = -1.0f;
    f.cam.Update(in);
    EXPECT_NEAR(f.cam.State().zoom, z0, 1e-4f);
}

TEST(WorldCameraController, DragPansOppositeScaledByInverseZoom)
{
    Fixture f;
    f.cam.ZoomBy(2.0f, kW * 0.5f, kH * 0.5f);
    const CameraState s0 = f.cam.State();
    CameraInput in; in.dragActive = true; in.dragDeltaX = 40.0f; in.dragDeltaY = -20.0f;
    f.cam.Update(in);
    EXPECT_NEAR(f.cam.State().centreX - s0.centreX, -40.0f / s0.zoom, 1e-3f);
    EXPECT_NEAR(f.cam.State().centreY - s0.centreY, 20.0f / s0.zoom, 1e-3f);
}

TEST(WorldCameraController, DragDeltaIgnoredWhenNotDragging)
{
    Fixture f;
    const CameraState s0 = f.cam.State();
    CameraInput in; in.dragActive = false; in.dragDeltaX = 40.0f;
    f.cam.Update(in);
    EXPECT_FLOAT_EQ(f.cam.State().centreX, s0.centreX);
}

TEST(WorldCameraController, KeyboardPanScalesWithDt)
{
    Fixture f;
    f.cam.ZoomBy(2.0f, kW * 0.5f, kH * 0.5f);
    const CameraState s0 = f.cam.State();
    CameraInput in; in.keyD = true; in.dtSeconds = 0.1f;
    f.cam.Update(in);
    const float d1 = f.cam.State().centreX - s0.centreX;
    EXPECT_NEAR(d1, WorldCameraController::kPanSpeed * 0.1f / s0.zoom, 1e-3f);
    const CameraState s1 = f.cam.State();
    in.dtSeconds = 0.2f;
    f.cam.Update(in);
    EXPECT_NEAR(f.cam.State().centreX - s1.centreX, 2.0f * d1, 1e-3f);
}

TEST(WorldCameraController, KeyboardPanDirectionsAndArrows)
{
    Fixture f;
    f.cam.ZoomBy(2.0f, kW * 0.5f, kH * 0.5f);
    const CameraState s0 = f.cam.State();
    CameraInput in; in.keyA = true; in.keyUp = true; in.dtSeconds = 0.05f;   // left and up
    f.cam.Update(in);
    EXPECT_LT(f.cam.State().centreX, s0.centreX);
    EXPECT_LT(f.cam.State().centreY, s0.centreY);
    CameraInput opp; opp.keyRight = true; opp.keyDown = true; opp.dtSeconds = 0.05f;
    f.cam.Update(opp);
    EXPECT_NEAR(f.cam.State().centreX, s0.centreX, 1e-3f);
    EXPECT_NEAR(f.cam.State().centreY, s0.centreY, 1e-3f);
}

TEST(WorldCameraController, KeyboardZoomInAndOutScaleWithDt)
{
    Fixture f;
    const float z0 = f.cam.State().zoom;
    CameraInput in; in.keyPlus = true; in.dtSeconds = 0.1f; in.mouseX = kW * 0.5f; in.mouseY = kH * 0.5f;
    f.cam.Update(in);
    const float zin = f.cam.State().zoom;
    EXPECT_GT(zin, z0);
    CameraInput out; out.keyMinus = true; out.dtSeconds = 0.1f; out.mouseX = kW * 0.5f; out.mouseY = kH * 0.5f;
    f.cam.Update(out);
    EXPECT_NEAR(f.cam.State().zoom, z0, 1e-4f);
    CameraInput qe; qe.keyE = true; qe.dtSeconds = 0.1f; qe.mouseX = kW * 0.5f; qe.mouseY = kH * 0.5f;
    f.cam.Update(qe);
    EXPECT_NEAR(f.cam.State().zoom, zin, 1e-4f);
    CameraInput q; q.keyQ = true; q.dtSeconds = 0.1f; q.mouseX = kW * 0.5f; q.mouseY = kH * 0.5f;
    f.cam.Update(q);
    EXPECT_NEAR(f.cam.State().zoom, z0, 1e-4f);
}
