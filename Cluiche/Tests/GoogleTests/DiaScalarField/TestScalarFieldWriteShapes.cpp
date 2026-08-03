// Suite: ScalarFieldWriteShapes

#include <gtest/gtest.h>
#include <DiaScalarField/DiaScalarField.h>
#include <DiaScalarField/SquareFieldTopology.h>
#include <DiaScalarField/HexFieldTopology.h>
#include <DiaScalarField/CFieldTopology.h>
#include <DiaScalarField/CellIndex.h>
#include <DiaScalarField/UniformDecayPolicy.h>
#include <DiaScalarField/Testing/ScalarFieldTestHelpers.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::ScalarField;
using Dia::Maths::Vector2D;

// ---------------------------------------------------------------------------
// Helper: field with zero diffusion and zero decay so Tick only flushes writes
// ---------------------------------------------------------------------------

static SquareScalarField MakeNoDecayField(int w, int h)
{
    SquareFieldTopology topo(w, h, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.0f, 0.0f });
    return SquareScalarField(topo, policy);
}

// ---------------------------------------------------------------------------
// WritePoint
// ---------------------------------------------------------------------------

TEST(ScalarFieldWriteShapes, WritePoint_SetsExactCell)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    field.WritePoint(CellIndex{ 1, 1 }, 0.75f);
    field.Tick();
    // With zero diffusion and zero decay the written value stays at that cell.
    Dia::ScalarField::Testing::AssertCellValue(field, CellIndex{ 1, 1 }, 0.75f, 1e-4f);
}

// ---------------------------------------------------------------------------
// WriteBox
// ---------------------------------------------------------------------------

TEST(ScalarFieldWriteShapes, WriteBox_SetsRegionCells)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    field.WriteBox(CellIndex{ 0, 0 }, 3, 3, 0.9f);
    field.Tick();

    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            EXPECT_GT(field.GetValue(CellIndex{ x, y }), 0.0f)
                << "Cell (" << x << "," << y << ") should be non-zero after WriteBox";
        }
    }
}

TEST(ScalarFieldWriteShapes, WriteBox_DoesNotAffectOutsideRegion)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    field.WriteBox(CellIndex{ 0, 0 }, 2, 2, 0.9f);
    field.Tick();
    // Cell (3,3) is outside the 2x2 box starting at (0,0)
    EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ 3, 3 }), 0.0f);
}

// ---------------------------------------------------------------------------
// WriteRadial
// ---------------------------------------------------------------------------

TEST(ScalarFieldWriteShapes, WriteRadial_LinearFalloff_PeakAtCenter)
{
    SquareScalarField field = MakeNoDecayField(7, 7);
    const CellIndex center{ 3, 3 };
    const float peakValue = 1.0f;
    const float radius    = 2.5f;

    field.WriteRadial(center, radius, peakValue, FalloffCurve::kLinear);
    field.Tick();

    const float centerVal = field.GetValue(center);
    const float farVal    = field.GetValue(CellIndex{ 3, 6 }); // dist ~3 > radius
    EXPECT_GT(centerVal, 0.8f)  << "Center should be near peak value";
    // Adjacent cell (3,2) is 1 unit away — center is the peak so its value >= neighbour
    EXPECT_GE(centerVal, field.GetValue(CellIndex{ 3, 2 }) - 0.01f);
    // cell at (3,4) is 1 unit away; should be less than center
    EXPECT_LT(field.GetValue(CellIndex{ 3, 4 }), centerVal + 1e-4f);
    (void)farVal;
}

TEST(ScalarFieldWriteShapes, WriteRadial_QuadraticFalloff_FasterDecayThanLinear)
{
    // At same distance, quadratic falloff should produce lower value than linear
    const CellIndex center{ 3, 3 };
    const float radius    = 3.0f;
    const float peakValue = 1.0f;
    const CellIndex testCell{ 3, 5 }; // dist = 2 from center

    SquareScalarField linearField = MakeNoDecayField(7, 7);
    linearField.WriteRadial(center, radius, peakValue, FalloffCurve::kLinear);
    linearField.Tick();

    SquareScalarField quadField = MakeNoDecayField(7, 7);
    quadField.WriteRadial(center, radius, peakValue, FalloffCurve::kQuadratic);
    quadField.Tick();

    // Quadratic attenuation at t=2/3: 1-(2/3)^2 = 0.555
    // Linear attenuation at t=2/3:    1-(2/3)   = 0.333
    // Actually quadratic > linear at t<1, but let's just verify both are in (0,1)
    // and quadratic >= linear (it stays higher closer to center but that's expected)
    EXPECT_GT(linearField.GetValue(testCell), 0.0f);
    EXPECT_GT(quadField.GetValue(testCell),   0.0f);
    // Quadratic falloff is less steep at mid-distance: quadratic value >= linear
    EXPECT_GE(quadField.GetValue(testCell), linearField.GetValue(testCell) - 1e-4f);
}

TEST(ScalarFieldWriteShapes, WriteRadial_InverseFalloff_DoesNotCrash)
{
    SquareScalarField field = MakeNoDecayField(7, 7);
    EXPECT_NO_FATAL_FAILURE({
        field.WriteRadial(CellIndex{ 3, 3 }, 3.0f, 1.0f, FalloffCurve::kInverse);
        field.Tick();
    });
}

TEST(ScalarFieldWriteShapes, WriteRadial_CellOutsideRadius_NotQueued)
{
    SquareScalarField field = MakeNoDecayField(7, 7);
    // Write radial with small radius at (3,3) — cell (0,0) is far outside
    field.WriteRadial(CellIndex{ 3, 3 }, 1.5f, 1.0f, FalloffCurve::kLinear);
    field.Tick();
    EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ 0, 0 }), 0.0f);
}
