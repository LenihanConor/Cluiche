////////////////////////////////////////////////////////////////////////////////
// Filename: TestLightPathArcDrawer.cpp
// Tests for Dia::Lighting3D::LightPathArcDrawer
////////////////////////////////////////////////////////////////////////////////

#ifndef DIA_DEBUG
#define DIA_DEBUG
#endif

#include <gtest/gtest.h>

#include <DiaLighting3DVisualDebugger/LightPathArcDrawer.h>
#include <DiaLighting3D/Registry/LightRegistry3D.h>
#include <DiaLighting3D/Behaviours/LightPathBehaviour3D.h>
#include <DiaLighting3D/Testing/LightBuilder3D.h>
#include <DiaGeometry3D/Shapes/Spline3D.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Lighting3D;
using namespace Dia::Geometry3D;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
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
} // anonymous namespace

// ---------------------------------------------------------------------------
// LightPathArcDrawer
// ---------------------------------------------------------------------------

TEST(LightPathArcDrawer, LayerName_IsLightPathArc)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Debug::DebugLayerManager            manager;
    LightPathArcDrawer                       drawer(builder.Registry(), manager);

    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kLightPathArc);
}

TEST(LightPathArcDrawer, Draw_LightWithPath_EmitsArcLines)
{
    // Register a point light and attach a path behaviour
    LightRegistry3D registry;
    const Dia::Core::StringCRC id("p0");
    Dia::Lighting3D::PointLight3D light;
    registry.RegisterPoint(id, light);

    PointLight3D* stored = &registry.GetPoint(id);
    LightPathBehaviour3D::Config cfg;
    cfg.spline = MakeTestSpline();
    cfg.speed  = 0.0f;
    LightPathBehaviour3D behaviour(stored, cfg);
    registry.AttachBehaviour(id, &behaviour);

    Dia::Debug::DebugLayerManager manager;
    LightPathArcDrawer            drawer(registry, manager);

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    // Default mArcSamples == 32 → 32 line segments emitted
    EXPECT_EQ(frameData.GetDebug3DPrimitiveCount(), static_cast<uint32_t>(32));
}

TEST(LightPathArcDrawer, Draw_LightNoBehaviour_NoPrimitives)
{
    // Spot + directional lights with no path behaviour attached
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithSpot("spot0").WithDirectional("dir0");

    Dia::Debug::DebugLayerManager manager;
    LightPathArcDrawer            drawer(builder.Registry(), manager);

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    EXPECT_EQ(frameData.GetDebug3DPrimitiveCount(), 0u);
}

TEST(LightPathArcDrawer, Draw_ArcColour_PointLight_IsHealthy)
{
    LightRegistry3D registry;
    const Dia::Core::StringCRC id("p1");
    Dia::Lighting3D::PointLight3D light;
    registry.RegisterPoint(id, light);

    PointLight3D* stored = &registry.GetPoint(id);
    LightPathBehaviour3D::Config cfg;
    cfg.spline = MakeTestSpline();
    cfg.speed  = 0.0f;
    LightPathBehaviour3D behaviour(stored, cfg);
    registry.AttachBehaviour(id, &behaviour);

    Dia::Debug::DebugLayerManager manager;
    LightPathArcDrawer            drawer(registry, manager);

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    ASSERT_GE(frameData.GetDebug3DPrimitiveCount(), 1u);
    const Dia::Graphics::DebugPrimitive& prim = frameData.GetDebug3DPrimitive(0);
    ASSERT_EQ(prim.type, Dia::Graphics::DebugPrimitiveType::Line3D);
    EXPECT_EQ(prim.line3D.colour, Dia::Debug::DebugColourPalette::kHealthy);
}

TEST(LightPathArcDrawer, Draw_ArcSamples_MatchesSliderValue)
{
    LightRegistry3D registry;
    const Dia::Core::StringCRC id("p2");
    Dia::Lighting3D::PointLight3D light;
    registry.RegisterPoint(id, light);

    PointLight3D* stored = &registry.GetPoint(id);
    LightPathBehaviour3D::Config cfg;
    cfg.spline = MakeTestSpline();
    cfg.speed  = 0.0f;
    LightPathBehaviour3D behaviour(stored, cfg);
    registry.AttachBehaviour(id, &behaviour);

    Dia::Debug::DebugLayerManager manager;
    LightPathArcDrawer            drawer(registry, manager);
    drawer.SetArcSamples(16);

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    EXPECT_EQ(frameData.GetDebug3DPrimitiveCount(), static_cast<uint32_t>(16));
}

TEST(LightPathArcDrawer, Draw_Disabled_NoPrimitives)
{
    LightRegistry3D registry;
    const Dia::Core::StringCRC id("p3");
    Dia::Lighting3D::PointLight3D light;
    registry.RegisterPoint(id, light);

    PointLight3D* stored = &registry.GetPoint(id);
    LightPathBehaviour3D::Config cfg;
    cfg.spline = MakeTestSpline();
    cfg.speed  = 0.0f;
    LightPathBehaviour3D behaviour(stored, cfg);
    registry.AttachBehaviour(id, &behaviour);

    Dia::Debug::DebugLayerManager manager;
    LightPathArcDrawer            drawer(registry, manager);
    drawer.SetEnabled(false);

    Dia::Graphics::FrameData frameData;
    // SetEnabled controls the IVisualDebugger enabled flag.
    // Draw() itself does not check IsEnabled() — the DebugLayerManager does.
    // Verify via the manager path: DisableLayer skips Draw for the disabled layer.
    Dia::Debug::DebugLayerManager layerManager;
    layerManager.Register(&drawer);
    layerManager.DisableLayer(drawer.GetLayerName());
    layerManager.Draw(frameData);

    EXPECT_EQ(frameData.GetDebug3DPrimitiveCount(), 0u);
}

TEST(LightPathArcDrawer, Draw_EmptyRegistry_NoPrimitives)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    // No lights registered

    Dia::Debug::DebugLayerManager manager;
    LightPathArcDrawer            drawer(builder.Registry(), manager);

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    EXPECT_EQ(frameData.GetDebug3DPrimitiveCount(), 0u);
}

// ---------------------------------------------------------------------------
// LightRegistry3D — path behaviour queries
// ---------------------------------------------------------------------------

TEST(LightRegistry3D, GetPathBehaviour_ReturnsAttached)
{
    LightRegistry3D registry;
    const Dia::Core::StringCRC id("light.path");
    Dia::Lighting3D::PointLight3D light;
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

TEST(LightRegistry3D, GetPathBehaviour_UnknownId_ReturnsNull)
{
    LightRegistry3D registry;

    EXPECT_EQ(registry.GetPathBehaviour(Dia::Core::StringCRC("light.unknown")), nullptr);
}

TEST(LightRegistry3D, GetPathBehaviour_NoBehaviour_ReturnsNull)
{
    LightRegistry3D registry;
    const Dia::Core::StringCRC id("light.nobehaviour");
    Dia::Lighting3D::PointLight3D light;
    registry.RegisterPoint(id, light);

    EXPECT_EQ(registry.GetPathBehaviour(id), nullptr);
}

TEST(LightPathArcDrawer, Draw_ArcColour_SpotLight_IsHealthy)
{
    LightRegistry3D registry;
    const Dia::Core::StringCRC id("spot.arc");
    Dia::Lighting3D::SpotLight3D spotLight;
    registry.RegisterSpot(id, spotLight);

    SpotLight3D* stored = &registry.GetSpot(id);
    LightPathBehaviour3D::Config cfg;
    cfg.spline = MakeTestSpline();
    cfg.speed  = 0.0f;
    LightPathBehaviour3D behaviour(stored, cfg);
    registry.AttachBehaviour(id, &behaviour);

    Dia::Debug::DebugLayerManager manager;
    LightPathArcDrawer            drawer(registry, manager);
    drawer.SetArcSamples(1);

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    ASSERT_GE(frameData.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_EQ(frameData.GetDebug3DPrimitive(0).line3D.colour,
              Dia::Debug::DebugColourPalette::kHealthy);
}

TEST(LightPathArcDrawer, Draw_ArcSamples_MinBoundary)
{
    LightRegistry3D registry;
    const Dia::Core::StringCRC id("p.min");
    Dia::Lighting3D::PointLight3D light;
    registry.RegisterPoint(id, light);

    PointLight3D* stored = &registry.GetPoint(id);
    LightPathBehaviour3D::Config cfg;
    cfg.spline = MakeTestSpline();
    cfg.speed  = 0.0f;
    LightPathBehaviour3D behaviour(stored, cfg);
    registry.AttachBehaviour(id, &behaviour);

    Dia::Debug::DebugLayerManager manager;
    LightPathArcDrawer            drawer(registry, manager);
    drawer.SetArcSamples(8);

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    EXPECT_EQ(frameData.GetDebug3DPrimitiveCount(), static_cast<uint32_t>(8));
}

TEST(LightPathArcDrawer, Draw_ArcSamples_MaxBoundary)
{
    LightRegistry3D registry;
    const Dia::Core::StringCRC id("p.max");
    Dia::Lighting3D::PointLight3D light;
    registry.RegisterPoint(id, light);

    PointLight3D* stored = &registry.GetPoint(id);
    LightPathBehaviour3D::Config cfg;
    cfg.spline = MakeTestSpline();
    cfg.speed  = 0.0f;
    LightPathBehaviour3D behaviour(stored, cfg);
    registry.AttachBehaviour(id, &behaviour);

    Dia::Debug::DebugLayerManager manager;
    LightPathArcDrawer            drawer(registry, manager);
    drawer.SetArcSamples(64);

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    EXPECT_EQ(frameData.GetDebug3DPrimitiveCount(), static_cast<uint32_t>(64));
}
