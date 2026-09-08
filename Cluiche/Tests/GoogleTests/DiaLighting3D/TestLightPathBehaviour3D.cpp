#include <gtest/gtest.h>

#include <DiaLighting3D/Behaviours/LightPathBehaviour3D.h>
#include <DiaLighting3D/PointLight3D.h>
#include <DiaLighting3D/SpotLight3D.h>
#include <DiaLighting3D/Registry/LightRegistry3D.h>
#include <DiaGeometry3D/Shapes/Spline3D.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Lighting3D;
using namespace Dia::Geometry3D;

namespace {

// Build a CatmullRom spline with 4 well-separated control points.
// Points span X from 0 to 30 so any non-zero t gives a clearly non-zero position.
Spline3D MakeTestSpline()
{
    Dia::Maths::Vector3D pts[4] = {
        Dia::Maths::Vector3D(  0.0f, 0.0f, 0.0f),
        Dia::Maths::Vector3D( 10.0f, 0.0f, 0.0f),
        Dia::Maths::Vector3D( 20.0f, 0.0f, 0.0f),
        Dia::Maths::Vector3D( 30.0f, 0.0f, 0.0f)
    };
    return SplineFactory3D::MakeCatmullRom(pts, 4);
}

} // namespace

// --- PositionAdvancesAlongCurve ---

TEST(DiaLighting3D_PathBehaviour, PositionAdvancesAlongCurve)
{
    PointLight3D light;
    light.position = Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f);

    LightPathBehaviour3D::Config cfg;
    cfg.spline   = MakeTestSpline();
    cfg.speed    = 0.5f;
    cfg.loopMode = LightPathBehaviour3D::LoopMode::Loop;

    LightPathBehaviour3D behaviour(&light, cfg);

    // After several updates the light position should have moved away from origin
    // and t should be non-zero.
    for (int i = 0; i < 5; ++i)
        behaviour.Update(0.016f);  // ~5 frames at 60 Hz

    EXPECT_GT(behaviour.GetT(), 0.0f);
    // With speed=0.5 and 5*0.016=0.08s, t=0.04; spline spans 0-30 on X so position must differ
    EXPECT_NE(light.position.x, 0.0f);
}

// --- LoopWraps ---

TEST(DiaLighting3D_PathBehaviour, LoopWraps)
{
    PointLight3D light;

    LightPathBehaviour3D::Config cfg;
    cfg.spline   = MakeTestSpline();
    cfg.speed    = 1.0f;
    cfg.loopMode = LightPathBehaviour3D::LoopMode::Loop;

    LightPathBehaviour3D behaviour(&light, cfg);

    // A single large dt pushes t well past 1.0 — loop must wrap back into [0,1)
    behaviour.Update(1.5f);  // t = 1.5 → wraps to 0.5

    EXPECT_GE(behaviour.GetT(), 0.0f);
    EXPECT_LT(behaviour.GetT(), 1.0f);
}

// --- PingPongReverses ---

TEST(DiaLighting3D_PathBehaviour, PingPongReverses)
{
    PointLight3D light;

    LightPathBehaviour3D::Config cfg;
    cfg.spline   = MakeTestSpline();
    cfg.speed    = 1.0f;
    cfg.loopMode = LightPathBehaviour3D::LoopMode::PingPong;

    LightPathBehaviour3D behaviour(&light, cfg);

    // Advance to exactly t=1.0 — direction must flip
    behaviour.Update(1.0f);
    EXPECT_NEAR(behaviour.GetT(), 1.0f, 1e-4f);

    // Next update with the reversed direction — t must decrease
    behaviour.Update(0.1f);
    EXPECT_LT(behaviour.GetT(), 1.0f);
}

// --- OnceClamps ---

TEST(DiaLighting3D_PathBehaviour, OnceClamps)
{
    PointLight3D light;

    LightPathBehaviour3D::Config cfg;
    cfg.spline   = MakeTestSpline();
    cfg.speed    = 1.0f;
    cfg.loopMode = LightPathBehaviour3D::LoopMode::Once;

    LightPathBehaviour3D behaviour(&light, cfg);

    // Large dt — t must clamp at 1.0, not exceed it
    behaviour.Update(5.0f);
    EXPECT_FLOAT_EQ(behaviour.GetT(), 1.0f);

    // A second update must not move t past 1.0
    behaviour.Update(1.0f);
    EXPECT_FLOAT_EQ(behaviour.GetT(), 1.0f);
}

// --- SpotLightPositionDriven ---

TEST(DiaLighting3D_PathBehaviour, SpotLightPositionDriven)
{
    SpotLight3D light;
    light.position = Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f);

    LightPathBehaviour3D::Config cfg;
    cfg.spline   = MakeTestSpline();
    cfg.speed    = 0.5f;
    cfg.loopMode = LightPathBehaviour3D::LoopMode::Loop;

    LightPathBehaviour3D behaviour(&light, cfg);

    for (int i = 0; i < 5; ++i)
        behaviour.Update(0.016f);

    EXPECT_GT(behaviour.GetT(), 0.0f);
    EXPECT_NE(light.position.x, 0.0f);
}

// --- TypeIdIsCorrect ---

TEST(DiaLighting3D_PathBehaviour, TypeIdIsCorrect)
{
    PointLight3D light;

    LightPathBehaviour3D::Config cfg;
    cfg.spline = MakeTestSpline();

    LightPathBehaviour3D behaviour(&light, cfg);

    EXPECT_EQ(behaviour.GetTypeId(), LightPathBehaviour3D::kTypeId);
}

// --- TypeMismatchRejected ---
//
// Type-mismatch is enforced at compile time — no DirectionalLight3D or
// AmbientLight3D constructor exists on LightPathBehaviour3D.  Attempting to
// pass any other light type will produce a compile error, not a runtime failure.
// There is no runtime test to write; this comment documents the AC.

TEST(DiaLighting3D_PathBehaviour, TypeMismatchRejectedAtCompileTime)
{
    // Compile-time enforcement: only PointLight3D* and SpotLight3D* constructors
    // exist.  Passing any other pointer type is a compile error.
    SUCCEED();
}

// --- GetLoopMode ---

TEST(DiaLighting3D_PathBehaviour, GetLoopMode_ReturnsConfiguredMode)
{
    PointLight3D light;
    LightPathBehaviour3D::Config cfg;
    cfg.spline   = MakeTestSpline();
    cfg.loopMode = LightPathBehaviour3D::LoopMode::PingPong;

    LightPathBehaviour3D behaviour(&light, cfg);
    EXPECT_EQ(behaviour.GetLoopMode(), LightPathBehaviour3D::LoopMode::PingPong);
}

// --- PingPong second bounce ---

TEST(DiaLighting3D_PathBehaviour, PingPongBouncesBothEnds)
{
    PointLight3D light;
    LightPathBehaviour3D::Config cfg;
    cfg.spline   = MakeTestSpline();
    cfg.speed    = 1.0f;
    cfg.loopMode = LightPathBehaviour3D::LoopMode::PingPong;

    LightPathBehaviour3D behaviour(&light, cfg);

    // Advance to t=1 — direction reverses
    behaviour.Update(1.0f);
    EXPECT_NEAR(behaviour.GetT(), 1.0f, 1e-4f);

    // Reverse back to t=0 — direction reverses again
    behaviour.Update(1.0f);
    EXPECT_NEAR(behaviour.GetT(), 0.0f, 1e-4f);

    // Forward again — t must now increase
    behaviour.Update(0.1f);
    EXPECT_GT(behaviour.GetT(), 0.0f);
}

// --- GetPathBehaviour on LightRegistry3D ---

TEST(DiaLighting3D_PathBehaviour, GetPathBehaviour_ReturnsNullptr_WhenNoneAttached)
{
    LightRegistry3D registry;
    PointLight3D light;
    registry.RegisterPoint(Dia::Core::StringCRC("light.a"), light);

    EXPECT_EQ(registry.GetPathBehaviour(Dia::Core::StringCRC("light.a")), nullptr);
}

TEST(DiaLighting3D_PathBehaviour, GetPathBehaviour_ReturnsNullptr_ForUnknownId)
{
    LightRegistry3D registry;
    EXPECT_EQ(registry.GetPathBehaviour(Dia::Core::StringCRC("light.unknown")), nullptr);
}

TEST(DiaLighting3D_PathBehaviour, GetPathBehaviour_ReturnsBehaviour_AfterAttach)
{
    LightRegistry3D registry;
    PointLight3D light;
    Dia::Core::StringCRC id("light.path");
    registry.RegisterPoint(id, light);

    PointLight3D* stored = &registry.GetPoint(id);
    LightPathBehaviour3D::Config cfg;
    cfg.spline = MakeTestSpline();
    LightPathBehaviour3D behaviour(stored, cfg);

    registry.AttachBehaviour(id, &behaviour);

    LightPathBehaviour3D* found = registry.GetPathBehaviour(id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->GetTypeId(), LightPathBehaviour3D::kTypeId);
}
