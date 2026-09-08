////////////////////////////////////////////////////////////////////////////////
// Filename: TestLightWidgetsDrawer.cpp
// Tests for Dia::Lighting3D::LightWidgetsDrawer
////////////////////////////////////////////////////////////////////////////////

#ifndef DIA_DEBUG
#define DIA_DEBUG
#endif

#include <gtest/gtest.h>

#include <DiaLighting3DVisualDebugger/LightWidgetsDrawer.h>
#include <DiaLighting3D/Registry/LightRegistry3D.h>
#include <DiaLighting3D/Testing/LightBuilder3D.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    // Count primitives of a specific type in FrameData
    uint32_t CountPrimitivesOfType(Dia::Graphics::FrameData& frameData,
                                   Dia::Graphics::DebugPrimitiveType type)
    {
        uint32_t n = 0;
        const uint32_t total = frameData.GetDebug3DPrimitiveCount();
        for (uint32_t i = 0; i < total; ++i)
        {
            if (frameData.GetDebug3DPrimitive(i).type == type)
                ++n;
        }
        return n;
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// LightWidgetsDrawer
// ---------------------------------------------------------------------------

TEST(LightWidgetsDrawer, LayerName_IsLightWidgets)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    Dia::Lighting3D::LightWidgetsDrawer      drawer(builder.Registry());

    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kLightWidgets);
}

TEST(LightWidgetsDrawer, Draw_PointLight_EmitsSphere)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithPoint("sun", 0.0f, 5.0f, 0.0f);

    Dia::Lighting3D::LightWidgetsDrawer drawer(builder.Registry());

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    const uint32_t sphereCount = CountPrimitivesOfType(frameData,
        Dia::Graphics::DebugPrimitiveType::Sphere3D);
    EXPECT_EQ(sphereCount, 1u);
}

TEST(LightWidgetsDrawer, Draw_PointLight_Colour_IsWarning)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithPoint("sun");

    Dia::Lighting3D::LightWidgetsDrawer drawer(builder.Registry());

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    ASSERT_GE(frameData.GetDebug3DPrimitiveCount(), 1u);
    const Dia::Graphics::DebugPrimitive& prim = frameData.GetDebug3DPrimitive(0);
    ASSERT_EQ(prim.type, Dia::Graphics::DebugPrimitiveType::Sphere3D);
    EXPECT_EQ(prim.sphere3D.colour, Dia::Debug::DebugColourPalette::kWarning);
}

TEST(LightWidgetsDrawer, Draw_SpotLight_EmitsSphereAndArrow)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithSpot("spot0", 1.0f, 2.0f, 3.0f);

    Dia::Lighting3D::LightWidgetsDrawer drawer(builder.Registry());

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    const uint32_t sphereCount = CountPrimitivesOfType(frameData,
        Dia::Graphics::DebugPrimitiveType::Sphere3D);
    const uint32_t rayCount    = CountPrimitivesOfType(frameData,
        Dia::Graphics::DebugPrimitiveType::Ray3D);

    EXPECT_EQ(sphereCount, 1u);
    EXPECT_EQ(rayCount,    1u);
}

TEST(LightWidgetsDrawer, Draw_DirectionalLight_EmitsAnchorSphereAndArrow)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithDirectional("sun.dir", 0.0f, -1.0f, 0.0f);

    Dia::Lighting3D::LightWidgetsDrawer drawer(builder.Registry());

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    const uint32_t sphereCount = CountPrimitivesOfType(frameData,
        Dia::Graphics::DebugPrimitiveType::Sphere3D);
    const uint32_t rayCount = CountPrimitivesOfType(frameData,
        Dia::Graphics::DebugPrimitiveType::Ray3D);
    EXPECT_EQ(sphereCount, 1u);
    EXPECT_EQ(rayCount,    1u);
}

TEST(LightWidgetsDrawer, Draw_DirectionalLight_Colour_IsGoal)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithDirectional("sun.dir");

    Dia::Lighting3D::LightWidgetsDrawer drawer(builder.Registry());

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    // Sphere (anchor) is emitted first, ray second
    ASSERT_GE(frameData.GetDebug3DPrimitiveCount(), 2u);
    const Dia::Graphics::DebugPrimitive& ray = frameData.GetDebug3DPrimitive(1);
    ASSERT_EQ(ray.type, Dia::Graphics::DebugPrimitiveType::Ray3D);
    EXPECT_EQ(ray.ray3D.colour, Dia::Debug::DebugColourPalette::kGoal);
}

TEST(LightWidgetsDrawer, Draw_PointLightsDisabled_NoPrimitivesForPoint)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithPoint("point0");

    Dia::Lighting3D::LightWidgetsDrawer drawer(builder.Registry());
    drawer.SetShowPointLights(false);

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    const uint32_t sphereCount = CountPrimitivesOfType(frameData,
        Dia::Graphics::DebugPrimitiveType::Sphere3D);
    EXPECT_EQ(sphereCount, 0u);
}

TEST(LightWidgetsDrawer, Draw_Disabled_NoPrimitives)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithPoint("point0").WithDirectional("dir0").WithSpot("spot0");

    Dia::Lighting3D::LightWidgetsDrawer drawer(builder.Registry());
    drawer.SetEnabled(false);

    // SetEnabled(false) → manager skips Draw() for disabled layers.
    Dia::Graphics::FrameData frameData;
    Dia::Debug::DebugLayerManager layerManager;
    layerManager.Register(&drawer);
    layerManager.DisableLayer(drawer.GetLayerName());
    layerManager.Draw(frameData);

    EXPECT_EQ(frameData.GetDebug3DPrimitiveCount(), 0u);
}

TEST(LightWidgetsDrawer, Draw_EmptyRegistry_NoPrimitives)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;

    Dia::Lighting3D::LightWidgetsDrawer drawer(builder.Registry());

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    EXPECT_EQ(frameData.GetDebug3DPrimitiveCount(), 0u);
}

TEST(LightWidgetsDrawer, Draw_SpotLightsDisabled_NoPrimitivesForSpot)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithSpot("spot0");

    Dia::Lighting3D::LightWidgetsDrawer drawer(builder.Registry());
    drawer.SetShowSpotLights(false);

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    EXPECT_EQ(frameData.GetDebug3DPrimitiveCount(), 0u);
}

TEST(LightWidgetsDrawer, Draw_DirectionalLightsDisabled_NoPrimitivesForDirectional)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithDirectional("dir0");

    Dia::Lighting3D::LightWidgetsDrawer drawer(builder.Registry());
    drawer.SetShowDirectionalLights(false);

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    EXPECT_EQ(frameData.GetDebug3DPrimitiveCount(), 0u);
}

TEST(LightWidgetsDrawer, Draw_DisabledLight_Skipped)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithPoint("enabled_point").WithPoint("disabled_point").PointDisabled();

    Dia::Lighting3D::LightWidgetsDrawer drawer(builder.Registry());

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    // Only 1 of the 2 point lights is enabled — expect exactly 1 sphere
    const uint32_t sphereCount = CountPrimitivesOfType(frameData,
        Dia::Graphics::DebugPrimitiveType::Sphere3D);
    EXPECT_EQ(sphereCount, 1u);
}

TEST(LightWidgetsDrawer, Draw_MultiplePointLights_EmitsOneSphereEach)
{
    Dia::Lighting3D::Testing::LightBuilder3D builder;
    builder.WithPoint("p0").WithPoint("p1").WithPoint("p2");

    Dia::Lighting3D::LightWidgetsDrawer drawer(builder.Registry());

    Dia::Graphics::FrameData frameData;
    drawer.Draw(frameData);

    const uint32_t sphereCount = CountPrimitivesOfType(frameData,
        Dia::Graphics::DebugPrimitiveType::Sphere3D);
    EXPECT_EQ(sphereCount, 3u);
}
