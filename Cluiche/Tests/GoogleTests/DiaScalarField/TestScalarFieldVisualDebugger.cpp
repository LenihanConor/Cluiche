// Suite: ScalarFieldVisualDebugger

#include <gtest/gtest.h>
#include <DiaScalarField/Adaptors/ScalarFieldOverlay.h>
#include <DiaScalarField/SquareFieldTopology.h>
#include <DiaScalarField/HexFieldTopology.h>
#include <DiaScalarField/UniformDecayPolicy.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <cstdint>

using namespace Dia::ScalarField;
using namespace Dia::ScalarField::Adaptors;

namespace {

struct MockDebugDraw : Dia::Core::IDebugDraw
{
    int             rectCount      = 0;
    int             rayCount       = 0;
    int             triangleCount  = 0;
    Dia::Core::RGBA lastFillColour = Dia::Core::RGBA(0, 0, 0, 0);
    Dia::Maths::Vector2D lastRectMin{ 0.0f, 0.0f };
    mutable Dia::Maths::Vector2D mousePixel{ 0.0f, 0.0f };

    void RequestDraw(const Dia::Maths::Vector2D&, float,
                     Dia::Core::RGBA, Dia::Core::RGBA) override {}
    void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&,
                     Dia::Core::RGBA) override {}
    void RequestDrawPoint(const Dia::Maths::Vector2D&, Dia::Core::RGBA) override {}
    void RequestDrawRect(const Dia::Maths::Vector2D& min, const Dia::Maths::Vector2D&,
                         Dia::Core::RGBA, Dia::Core::RGBA fill) override
    {
        ++rectCount;
        lastFillColour = fill;
        lastRectMin = min;
    }
    void RequestDrawArc(const Dia::Maths::Vector2D&, float, float, float,
                        Dia::Core::RGBA) override {}
    void RequestDrawRay(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&,
                        float, Dia::Core::RGBA) override
    {
        ++rayCount;
    }
    void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&,
                     const Dia::Maths::Vector2D&,
                     Dia::Core::RGBA, Dia::Core::RGBA) override
    {
        ++triangleCount;
    }
    void RequestDrawText(const Dia::Maths::Vector2D&, const char*,
                         float, Dia::Core::RGBA) override {}
    void RequestDrawLine3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&,
                           Dia::Core::RGBA) override {}
    void RequestDrawRay3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&,
                          float, Dia::Core::RGBA) override {}
    void RequestDrawBox3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&,
                          Dia::Core::RGBA) override {}
    void RequestDrawSphere3D(const Dia::Maths::Vector3D&, float,
                             Dia::Core::RGBA) override {}
    void RequestDrawArrow3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&,
                            float, float, Dia::Core::RGBA) override {}
    uint32_t DroppedCount() const override { return 0; }
    const Dia::Maths::Vector2D& GetMousePixel() const override { return mousePixel; }
};

} // namespace

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static SquareScalarField MakeSquareField(int w, int h)
{
    SquareFieldTopology topo(w, h, SquareConnectivity::k8Connected);
    UniformDecayPolicy  policy(UniformDecayParams{ 0.0f, 0.0f });
    return SquareScalarField(topo, policy);
}

static HexScalarField MakeHexField(int radius)
{
    HexFieldTopology   topo(radius);
    UniformDecayPolicy policy(UniformDecayParams{ 0.0f, 0.0f });
    return HexScalarField(topo, policy);
}

static Dia::Core::StringCRC kHeatmapLayer { "test.sf.heatmap" };
static Dia::Core::StringCRC kGradLayer    { "test.sf.gradient" };

// ===========================================================================
// ScalarFieldVisualDebugger tests — Heatmap
// ===========================================================================

TEST(ScalarFieldVisualDebugger, HeatmapOverlay_Construct_DoesNotCrash)
{
    auto field = MakeSquareField(3, 3);
    EXPECT_NO_FATAL_FAILURE(
        ScalarFieldHeatmapOverlay<SquareFieldTopology> overlay(
            field, kHeatmapLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f)));
}

TEST(ScalarFieldVisualDebugger, HeatmapOverlay_GetLayerName_ReturnsConstructorValue)
{
    auto field = MakeSquareField(3, 3);
    ScalarFieldHeatmapOverlay<SquareFieldTopology> overlay(
        field, kHeatmapLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

    EXPECT_EQ(overlay.GetLayerName(), kHeatmapLayer);
}

TEST(ScalarFieldVisualDebugger, HeatmapOverlay_Draw_AllCellsVisited)
{
    auto field = MakeSquareField(3, 3);   // 9 cells
    ScalarFieldHeatmapOverlay<SquareFieldTopology> overlay(
        field, kHeatmapLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.rectCount, field.GetCellCount());
}

TEST(ScalarFieldVisualDebugger, HeatmapOverlay_Draw_HighValue_UsesHighColour)
{
    auto field = MakeSquareField(1, 1);
    field.WritePoint(CellIndex{0, 0}, 1.0f);
    field.Tick();

    OverlayColourMap colourMap;
    colourMap.lowColour  = Dia::Core::RGBA(0,   0,   255, 200);
    colourMap.highColour = Dia::Core::RGBA(255, 0,   0,   200);
    colourMap.minValue   = 0.0f;
    colourMap.maxValue   = 1.0f;

    ScalarFieldHeatmapOverlay<SquareFieldTopology> overlay(
        field, kHeatmapLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f), colourMap);

    MockDebugDraw mock;
    overlay.Draw(mock);

    ASSERT_EQ(mock.rectCount, 1);
    EXPECT_EQ(mock.lastFillColour.R(), 255u);
    EXPECT_EQ(mock.lastFillColour.G(),   0u);
    EXPECT_EQ(mock.lastFillColour.B(),   0u);
    EXPECT_EQ(mock.lastFillColour.A(), 200u);
}

TEST(ScalarFieldVisualDebugger, HeatmapOverlay_Draw_LowValue_UsesLowColour)
{
    // 1×1 field, no writes → value = 0 → t = 0 → fill == lowColour
    auto field = MakeSquareField(1, 1);

    OverlayColourMap colourMap;
    colourMap.lowColour  = Dia::Core::RGBA(0,   0,   255, 200);
    colourMap.highColour = Dia::Core::RGBA(255, 0,   0,   200);
    colourMap.minValue   = 0.0f;
    colourMap.maxValue   = 1.0f;

    ScalarFieldHeatmapOverlay<SquareFieldTopology> overlay(
        field, kHeatmapLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f), colourMap);

    MockDebugDraw mock;
    overlay.Draw(mock);

    ASSERT_EQ(mock.rectCount, 1);
    EXPECT_EQ(mock.lastFillColour.R(),   0u);
    EXPECT_EQ(mock.lastFillColour.G(),   0u);
    EXPECT_EQ(mock.lastFillColour.B(), 255u);
    EXPECT_EQ(mock.lastFillColour.A(), 200u);
}

TEST(ScalarFieldVisualDebugger, HeatmapOverlay_Draw_MidValue_UsesLerpedColour)
{
    auto field = MakeSquareField(1, 1);
    field.WritePoint(CellIndex{0, 0}, 0.5f);
    field.Tick();

    OverlayColourMap colourMap;
    colourMap.lowColour  = Dia::Core::RGBA(0,   0,   0,   255);
    colourMap.highColour = Dia::Core::RGBA(100, 100, 100, 255);
    colourMap.minValue   = 0.0f;
    colourMap.maxValue   = 1.0f;

    ScalarFieldHeatmapOverlay<SquareFieldTopology> overlay(
        field, kHeatmapLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f), colourMap);

    MockDebugDraw mock;
    overlay.Draw(mock);

    ASSERT_EQ(mock.rectCount, 1);
    // At t=0.5: lerp(0, 100, 0.5) = 50 for each channel. Allow ±2 for integer rounding.
    EXPECT_NEAR(static_cast<int>(mock.lastFillColour.R()), 50, 2);
    EXPECT_NEAR(static_cast<int>(mock.lastFillColour.G()), 50, 2);
    EXPECT_NEAR(static_cast<int>(mock.lastFillColour.B()), 50, 2);
    EXPECT_EQ(mock.lastFillColour.A(), 255u);
}

TEST(ScalarFieldVisualDebugger, HeatmapOverlay_Draw_ClampBelowMin_UsesLowColour)
{
    // colourMap.minValue = 0.5f. Field value = 0.0f → t clamps to 0 → fill == lowColour.
    auto field = MakeSquareField(1, 1);
    // no writes: value stays 0

    OverlayColourMap colourMap;
    colourMap.lowColour  = Dia::Core::RGBA(0,   0,   255, 200);
    colourMap.highColour = Dia::Core::RGBA(255, 0,   0,   200);
    colourMap.minValue   = 0.5f;
    colourMap.maxValue   = 1.0f;

    ScalarFieldHeatmapOverlay<SquareFieldTopology> overlay(
        field, kHeatmapLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f), colourMap);

    MockDebugDraw mock;
    overlay.Draw(mock);

    ASSERT_EQ(mock.rectCount, 1);
    EXPECT_EQ(mock.lastFillColour.R(),   0u);
    EXPECT_EQ(mock.lastFillColour.G(),   0u);
    EXPECT_EQ(mock.lastFillColour.B(), 255u);
    EXPECT_EQ(mock.lastFillColour.A(), 200u);
}

TEST(ScalarFieldVisualDebugger, HeatmapOverlay_Draw_ClampAboveMax_UsesHighColour)
{
    // colourMap.maxValue = 0.5f. Field value = 1.0f → t clamps to 1 → fill == highColour.
    auto field = MakeSquareField(1, 1);
    field.WritePoint(CellIndex{0, 0}, 1.0f);
    field.Tick();

    OverlayColourMap colourMap;
    colourMap.lowColour  = Dia::Core::RGBA(0,   0,   255, 200);
    colourMap.highColour = Dia::Core::RGBA(255, 0,   0,   200);
    colourMap.minValue   = 0.0f;
    colourMap.maxValue   = 0.5f;

    ScalarFieldHeatmapOverlay<SquareFieldTopology> overlay(
        field, kHeatmapLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f), colourMap);

    MockDebugDraw mock;
    overlay.Draw(mock);

    ASSERT_EQ(mock.rectCount, 1);
    EXPECT_EQ(mock.lastFillColour.R(), 255u);
    EXPECT_EQ(mock.lastFillColour.G(),   0u);
    EXPECT_EQ(mock.lastFillColour.B(),   0u);
    EXPECT_EQ(mock.lastFillColour.A(), 200u);
}

TEST(ScalarFieldVisualDebugger, HeatmapOverlay_Draw_WhenDisabled_DrawsNothing)
{
    auto field = MakeSquareField(3, 3);
    ScalarFieldHeatmapOverlay<SquareFieldTopology> overlay(
        field, kHeatmapLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

    overlay.SetEnabled(false);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.rectCount, 0);
}

TEST(ScalarFieldVisualDebugger, HeatmapOverlay_Draw_ReenableAfterDisable_DrawsAgain)
{
    auto field = MakeSquareField(3, 3);
    ScalarFieldHeatmapOverlay<SquareFieldTopology> overlay(
        field, kHeatmapLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

    overlay.SetEnabled(false);

    MockDebugDraw mock;
    overlay.Draw(mock);
    EXPECT_EQ(mock.rectCount, 0);

    overlay.SetEnabled(true);
    overlay.Draw(mock);
    EXPECT_EQ(mock.rectCount, field.GetCellCount());
}

TEST(ScalarFieldVisualDebugger, HeatmapOverlay_Draw_WorldOriginOffset_CellPositionShifted)
{
    // Cell (0,0) rect starts at worldOrigin.
    auto field = MakeSquareField(1, 1);

    ScalarFieldHeatmapOverlay<SquareFieldTopology> overlay(
        field, kHeatmapLayer, 32.0f, Dia::Maths::Vector2D(100.0f, 200.0f));

    MockDebugDraw mock;
    overlay.Draw(mock);

    ASSERT_EQ(mock.rectCount, 1);
    EXPECT_FLOAT_EQ(mock.lastRectMin.x, 100.0f);
    EXPECT_FLOAT_EQ(mock.lastRectMin.y, 200.0f);
}

TEST(ScalarFieldVisualDebugger, HeatmapOverlay_Hex_Draw_AllCellsVisited)
{
    // Radius 2: 3*2*2 + 3*2 + 1 = 12 + 6 + 1 = 19 cells
    auto field = MakeHexField(2);
    ASSERT_EQ(field.GetCellCount(), 19);

    ScalarFieldHeatmapOverlay<HexFieldTopology> overlay(
        field, kHeatmapLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.rectCount, 19);
}

// ===========================================================================
// ScalarFieldVisualDebugger tests — Gradient arrows
// ===========================================================================

TEST(ScalarFieldVisualDebugger, GradientOverlay_Construct_DoesNotCrash)
{
    auto field = MakeSquareField(3, 3);
    EXPECT_NO_FATAL_FAILURE(
        ScalarFieldGradientOverlay<SquareFieldTopology> overlay(
            field, kGradLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f)));
}

TEST(ScalarFieldVisualDebugger, GradientOverlay_GetLayerName_ReturnsConstructorValue)
{
    auto field = MakeSquareField(3, 3);
    ScalarFieldGradientOverlay<SquareFieldTopology> overlay(
        field, kGradLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

    EXPECT_EQ(overlay.GetLayerName(), kGradLayer);
}

TEST(ScalarFieldVisualDebugger, GradientOverlay_Draw_ZeroGradient_SkipsCell)
{
    // Uniform all-zero field — gradient magnitude everywhere is 0.
    auto field = MakeSquareField(3, 3);
    field.Tick();

    ScalarFieldGradientOverlay<SquareFieldTopology> overlay(
        field, kGradLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.rayCount, 0);
    EXPECT_EQ(mock.triangleCount, 0);
}

TEST(ScalarFieldVisualDebugger, GradientOverlay_Draw_NonZeroGradient_DrawsRay)
{
    // Write a peak at center; after Tick() the field is non-uniform so
    // at least one cell will have a non-zero gradient.
    // With diffusionFactor=0, decayRate=0: peak cell stays at 1.0, neighbours at 0.
    // Neighbours of the peak cell see gradient pointing toward the peak.
    auto field = MakeSquareField(5, 5);
    field.WritePoint(CellIndex{2, 2}, 1.0f);
    field.Tick();

    ScalarFieldGradientOverlay<SquareFieldTopology> overlay(
        field, kGradLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_GT(mock.rayCount, 0);
}

TEST(ScalarFieldVisualDebugger, GradientOverlay_Draw_NonZeroGradient_RayAndTriangleCountMatch)
{
    // Each drawn arrow = one ray (shaft) + one triangle (arrowhead).
    auto field = MakeSquareField(5, 5);
    field.WritePoint(CellIndex{2, 2}, 1.0f);
    field.Tick();

    ScalarFieldGradientOverlay<SquareFieldTopology> overlay(
        field, kGradLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_GT(mock.rayCount, 0);
    EXPECT_EQ(mock.rayCount, mock.triangleCount);
}

TEST(ScalarFieldVisualDebugger, GradientOverlay_Draw_WhenDisabled_DrawsNothing)
{
    auto field = MakeSquareField(5, 5);
    field.WritePoint(CellIndex{2, 2}, 1.0f);
    field.Tick();

    ScalarFieldGradientOverlay<SquareFieldTopology> overlay(
        field, kGradLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f));
    overlay.SetEnabled(false);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.rayCount, 0);
    EXPECT_EQ(mock.triangleCount, 0);
}

TEST(ScalarFieldVisualDebugger, GradientOverlay_Draw_ReenableAfterDisable_DrawsAgain)
{
    auto field = MakeSquareField(5, 5);
    field.WritePoint(CellIndex{2, 2}, 1.0f);
    field.Tick();

    ScalarFieldGradientOverlay<SquareFieldTopology> overlay(
        field, kGradLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

    overlay.SetEnabled(false);
    MockDebugDraw mock;
    overlay.Draw(mock);
    EXPECT_EQ(mock.rayCount, 0);

    overlay.SetEnabled(true);
    overlay.Draw(mock);
    EXPECT_GT(mock.rayCount, 0);
}

TEST(ScalarFieldVisualDebugger, GradientOverlay_Draw_BelowMinMagnitude_SkipsCell)
{
    // Very high threshold: no real gradient will exceed 100.
    auto field = MakeSquareField(5, 5);
    field.WritePoint(CellIndex{2, 2}, 1.0f);
    field.Tick();

    // minMagnitude = 100.0f — all gradients are in [0,1] range, well below this.
    ScalarFieldGradientOverlay<SquareFieldTopology> overlay(
        field, kGradLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f),
        /*arrowScale=*/0.35f, /*minMagnitude=*/100.0f);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.rayCount, 0);
}

TEST(ScalarFieldVisualDebugger, GradientOverlay_Draw_AtMinMagnitude_DrawsCell)
{
    // Very low threshold: any non-zero gradient will pass.
    auto field = MakeSquareField(5, 5);
    field.WritePoint(CellIndex{2, 2}, 1.0f);
    field.Tick();

    ScalarFieldGradientOverlay<SquareFieldTopology> overlay(
        field, kGradLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f),
        /*arrowScale=*/0.35f, /*minMagnitude=*/0.001f);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_GT(mock.rayCount, 0);
}

TEST(ScalarFieldVisualDebugger, GradientOverlay_Hex_Draw_NonZeroGradient_DrawsRays)
{
    // Hex radius 2. Write peak at {0,0} (axial centre). After Tick, the 6
    // immediate axial neighbours of {0,0} will have gradient pointing inward.
    auto field = MakeHexField(2);
    field.WritePoint(CellIndex{0, 0}, 1.0f);
    field.Tick();

    ScalarFieldGradientOverlay<HexFieldTopology> overlay(
        field, kGradLayer, 32.0f, Dia::Maths::Vector2D(0.0f, 0.0f));

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_GT(mock.rayCount, 0);
}
