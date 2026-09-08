// Suite: ScalarFieldEdgeCases

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
using ResultArray = Dia::Core::Containers::DynamicArrayC<CellIndex, 1024>;

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
// WriteRadial edge cases
// ---------------------------------------------------------------------------

// radius == 0: only the centre cell should receive the peak value.
TEST(ScalarFieldEdgeCases, WriteRadial_ZeroRadius_OnlyWritesCenter)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    const CellIndex center{ 2, 2 };
    field.WriteRadial(center, 0.0f, 1.0f, FalloffCurve::kLinear);
    field.Tick();

    EXPECT_GT(field.GetValue(center), 0.0f) << "Centre cell must receive peak value";

    // Orthogonal neighbours must stay at zero.
    EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ 1, 2 }), 0.0f);
    EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ 3, 2 }), 0.0f);
    EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ 2, 1 }), 0.0f);
    EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ 2, 3 }), 0.0f);
}

// ---------------------------------------------------------------------------
// WriteBox edge cases
// ---------------------------------------------------------------------------

// width == 0: no cells match the box check, so nothing is written.
TEST(ScalarFieldEdgeCases, WriteBox_ZeroWidth_WritesNothing)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    field.WriteBox(CellIndex{ 1, 1 }, 0, 3, 0.9f);
    field.Tick();

    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ x, y }), 0.0f)
                << "Cell (" << x << "," << y << ") should stay 0 with zero-width box";
}

// height == 0: no cells match the box check, so nothing is written.
TEST(ScalarFieldEdgeCases, WriteBox_ZeroHeight_WritesNothing)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    field.WriteBox(CellIndex{ 1, 1 }, 3, 0, 0.9f);
    field.Tick();

    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ x, y }), 0.0f)
                << "Cell (" << x << "," << y << ") should stay 0 with zero-height box";
}

// ---------------------------------------------------------------------------
// Combine edge cases
// ---------------------------------------------------------------------------

// Combine with empty inputs writes 0 to all cells (clamped to minClamp=0).
// Any pre-existing values in the result field are overwritten with the
// weighted sum of zero inputs.
TEST(ScalarFieldEdgeCases, Combine_EmptyInputs_WritesZero)
{
    // Set up result field with some nonzero values.
    SquareScalarField result = MakeNoDecayField(5, 5);
    result.WritePoint(CellIndex{ 2, 2 }, 0.8f);
    result.Tick();
    ASSERT_GT(result.GetValue(CellIndex{ 2, 2 }), 0.0f);

    // Combine with empty input list.
    Dia::Core::Containers::DynamicArrayC<SquareScalarField::WeightedField, 32> inputs;
    SquareScalarField::Combine(result, inputs);

    // All cells should now be 0 (combined sum = 0, clamped to [0,1]).
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            EXPECT_FLOAT_EQ(result.GetValue(CellIndex{ x, y }), 0.0f)
                << "Cell (" << x << "," << y << ") should be 0 after Combine with empty inputs";
}

// ---------------------------------------------------------------------------
// FindLocalMaxima edge cases
// ---------------------------------------------------------------------------

// Uniform field: no cell is strictly greater than all its neighbours.
TEST(ScalarFieldEdgeCases, FindLocalMaxima_TiedValues_NotReturned)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            field.WritePoint(CellIndex{ x, y }, 0.5f);
    field.Tick();

    ResultArray results;
    field.FindLocalMaxima(CellIndex{ 0, 0 }, 5, 5, results);

    EXPECT_EQ(results.Size(), 0)
        << "Uniform field should yield no local maxima";
}

// A cell and all its neighbours share the same value — cell is NOT a strict max.
TEST(ScalarFieldEdgeCases, FindLocalMaxima_IsolatedPeak_AdjacentEqual_NotReturned)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    // Write 0.8 to (2,2) and all 8 of its neighbours.
    field.WritePoint(CellIndex{ 2, 2 }, 0.8f);
    field.WritePoint(CellIndex{ 1, 1 }, 0.8f);
    field.WritePoint(CellIndex{ 2, 1 }, 0.8f);
    field.WritePoint(CellIndex{ 3, 1 }, 0.8f);
    field.WritePoint(CellIndex{ 1, 2 }, 0.8f);
    field.WritePoint(CellIndex{ 3, 2 }, 0.8f);
    field.WritePoint(CellIndex{ 1, 3 }, 0.8f);
    field.WritePoint(CellIndex{ 2, 3 }, 0.8f);
    field.WritePoint(CellIndex{ 3, 3 }, 0.8f);
    field.Tick();

    ResultArray results;
    field.FindLocalMaxima(CellIndex{ 0, 0 }, 5, 5, results);

    bool found = false;
    for (int i = 0; i < results.Size(); ++i)
        if (results[i] == (CellIndex{ 2, 2 }))
            found = true;

    EXPECT_FALSE(found)
        << "(2,2) should not be a local max when all neighbours share its value";
}

// ---------------------------------------------------------------------------
// Gradient edge cases
// ---------------------------------------------------------------------------

// GetGradient on a HexScalarField at origin cell must not crash.
TEST(ScalarFieldEdgeCases, GetGradient_HexTopology_OriginCell_DoesNotCrash)
{
    HexScalarField field(HexFieldTopology(2));
    EXPECT_NO_FATAL_FAILURE(field.GetGradient(CellIndex{ 0, 0 }));
}

// GetGradient on an all-zero field should return (0, 0).
TEST(ScalarFieldEdgeCases, GetGradient_AllZero_ReturnsZero)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    // No writes — all cells remain 0.
    Vector2D grad = field.GetGradient(CellIndex{ 2, 2 });
    EXPECT_NEAR(grad.X(), 0.0f, 1e-6f);
    EXPECT_NEAR(grad.Y(), 0.0f, 1e-6f);
}

// ---------------------------------------------------------------------------
// Topology / cell count edge cases
// ---------------------------------------------------------------------------

// Cell counts must match topology specifications.
TEST(ScalarFieldEdgeCases, BFS_CellCount_MatchesTopology)
{
    SquareScalarField sqField(SquareFieldTopology(5, 4));
    EXPECT_EQ(sqField.GetCellCount(), 20)
        << "5x4 square field should have 20 cells";

    HexScalarField hexField(HexFieldTopology(2));
    // radius=2: 3*4 + 3*2 + 1 = 12 + 6 + 1 = 19
    EXPECT_EQ(hexField.GetCellCount(), 19)
        << "HexFieldTopology(radius=2) should have 19 cells";
}

// ---------------------------------------------------------------------------
// Stress / invariant
// ---------------------------------------------------------------------------

// Large field, many ticks: all values must stay within [0, 1] clamp range.
TEST(ScalarFieldEdgeCases, Tick_LargeField_ManyTicks_StaysWithinClampRange)
{
    SquareFieldTopology topo(20, 20, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.5f, 0.01f });
    SquareScalarField field(topo, policy);
    field.SetClampRange(0.0f, 1.0f);
    field.WritePoint(CellIndex{ 10, 10 }, 1.0f);

    for (int t = 0; t < 100; ++t)
        field.Tick();

    for (int y = 0; y < 20; ++y)
    {
        for (int x = 0; x < 20; ++x)
        {
            const float v = field.GetValue(CellIndex{ x, y });
            EXPECT_GE(v, 0.0f) << "Cell (" << x << "," << y << ") below min clamp";
            EXPECT_LE(v, 1.0f) << "Cell (" << x << "," << y << ") above max clamp";
            EXPECT_FALSE(std::isnan(v)) << "Cell (" << x << "," << y << ") is NaN";
        }
    }
}

// ---------------------------------------------------------------------------
// Write ordering
// ---------------------------------------------------------------------------

// When the same cell is written twice before Tick, the last write wins.
TEST(ScalarFieldEdgeCases, WritePoint_SameCell_TwiceSameTickOverwrites)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    field.WritePoint(CellIndex{ 2, 2 }, 0.3f);
    field.WritePoint(CellIndex{ 2, 2 }, 0.9f);
    field.Tick();

    // The last write (0.9) should be what lands in the buffer.
    EXPECT_NEAR(field.GetValue(CellIndex{ 2, 2 }), 0.9f, 1e-4f);
}

// ---------------------------------------------------------------------------
// StaticModifier
// ---------------------------------------------------------------------------

// StaticModifier=0 suppresses diffusion TO that cell from neighbours but
// a direct WritePoint to that cell still survives through Tick.
TEST(ScalarFieldEdgeCases, SetStaticModifier_Zero_BlocksPropagationButNotWrite)
{
    // Field with diffusion so neighbours can contribute.
    SquareFieldTopology topo(5, 5, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.5f, 0.0f }); // diffuse, no decay
    SquareScalarField field(topo, policy);

    // Write 1.0 into several cells around (3,3) so they could diffuse to (3,3).
    field.WritePoint(CellIndex{ 2, 3 }, 1.0f);
    field.WritePoint(CellIndex{ 4, 3 }, 1.0f);
    field.WritePoint(CellIndex{ 3, 2 }, 1.0f);
    field.WritePoint(CellIndex{ 3, 4 }, 1.0f);
    field.Tick(); // Establish neighbour values

    // Set modifier=0 on (3,3) — kills diffusion contribution at (3,3).
    field.SetStaticModifier(CellIndex{ 3, 3 }, 0.0f);

    // Build a comparison: same setup without modifier=0.
    SquareScalarField fieldControl(topo, policy);
    fieldControl.WritePoint(CellIndex{ 2, 3 }, 1.0f);
    fieldControl.WritePoint(CellIndex{ 4, 3 }, 1.0f);
    fieldControl.WritePoint(CellIndex{ 3, 2 }, 1.0f);
    fieldControl.WritePoint(CellIndex{ 3, 4 }, 1.0f);
    fieldControl.Tick();

    // Tick both one more time without new writes — tests propagation pass.
    field.Tick();
    fieldControl.Tick();

    // The field with modifier=0 should have LESS value at (3,3) than the control.
    EXPECT_LT(field.GetValue(CellIndex{ 3, 3 }),
              fieldControl.GetValue(CellIndex{ 3, 3 }) + 1e-4f)
        << "Modifier=0 should suppress diffusion contribution at (3,3)";

    // Now confirm a direct WritePoint to (3,3) still survives in the modifier=0 field.
    SquareScalarField fieldDirect(topo, policy);
    fieldDirect.SetStaticModifier(CellIndex{ 3, 3 }, 0.0f);
    fieldDirect.WritePoint(CellIndex{ 3, 3 }, 1.0f);
    fieldDirect.Tick();

    EXPECT_GT(fieldDirect.GetValue(CellIndex{ 3, 3 }), 0.0f)
        << "A direct WritePoint to a zero-modifier cell must still be visible after Tick";
}
