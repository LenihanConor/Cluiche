////////////////////////////////////////////////////////////////////////////////
// Filename: TestCameraViewport.cpp
// Tests: ViewportTransform screen<->world conversion used by CameraModule.
//        CameraModule is a thin wrapper — this validates the underlying transform.
// AC11
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaGraphics/Camera/Camera2D.h>
#include <DiaGraphics/Camera/ViewportTransform.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Graphics;
using namespace Dia::Maths;

static constexpr float kEpsilon = 0.1f;

// Window 800x600, camera at origin, no zoom
class ViewportFixture : public ::testing::Test
{
protected:
    Camera2D cam;
    Vector2D windowSize{800.0f, 600.0f};

    ViewportTransform MakeVT() const
    {
        return ViewportTransform(cam, windowSize);
    }
};

TEST_F(ViewportFixture, ScreenCentre_MapsToWorldOrigin)
{
    auto vt = MakeVT();
    Vector2D world = vt.ScreenToWorld(Vector2D(400.0f, 300.0f));
    EXPECT_NEAR(world.x, 0.0f, kEpsilon);
    EXPECT_NEAR(world.y, 0.0f, kEpsilon);
}

TEST_F(ViewportFixture, WorldOrigin_MapsToScreenCentre)
{
    auto vt = MakeVT();
    Vector2D screen = vt.WorldToScreen(Vector2D(0.0f, 0.0f));
    EXPECT_NEAR(screen.x, 400.0f, kEpsilon);
    EXPECT_NEAR(screen.y, 300.0f, kEpsilon);
}

TEST_F(ViewportFixture, RoundTrip_ScreenToWorld_WorldToScreen)
{
    auto vt = MakeVT();
    Vector2D original(150.0f, 250.0f);
    Vector2D world  = vt.ScreenToWorld(original);
    Vector2D back   = vt.WorldToScreen(world);
    EXPECT_NEAR(back.x, original.x, kEpsilon);
    EXPECT_NEAR(back.y, original.y, kEpsilon);
}

TEST_F(ViewportFixture, CameraOffset_ShiftsMapping)
{
    cam.SetPosition(Vector2D(100.0f, 0.0f)); // camera moved right
    auto vt = MakeVT();
    // Screen centre should now map to world (100, 0)
    Vector2D world = vt.ScreenToWorld(Vector2D(400.0f, 300.0f));
    EXPECT_NEAR(world.x, 100.0f, kEpsilon);
    EXPECT_NEAR(world.y, 0.0f, kEpsilon);
}

TEST_F(ViewportFixture, ViewportTransform_NonDebug_Constructible)
{
    // AC11: ViewportTransform (and hence CameraModule) must work in non-debug builds.
    // This test has no DIA_DEBUG dependency — verify it compiles and runs in Release.
    Camera2D releaseCam;
    Vector2D releaseSize(1400.0f, 1000.0f);
    ViewportTransform vt(releaseCam, releaseSize);
    Vector2D world = vt.ScreenToWorld(Vector2D(700.0f, 500.0f));
    EXPECT_NEAR(world.x, 0.0f, kEpsilon);
    EXPECT_NEAR(world.y, 0.0f, kEpsilon);
}
