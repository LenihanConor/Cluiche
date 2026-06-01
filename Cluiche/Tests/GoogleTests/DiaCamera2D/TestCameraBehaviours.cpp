#include <gtest/gtest.h>

#include <DiaCamera2D/Camera2D.h>
#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaCamera2D/Behaviour/CameraBehaviourRegistry.h>
#include <DiaCamera2D/Behaviour/FollowBehaviour.h>
#include <DiaCamera2D/Behaviour/SmoothDampBehaviour.h>
#include <DiaCamera2D/Behaviour/DeadzoneBehaviour.h>
#include <DiaCamera2D/Behaviour/BoundsClampBehaviour.h>
#include <DiaCamera2D/Behaviour/ScreenShakeBehaviour.h>
#include <DiaCamera2D/Behaviour/ZoomToFitBehaviour.h>
#include <DiaCamera2D/Behaviour/PanBehaviour.h>
#include <DiaCamera2D/Behaviour/ZoomBehaviour.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Camera2D;

// ---------------------------------------------------------------------------
// FollowBehaviour
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_Follow, NoTarget_CameraUnchanged)
{
    Camera2D cam(Dia::Maths::Vector2D(5.0f, 5.0f));
    FollowBehaviour b;
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 5.0f);
}

TEST(DiaCamera2D_Follow, WithTarget_MovesToTarget)
{
    Camera2D cam;
    FollowBehaviour b;
    b.SetTarget(Dia::Maths::Vector2D(100.0f, 200.0f));
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 100.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 200.0f);
}

TEST(DiaCamera2D_Follow, WithTargetAndOffset_AppliesOffset)
{
    Camera2D cam;
    FollowBehaviour b(Dia::Maths::Vector2D(0.0f, -50.0f));
    b.SetTarget(Dia::Maths::Vector2D(100.0f, 200.0f));
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 100.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 150.0f);
}

TEST(DiaCamera2D_Follow, ZeroDt_StillFollows)
{
    Camera2D cam;
    FollowBehaviour b;
    b.SetTarget(Dia::Maths::Vector2D(50.0f, 50.0f));
    b.Update(cam, 0.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 50.0f);
}

// ---------------------------------------------------------------------------
// SmoothDampBehaviour
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_SmoothDamp, NoTarget_CameraUnchanged)
{
    Camera2D cam(Dia::Maths::Vector2D(10.0f, 10.0f));
    SmoothDampBehaviour b;
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 10.0f);
}

TEST(DiaCamera2D_SmoothDamp, ConvergesOverTime)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f));
    SmoothDampBehaviour b(0.15f);
    b.SetTarget(Dia::Maths::Vector2D(100.0f, 0.0f));

    for (int i = 0; i < 120; ++i)
        b.Update(cam, 1.0f / 60.0f);

    EXPECT_NEAR(cam.GetPosition().x, 100.0f, 1.0f);
}

TEST(DiaCamera2D_SmoothDamp, ZeroDt_NoCrash)
{
    Camera2D cam;
    SmoothDampBehaviour b;
    b.SetTarget(Dia::Maths::Vector2D(50.0f, 50.0f));
    b.Update(cam, 0.0f);  // must not divide by zero
    EXPECT_EQ(cam.GetPosition().x, 0.0f);
}

// ---------------------------------------------------------------------------
// DeadzoneBehaviour
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_Deadzone, TargetInsideDeadzone_CameraUnchanged)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f));
    DeadzoneBehaviour b(50.0f, 30.0f);
    b.SetTarget(Dia::Maths::Vector2D(10.0f, 10.0f));  // inside
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 0.0f);
}

TEST(DiaCamera2D_Deadzone, TargetOutsideDeadzone_CameraMoves)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f));
    DeadzoneBehaviour b(50.0f, 30.0f);
    b.SetTarget(Dia::Maths::Vector2D(100.0f, 0.0f));  // far outside
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_GT(cam.GetPosition().x, 0.0f);
}

TEST(DiaCamera2D_Deadzone, TargetExactlyOnBoundary_CameraUnchanged)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f));
    DeadzoneBehaviour b(50.0f, 30.0f);
    b.SetTarget(Dia::Maths::Vector2D(50.0f, 0.0f));  // exactly on edge
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 0.0f);
}

TEST(DiaCamera2D_Deadzone, NoTarget_CameraUnchanged)
{
    Camera2D cam(Dia::Maths::Vector2D(5.0f, 5.0f));
    DeadzoneBehaviour b;
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 5.0f);
}

// ---------------------------------------------------------------------------
// BoundsClampBehaviour
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_BoundsClamp, CameraInsideBounds_Unchanged)
{
    Camera2D cam(Dia::Maths::Vector2D(500.0f, 300.0f), 1.0f);
    Dia::Geometry2D::AARect bounds(Dia::Maths::Vector2D(0,0), Dia::Maths::Vector2D(1920,1080));
    BoundsClampBehaviour b(bounds, Dia::Maths::Vector2D(100.0f, 100.0f));
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 500.0f);
}

TEST(DiaCamera2D_BoundsClamp, CameraOutsideBounds_Clamped)
{
    Camera2D cam(Dia::Maths::Vector2D(2000.0f, 0.0f), 1.0f);
    Dia::Geometry2D::AARect bounds(Dia::Maths::Vector2D(0,0), Dia::Maths::Vector2D(1920,1080));
    BoundsClampBehaviour b(bounds, Dia::Maths::Vector2D(100.0f, 100.0f));
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_LE(cam.GetPosition().x, 1920.0f);
}

TEST(DiaCamera2D_BoundsClamp, ZeroBounds_NoOp)
{
    Camera2D cam(Dia::Maths::Vector2D(999.0f, 999.0f), 1.0f);
    Dia::Geometry2D::AARect zeroBounds;  // zero area
    BoundsClampBehaviour b(zeroBounds);
    b.Update(cam, 1.0f / 60.0f);
    // Must not change position — unbounded world
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 999.0f);
}

// ---------------------------------------------------------------------------
// ScreenShakeBehaviour
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_ScreenShake, NoTrauma_CameraUnchanged)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f));
    ScreenShakeBehaviour b;
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 0.0f);
}

TEST(DiaCamera2D_ScreenShake, WithTrauma_CameraDisplaced)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f));
    ScreenShakeBehaviour b(20.0f, 0.5f);
    b.Trigger(1.0f);
    b.Update(cam, 1.0f / 60.0f);
    // Position should have changed (shake is additive)
    const float dx = cam.GetPosition().x;
    const float dy = cam.GetPosition().y;
    EXPECT_TRUE(dx != 0.0f || dy != 0.0f);
}

TEST(DiaCamera2D_ScreenShake, TraumaDecays)
{
    ScreenShakeBehaviour b(20.0f, 2.0f);
    b.Trigger(1.0f);
    EXPECT_FLOAT_EQ(b.GetTrauma(), 1.0f);

    Camera2D cam;
    for (int i = 0; i < 60; ++i)  // simulate 1 second at 60Hz
        b.Update(cam, 1.0f / 60.0f);

    EXPECT_FLOAT_EQ(b.GetTrauma(), 0.0f);
}

TEST(DiaCamera2D_ScreenShake, TriggerAboveOne_ClampedToOne)
{
    ScreenShakeBehaviour b;
    b.Trigger(0.7f);
    b.Trigger(0.7f);  // sum = 1.4, clamped to 1.0
    EXPECT_FLOAT_EQ(b.GetTrauma(), 1.0f);
}

// ---------------------------------------------------------------------------
// ZoomToFitBehaviour
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_ZoomToFit, NoTargets_CameraUnchanged)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f), 1.0f);
    ZoomToFitBehaviour b(Dia::Maths::Vector2D(1400.0f, 1000.0f));
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(), 1.0f);
}

TEST(DiaCamera2D_ZoomToFit, SingleTarget_CenterOnTarget)
{
    Camera2D cam;
    ZoomToFitBehaviour b(Dia::Maths::Vector2D(1400.0f, 1000.0f), 0.0f, 0.1f, 10.0f);
    Dia::Maths::Vector2D target(200.0f, 300.0f);
    b.SetTargets(&target, 1);
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 200.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 300.0f);
}

TEST(DiaCamera2D_ZoomToFit, AllSamePoint_NoNaN)
{
    Camera2D cam;
    ZoomToFitBehaviour b(Dia::Maths::Vector2D(1400.0f, 1000.0f), 50.0f, 0.5f, 4.0f);
    Dia::Maths::Vector2D targets[3] = {
        Dia::Maths::Vector2D(100.0f, 100.0f),
        Dia::Maths::Vector2D(100.0f, 100.0f),
        Dia::Maths::Vector2D(100.0f, 100.0f)
    };
    b.SetTargets(targets, 3);
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FALSE(std::isnan(cam.GetZoom()));
    EXPECT_FALSE(std::isinf(cam.GetZoom()));
}

TEST(DiaCamera2D_ZoomToFit, ZoomClamped)
{
    Camera2D cam;
    ZoomToFitBehaviour b(Dia::Maths::Vector2D(100.0f, 100.0f), 0.0f, 0.5f, 2.0f);
    // Targets very far apart — would require tiny zoom
    Dia::Maths::Vector2D targets[2] = { Dia::Maths::Vector2D(0.0f, 0.0f), Dia::Maths::Vector2D(10000.0f, 0.0f) };
    b.SetTargets(targets, 2);
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_GE(cam.GetZoom(), 0.5f);
    EXPECT_LE(cam.GetZoom(), 2.0f);
}

// ---------------------------------------------------------------------------
// PanBehaviour
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_Pan, ZeroDelta_CameraUnchanged)
{
    Camera2D cam(Dia::Maths::Vector2D(10.0f, 20.0f));
    PanBehaviour b;
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 10.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 20.0f);
}

TEST(DiaCamera2D_Pan, SetDelta_MovesCamera)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f));
    PanBehaviour b;
    b.SetDelta(Dia::Maths::Vector2D(5.0f, -3.0f));
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x,  5.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, -3.0f);
}

TEST(DiaCamera2D_Pan, DeltaConsumedAfterUpdate)
{
    Camera2D cam;
    PanBehaviour b;
    b.SetDelta(Dia::Maths::Vector2D(10.0f, 0.0f));
    b.Update(cam, 1.0f / 60.0f);
    b.Update(cam, 1.0f / 60.0f);  // second frame — delta should be zero
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 10.0f);  // no additional movement
}

// ---------------------------------------------------------------------------
// ZoomBehaviour
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_Zoom, ZeroInput_ZoomUnchanged)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f), 1.0f);
    ZoomBehaviour b;
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(), 1.0f);
}

TEST(DiaCamera2D_Zoom, PositiveInput_ZoomsIn)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f), 1.0f);
    ZoomBehaviour b(0.1f, 0.25f, 4.0f);
    b.SetInput(1.0f);
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_GT(cam.GetZoom(), 1.0f);
}

TEST(DiaCamera2D_Zoom, NegativeInput_ZoomsOut)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f), 1.0f);
    ZoomBehaviour b(0.1f, 0.25f, 4.0f);
    b.SetInput(-1.0f);
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_LT(cam.GetZoom(), 1.0f);
}

TEST(DiaCamera2D_Zoom, ClampedAtMin)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f), 0.3f);
    ZoomBehaviour b(1.0f, 0.25f, 4.0f);  // high sensitivity
    b.SetInput(-100.0f);  // huge zoom-out
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_GE(cam.GetZoom(), 0.25f);
}

TEST(DiaCamera2D_Zoom, ClampedAtMax)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f), 3.5f);
    ZoomBehaviour b(1.0f, 0.25f, 4.0f);
    b.SetInput(100.0f);  // huge zoom-in
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_LE(cam.GetZoom(), 4.0f);
}

TEST(DiaCamera2D_Zoom, InputConsumedAfterUpdate)
{
    Camera2D cam(Dia::Maths::Vector2D(0.0f, 0.0f), 1.0f);
    ZoomBehaviour b;
    b.SetInput(1.0f);
    b.Update(cam, 1.0f / 60.0f);
    const float zoomAfterFirst = cam.GetZoom();
    b.Update(cam, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(), zoomAfterFirst);  // no change on second frame
}

// ---------------------------------------------------------------------------
// BehaviourFactory
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_Factory, CreateKnownType_ReturnsInstance)
{
    auto* b = CameraBehaviourRegistry::Get().Create(Dia::Core::StringCRC("FollowBehaviour"));
    EXPECT_NE(b, nullptr);
    delete b;
}

TEST(DiaCamera2D_Factory, CreateUnknownType_ReturnsNull)
{
    auto* b = CameraBehaviourRegistry::Get().Create(Dia::Core::StringCRC("NonExistentBehaviour"));
    EXPECT_EQ(b, nullptr);
}

TEST(DiaCamera2D_Factory, AllBuiltinTypesRegistered)
{
    auto& reg = CameraBehaviourRegistry::Get();
    EXPECT_TRUE(reg.IsRegistered(Dia::Core::StringCRC("FollowBehaviour")));
    EXPECT_TRUE(reg.IsRegistered(Dia::Core::StringCRC("SmoothDampBehaviour")));
    EXPECT_TRUE(reg.IsRegistered(Dia::Core::StringCRC("DeadzoneBehaviour")));
    EXPECT_TRUE(reg.IsRegistered(Dia::Core::StringCRC("BoundsClampBehaviour")));
    EXPECT_TRUE(reg.IsRegistered(Dia::Core::StringCRC("ScreenShakeBehaviour")));
    EXPECT_TRUE(reg.IsRegistered(Dia::Core::StringCRC("ZoomToFitBehaviour")));
    EXPECT_TRUE(reg.IsRegistered(Dia::Core::StringCRC("PanBehaviour")));
    EXPECT_TRUE(reg.IsRegistered(Dia::Core::StringCRC("ZoomBehaviour")));
}
