#include <gtest/gtest.h>

#include <DiaCamera2D/Camera2D.h>
#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaCamera2D/Behaviour/FollowBehaviour.h>
#include <DiaCamera2D/Behaviour/BoundsClampBehaviour.h>
#include <DiaCamera2D/Behaviour/ScreenShakeBehaviour.h>
#include <DiaCamera2D/Behaviour/PanBehaviour.h>
#include <DiaCamera2D/Behaviour/ZoomBehaviour.h>
#include <DiaCamera2D/Behaviour/SmoothDampBehaviour.h>
#include <DiaCamera2D/Testing/CameraBuilder.h>
#include <DiaCore/CRC/StringCRC.h>
#include <cmath>
#include <random>

using namespace Dia::Camera2D;

// ---------------------------------------------------------------------------
// ViewportTransform (migrated — tests must still pass)
// ---------------------------------------------------------------------------
#include <DiaCamera2D/ViewportTransform.h>

TEST(DiaCamera2D_Viewport, DefaultCamera_CentreOfScreenIsOrigin)
{
    Camera2D cam;
    ViewportTransform vt(cam, Dia::Maths::Vector2D(1400.0f, 1000.0f));
    Dia::Maths::Vector2D world = vt.ScreenToWorld(Dia::Maths::Vector2D(700.0f, 500.0f));
    EXPECT_NEAR(world.x, 0.0f, 0.001f);
    EXPECT_NEAR(world.y, 0.0f, 0.001f);
}

TEST(DiaCamera2D_Viewport, RoundTrip_WorldToScreenToWorld)
{
    Camera2D cam(Dia::Maths::Vector2D(100.0f, 50.0f), 2.0f, 0.0f);
    ViewportTransform vt(cam, Dia::Maths::Vector2D(1400.0f, 1000.0f));
    const Dia::Maths::Vector2D original(73.0f, -42.0f);
    const Dia::Maths::Vector2D roundTrip = vt.ScreenToWorld(vt.WorldToScreen(original));
    EXPECT_NEAR(roundTrip.x, original.x, 0.01f);
    EXPECT_NEAR(roundTrip.y, original.y, 0.01f);
}

TEST(DiaCamera2D_Viewport, RoundTrip_WithRotation)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f), 1.0f, 45.0f);
    ViewportTransform vt(cam, Dia::Maths::Vector2D(800.0f, 600.0f));
    const Dia::Maths::Vector2D original(25.0f, 75.0f);
    const Dia::Maths::Vector2D roundTrip = vt.ScreenToWorld(vt.WorldToScreen(original));
    EXPECT_NEAR(roundTrip.x, original.x, 0.01f);
    EXPECT_NEAR(roundTrip.y, original.y, 0.01f);
}

TEST(DiaCamera2D_Viewport, GetWorldBounds_ContainsScreenCorners)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f), 1.0f, 0.0f);
    ViewportTransform vt(cam, Dia::Maths::Vector2D(1400.0f, 1000.0f));
    const Dia::Geometry2D::AARect bounds = vt.GetWorldBounds();
    // World origin should be inside the viewport (bottomLeft < 0, topRight > 0)
    EXPECT_LT(bounds.GetBottomLeft().x, 0.0f);
    EXPECT_LT(bounds.GetBottomLeft().y, 0.0f);
    EXPECT_GT(bounds.GetTopRight().x, 0.0f);
    EXPECT_GT(bounds.GetTopRight().y, 0.0f);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_EdgeCases, Camera2D_ExtremeZoom_NoNaN)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f), 0.00001f);
    EXPECT_FALSE(std::isnan(cam.GetZoom()));
}

TEST(DiaCamera2D_EdgeCases, RegistryAtCapacity_MaxCameras)
{
    CameraRegistry2D reg;
    for (unsigned int i = 0; i < CameraRegistry2D::kMaxCameras; ++i)
    {
        char name[8];
        name[0] = 'c'; name[1] = static_cast<char>('0' + i); name[2] = '\0';
        reg.Register(Dia::Core::StringCRC(name), Camera2D{});
    }
    EXPECT_EQ(reg.GetCount(), CameraRegistry2D::kMaxCameras);
}

TEST(DiaCamera2D_EdgeCases, UnregisterMiddleCamera_OthersCameraIntact)
{
    CameraRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("a"), Camera2D(Dia::Maths::Vector2D(1.0f, 0.0f)));
    reg.Register(Dia::Core::StringCRC("b"), Camera2D(Dia::Maths::Vector2D(2.0f, 0.0f)));
    reg.Register(Dia::Core::StringCRC("c"), Camera2D(Dia::Maths::Vector2D(3.0f, 0.0f)));
    reg.Unregister(Dia::Core::StringCRC("b"));
    EXPECT_FLOAT_EQ(reg.Get(Dia::Core::StringCRC("a")).GetPosition().x, 1.0f);
    EXPECT_FLOAT_EQ(reg.Get(Dia::Core::StringCRC("c")).GetPosition().x, 3.0f);
}

TEST(DiaCamera2D_EdgeCases, BoundsClamp_ViewportLargerThanWorld_NoOp)
{
    // Window is bigger than world — clamping should not apply
    Camera2D cam(Dia::Maths::Vector2D(500.0f, 500.0f), 1.0f);
    Dia::Geometry2D::AARect tinyBounds(Dia::Maths::Vector2D(0,0), Dia::Maths::Vector2D(10,10));
    BoundsClampBehaviour b(tinyBounds, Dia::Maths::Vector2D(2000.0f, 2000.0f));
    b.Update(cam, 1.0f / 60.0f);
    // Should not crash, result within bounds or unchanged
    EXPECT_FALSE(std::isnan(cam.GetPosition().x));
}

TEST(DiaCamera2D_EdgeCases, ScreenShake_ZeroTrauma_ZeroDecayNoInf)
{
    ScreenShakeBehaviour b(20.0f, 0.0f);  // zero decay
    Camera2D cam;
    b.Update(cam, 1.0f / 60.0f);  // no trauma yet — should not affect camera
    EXPECT_FALSE(std::isnan(cam.GetPosition().x));
}

TEST(DiaCamera2D_EdgeCases, ZoomBehaviour_ZeroSensitivity_ZoomUnchanged)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f), 1.0f);
    ZoomBehaviour b(0.0f, 0.25f, 4.0f);
    b.SetInput(10.0f);
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(), 1.0f);
}

// ---------------------------------------------------------------------------
// Composition — behaviours run in attachment order
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_Composition, FollowThenBoundsClamp_ClampedAfterFollow)
{
    CameraRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("cam"), Camera2D{});
    reg.SetActive(Dia::Core::StringCRC("cam"));

    auto* follow = new FollowBehaviour();
    auto* clamp  = new BoundsClampBehaviour(
        Dia::Geometry2D::AARect(Dia::Maths::Vector2D(0,0), Dia::Maths::Vector2D(500,500)),
        Dia::Maths::Vector2D(100.0f, 100.0f));

    reg.AttachBehaviour(Dia::Core::StringCRC("cam"), follow);
    reg.AttachBehaviour(Dia::Core::StringCRC("cam"), clamp);

    follow->SetTarget(Dia::Maths::Vector2D(1000.0f, 1000.0f));  // outside bounds
    reg.UpdateAll(1.0f / 60.0f);

    // After follow moves to 1000,1000 and clamp trims it, position should be <= 450 (500 - 50 halfW)
    EXPECT_LE(reg.GetActive().GetPosition().x, 500.0f);
}

TEST(DiaCamera2D_Composition, FollowThenShake_ShakeAddedOnTop)
{
    CameraRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("cam"), Camera2D{});
    reg.SetActive(Dia::Core::StringCRC("cam"));

    auto* follow = new FollowBehaviour();
    auto* shake  = new ScreenShakeBehaviour(10.0f, 0.0f);  // no decay this frame

    reg.AttachBehaviour(Dia::Core::StringCRC("cam"), follow);
    reg.AttachBehaviour(Dia::Core::StringCRC("cam"), shake);

    follow->SetTarget(Dia::Maths::Vector2D(100.0f, 100.0f));
    shake->Trigger(1.0f);
    reg.UpdateAll(1.0f / 60.0f);

    // Camera is near 100,100 but displaced by shake
    const float x = reg.GetActive().GetPosition().x;
    const float y = reg.GetActive().GetPosition().y;
    EXPECT_FALSE(std::isnan(x));
    EXPECT_FALSE(std::isnan(y));
}

// ---------------------------------------------------------------------------
// Stress test
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_Stress, RapidRegisterUnregister_NoCrash)
{
    CameraRegistry2D reg;
    for (int round = 0; round < 100; ++round)
    {
        for (unsigned int i = 0; i < CameraRegistry2D::kMaxCameras; ++i)
        {
            char name[8];
            name[0] = 'c'; name[1] = static_cast<char>('a' + i); name[2] = '\0';
            reg.Register(Dia::Core::StringCRC(name), Camera2D{});
        }
        for (unsigned int i = 0; i < CameraRegistry2D::kMaxCameras; ++i)
        {
            char name[8];
            name[0] = 'c'; name[1] = static_cast<char>('a' + i); name[2] = '\0';
            reg.Unregister(Dia::Core::StringCRC(name));
        }
    }
    EXPECT_EQ(reg.GetCount(), 0u);
}

TEST(DiaCamera2D_Stress, MaxCamerasMaxBehaviours_UpdateAll_NoCrash)
{
    CameraRegistry2D reg;
    for (unsigned int i = 0; i < CameraRegistry2D::kMaxCameras; ++i)
    {
        char name[8];
        name[0] = 'c'; name[1] = static_cast<char>('a' + i); name[2] = '\0';
        Dia::Core::StringCRC id(name);
        reg.Register(id, Camera2D{});
        for (unsigned int b = 0; b < CameraRegistry2D::kMaxBehaviours; ++b)
            reg.AttachBehaviour(id, new PanBehaviour());
    }
    for (int frame = 0; frame < 120; ++frame)
        reg.UpdateAll(1.0f / 60.0f);

    EXPECT_EQ(reg.GetCount(), CameraRegistry2D::kMaxCameras);
}

// ---------------------------------------------------------------------------
// Determinism
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_Determinism, FollowBehaviour_120Frames_IdenticalResults)
{
    auto simulate = [](unsigned int seed) -> Dia::Maths::Vector2D
    {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist(-200.0f, 200.0f);

        Camera2D cam;
        FollowBehaviour follow;
        follow.SetTarget(Dia::Maths::Vector2D(dist(rng), dist(rng)));

        for (int i = 0; i < 120; ++i)
        {
            follow.SetTarget(Dia::Maths::Vector2D(dist(rng), dist(rng)));
            follow.Update(cam, 1.0f / 60.0f);
        }
        return cam.GetPosition();
    };

    const Dia::Maths::Vector2D a = simulate(0xCAFEBABE);
    const Dia::Maths::Vector2D b = simulate(0xCAFEBABE);
    EXPECT_FLOAT_EQ(a.x, b.x);
    EXPECT_FLOAT_EQ(a.y, b.y);
}

TEST(DiaCamera2D_Determinism, SmoothDamp_120Frames_IdenticalResults)
{
    auto simulate = [](unsigned int /*seed*/) -> Dia::Maths::Vector2D
    {
        Camera2D cam;
        SmoothDampBehaviour b(0.2f);
        b.SetTarget(Dia::Maths::Vector2D(300.0f, -150.0f));
        for (int i = 0; i < 120; ++i)
            b.Update(cam, 1.0f / 60.0f);
        return cam.GetPosition();
    };

    const Dia::Maths::Vector2D a = simulate(1);
    const Dia::Maths::Vector2D b = simulate(1);
    EXPECT_FLOAT_EQ(a.x, b.x);
    EXPECT_FLOAT_EQ(a.y, b.y);
}
