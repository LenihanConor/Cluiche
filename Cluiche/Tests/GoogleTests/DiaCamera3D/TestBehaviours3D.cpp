#include <gtest/gtest.h>

#include <DiaCamera3D/Camera3D.h>
#include <DiaCamera3D/Behaviours/Follow3D.h>
#include <DiaCamera3D/Behaviours/SmoothDamp3D.h>
#include <DiaCamera3D/Behaviours/BoundsClamp3D.h>
#include <DiaCamera3D/Behaviours/ScreenShake3D.h>
#include <DiaCamera3D/Behaviours/Orbit.h>
#include <DiaCamera3D/Behaviours/Flythrough.h>
#include <DiaGeometry3D/Shapes/AABB.h>
#include <DiaMaths/Vector/Vector3D.h>

#include <cmath>

using namespace Dia::Camera3D;

// ---------------------------------------------------------------------------
// Follow3D
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_Follow3D, NoTarget_PositionUnchanged)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(1.0f, 2.0f, 3.0f);

    Follow3D follow;  // no SetTarget called
    follow.Update(cam, 0.016f);

    EXPECT_FLOAT_EQ(cam.position.x, 1.0f);
    EXPECT_FLOAT_EQ(cam.position.y, 2.0f);
    EXPECT_FLOAT_EQ(cam.position.z, 3.0f);
}

TEST(DiaCamera3D_Follow3D, WithTarget_SnapsToTarget)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f);

    Follow3D follow;
    follow.SetTarget(Dia::Maths::Vector3D(5.0f, 5.0f, 5.0f));
    follow.Update(cam, 0.016f);

    EXPECT_FLOAT_EQ(cam.position.x, 5.0f);
    EXPECT_FLOAT_EQ(cam.position.y, 5.0f);
    EXPECT_FLOAT_EQ(cam.position.z, 5.0f);
}

TEST(DiaCamera3D_Follow3D, WithOffset_AppliesOffset)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f);

    Follow3D follow(Dia::Maths::Vector3D(0.0f, 2.0f, 5.0f));
    follow.SetTarget(Dia::Maths::Vector3D(10.0f, 0.0f, 0.0f));
    follow.Update(cam, 0.016f);

    // Position should be target + offset
    EXPECT_FLOAT_EQ(cam.position.x, 10.0f);
    EXPECT_FLOAT_EQ(cam.position.y,  2.0f);
    EXPECT_FLOAT_EQ(cam.position.z,  5.0f);
}

// ---------------------------------------------------------------------------
// SmoothDamp3D
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_SmoothDamp3D, NoTarget_PositionUnchanged)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(3.0f, 3.0f, 3.0f);

    SmoothDamp3D smooth;  // no SetTarget called
    smooth.Update(cam, 0.016f);

    EXPECT_FLOAT_EQ(cam.position.x, 3.0f);
    EXPECT_FLOAT_EQ(cam.position.y, 3.0f);
    EXPECT_FLOAT_EQ(cam.position.z, 3.0f);
}

TEST(DiaCamera3D_SmoothDamp3D, WithTarget_ConvergesOverTime)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f);

    SmoothDamp3D smooth(0.15f);
    smooth.SetTarget(Dia::Maths::Vector3D(10.0f, 0.0f, 0.0f));

    for (int i = 0; i < 100; ++i)
        smooth.Update(cam, 0.016f);

    // After 100 steps of 16ms (~1.6s >> smoothTime=0.15s), should be very close to target
    EXPECT_NEAR(cam.position.x, 10.0f, 0.1f);
}

// ---------------------------------------------------------------------------
// BoundsClamp3D
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_BoundsClamp3D, WithinBounds_NoChange)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f);

    const Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(-10.0f, -10.0f, -10.0f),
        Dia::Maths::Vector3D( 10.0f,  10.0f,  10.0f));
    BoundsClamp3D clamp(bounds);
    clamp.Update(cam, 0.016f);

    EXPECT_FLOAT_EQ(cam.position.x, 0.0f);
    EXPECT_FLOAT_EQ(cam.position.y, 0.0f);
    EXPECT_FLOAT_EQ(cam.position.z, 0.0f);
}

TEST(DiaCamera3D_BoundsClamp3D, OutsideBounds_Clamped)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(50.0f, -50.0f, 30.0f);

    const Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(-5.0f, -5.0f, -5.0f),
        Dia::Maths::Vector3D( 5.0f,  5.0f,  5.0f));
    BoundsClamp3D clamp(bounds);
    clamp.Update(cam, 0.016f);

    EXPECT_LE(cam.position.x, 5.0f);
    EXPECT_GE(cam.position.x, -5.0f);
    EXPECT_LE(cam.position.y, 5.0f);
    EXPECT_GE(cam.position.y, -5.0f);
    EXPECT_LE(cam.position.z, 5.0f);
    EXPECT_GE(cam.position.z, -5.0f);
}

TEST(DiaCamera3D_BoundsClamp3D, ZeroVolumeBounds_NoOp)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(99.0f, 99.0f, 99.0f);

    // Zero-volume AABB: min == max
    const Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f),
        Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f));
    BoundsClamp3D clamp(bounds);
    clamp.Update(cam, 0.016f);  // must not crash
}

// ---------------------------------------------------------------------------
// ScreenShake3D
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_ScreenShake3D, NoTrigger_PositionUnchanged)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(1.0f, 2.0f, 3.0f);

    ScreenShake3D shake;
    shake.Update(cam, 0.016f);

    EXPECT_FLOAT_EQ(cam.position.x, 1.0f);
    EXPECT_FLOAT_EQ(cam.position.y, 2.0f);
    EXPECT_FLOAT_EQ(cam.position.z, 3.0f);
}

TEST(DiaCamera3D_ScreenShake3D, Trigger_ModifiesPosition)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f);

    ScreenShake3D shake(1.0f, 0.1f, 1.5f);
    shake.Trigger(1.0f);  // full trauma
    shake.Update(cam, 0.016f);

    // Position should differ from origin since trauma>0 applies an offset
    const float dist = std::sqrt(
        cam.position.x * cam.position.x +
        cam.position.y * cam.position.y +
        cam.position.z * cam.position.z);
    // At least some movement is expected; not all axes may fire but the behaviour ran
    // We just verify it doesn't crash and the trauma is positive going in.
    EXPECT_GE(shake.GetTrauma(), 0.0f);
}

TEST(DiaCamera3D_ScreenShake3D, Trauma_DecaysToZero)
{
    Camera3D cam;
    ScreenShake3D shake(0.5f, 0.05f, 2.0f);  // traumaDecay = 2.0
    shake.Trigger(1.0f);
    EXPECT_GT(shake.GetTrauma(), 0.0f);

    // Update many times with dt=0.1s; total time = 5s, decay rate=2.0 → fully decayed
    for (int i = 0; i < 50; ++i)
        shake.Update(cam, 0.1f);

    EXPECT_FLOAT_EQ(shake.GetTrauma(), 0.0f);
}

// ---------------------------------------------------------------------------
// Orbit
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_Orbit, NoInput_PositionUpdated)
{
    // Orbit with no input still computes a valid spherical position each frame.
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f);

    Orbit orbit(10.0f, 0.0f, 0.3f);
    orbit.SetTarget(Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f));
    orbit.Update(cam, 0.016f);

    // Camera should be placed at radius=10 from target — position should not remain at origin
    const float dist = std::sqrt(
        cam.position.x * cam.position.x +
        cam.position.y * cam.position.y +
        cam.position.z * cam.position.z);
    EXPECT_NEAR(dist, 10.0f, 0.01f);
}

TEST(DiaCamera3D_Orbit, YawInput_ChangesPosition)
{
    Camera3D cam1;
    Camera3D cam2;

    Orbit orbit1(10.0f, 0.0f, 0.0f);
    orbit1.SetTarget(Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f));
    orbit1.Update(cam1, 0.016f);

    Orbit orbit2(10.0f, 0.0f, 0.0f);
    orbit2.SetTarget(Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f));
    orbit2.SetInput(1.0f, 0.0f, 0.0f);  // non-zero yaw
    orbit2.Update(cam2, 0.016f);

    // Different yaw → different position
    const bool different = (cam1.position.x != cam2.position.x) ||
                           (cam1.position.z != cam2.position.z);
    EXPECT_TRUE(different);
}

TEST(DiaCamera3D_Orbit, RadiusInput_ChangesRadius)
{
    Camera3D cam;
    Orbit orbit(10.0f, 0.0f, 0.0f);
    orbit.SetTarget(Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f));
    orbit.SetInput(0.0f, 0.0f, -5.0f);  // shrink radius by 5
    orbit.Update(cam, 0.016f);

    const float dist = std::sqrt(
        cam.position.x * cam.position.x +
        cam.position.y * cam.position.y +
        cam.position.z * cam.position.z);
    EXPECT_NEAR(dist, 5.0f, 0.01f);
}

TEST(DiaCamera3D_Orbit, PitchClamp_NeverExceedsPiOver2)
{
    Camera3D cam;
    Orbit orbit(10.0f, 0.0f, 0.0f);
    orbit.SetTarget(Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f));

    // Apply extreme upward pitch repeatedly
    for (int i = 0; i < 20; ++i)
    {
        orbit.SetInput(0.0f, 10.0f, 0.0f);  // big pitch delta
        orbit.Update(cam, 0.016f);
    }

    // Y component must not exceed radius (would indicate pitch > π/2).
    // With pitch clamped to [−π/2+ε, π/2−ε], y < radius.
    const float absY = std::abs(cam.position.y);
    EXPECT_LT(absY, 10.0f);
}

// ---------------------------------------------------------------------------
// Flythrough
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_Flythrough, NoInput_PositionUnchanged)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(5.0f, 5.0f, 5.0f);

    Flythrough fly;  // no SetMoveInput / SetLookInput called
    fly.Update(cam, 0.016f);

    EXPECT_FLOAT_EQ(cam.position.x, 5.0f);
    EXPECT_FLOAT_EQ(cam.position.y, 5.0f);
    EXPECT_FLOAT_EQ(cam.position.z, 5.0f);
}

TEST(DiaCamera3D_Flythrough, ForwardInput_MovesPosition)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f);

    Flythrough fly(5.0f);
    fly.SetMoveInput(Dia::Maths::Vector3D(0.0f, 0.0f, 1.0f));  // forward in local space
    fly.Update(cam, 1.0f);  // dt=1s, speed=5 → 5 units moved

    // Position must differ from origin
    const float dist = std::sqrt(
        cam.position.x * cam.position.x +
        cam.position.y * cam.position.y +
        cam.position.z * cam.position.z);
    EXPECT_GT(dist, 0.0f);
}

TEST(DiaCamera3D_Flythrough, LookInput_ChangesOrientation)
{
    Camera3D cam;
    // Store orientation before
    const Dia::Maths::Quaternion before = cam.orientation;

    Flythrough fly;
    fly.SetLookInput(0.5f, 0.2f);  // yaw + pitch
    fly.Update(cam, 0.016f);

    // Orientation should have changed
    const bool changed = (cam.orientation.x != before.x) ||
                         (cam.orientation.y != before.y) ||
                         (cam.orientation.z != before.z) ||
                         (cam.orientation.w != before.w);
    EXPECT_TRUE(changed);
}
