// Suite: ScalarFieldCore

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
// Helper: 5x5 no-decay field
// ---------------------------------------------------------------------------

static SquareScalarField MakeNoDecayField(int w, int h)
{
    SquareFieldTopology topo(w, h, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.0f, 0.0f });
    return SquareScalarField(topo, policy);
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(ScalarFieldCore, Construct_GetCellCount_MatchesTopology)
{
    SquareScalarField field(SquareFieldTopology(5, 5));
    EXPECT_EQ(field.GetCellCount(), 25);
}

TEST(ScalarFieldCore, Construct_GetValue_AllZero)
{
    SquareScalarField field(SquareFieldTopology(3, 3));
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ x, y }), 0.0f)
                << "Cell (" << x << "," << y << ") should start at zero";
        }
    }
}

// ---------------------------------------------------------------------------
// SetBlocked / IsBlocked
// ---------------------------------------------------------------------------

TEST(ScalarFieldCore, SetBlocked_IsBlocked_RoundTrips)
{
    SquareScalarField field(SquareFieldTopology(5, 5));
    CellIndex cell{ 2, 2 };
    EXPECT_FALSE(field.IsBlocked(cell));
    field.SetBlocked(cell, true);
    EXPECT_TRUE(field.IsBlocked(cell));
}

TEST(ScalarFieldCore, SetBlocked_False_UnblocksCell)
{
    SquareScalarField field(SquareFieldTopology(5, 5));
    CellIndex cell{ 1, 3 };
    field.SetBlocked(cell, true);
    ASSERT_TRUE(field.IsBlocked(cell));
    field.SetBlocked(cell, false);
    EXPECT_FALSE(field.IsBlocked(cell));
}

// ---------------------------------------------------------------------------
// SetStaticModifier
// ---------------------------------------------------------------------------

TEST(ScalarFieldCore, SetStaticModifier_DoesNotCrash)
{
    SquareScalarField field(SquareFieldTopology(5, 5));
    EXPECT_NO_FATAL_FAILURE(field.SetStaticModifier(CellIndex{ 1, 1 }, 0.5f));
}

// ---------------------------------------------------------------------------
// SetClampRange
// ---------------------------------------------------------------------------

TEST(ScalarFieldCore, SetClampRange_LimitsValues)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    field.SetClampRange(0.0f, 0.5f);
    field.WritePoint(CellIndex{ 2, 2 }, 1.0f);
    field.Tick();

    for (int y = 0; y < 5; ++y)
    {
        for (int x = 0; x < 5; ++x)
        {
            EXPECT_LE(field.GetValue(CellIndex{ x, y }), 0.5f)
                << "Cell (" << x << "," << y << ") exceeds max clamp";
        }
    }
}

// ---------------------------------------------------------------------------
// WritePoint + Tick
// ---------------------------------------------------------------------------

TEST(ScalarFieldCore, WritePoint_ThenGetValue_BeforeTick_NotYetApplied)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    field.WritePoint(CellIndex{ 2, 2 }, 1.0f);
    // Pending write is not yet applied — read buffer still zero
    EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ 2, 2 }), 0.0f);
}

TEST(ScalarFieldCore, WritePoint_ThenTick_ValueAppearsAtCell)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    field.WritePoint(CellIndex{ 2, 2 }, 1.0f);
    field.Tick();
    EXPECT_GT(field.GetValue(CellIndex{ 2, 2 }), 0.0f);
}

// ---------------------------------------------------------------------------
// Blocked cell stays zero even when written
// ---------------------------------------------------------------------------

TEST(ScalarFieldCore, Tick_BlockedCell_StaysZero)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    field.SetBlocked(CellIndex{ 2, 2 }, true);
    field.WritePoint(CellIndex{ 2, 2 }, 1.0f);
    field.Tick();
    EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ 2, 2 }), 0.0f);
}

// ---------------------------------------------------------------------------
// Propagation
// ---------------------------------------------------------------------------

TEST(ScalarFieldCore, Tick_Propagates_NeighboursGainValue)
{
    // Use a field with diffusion so values spread from (2,2) to adjacent cells.
    SquareFieldTopology topo(5, 5, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.5f, 0.0f }); // diffuse, no decay
    SquareScalarField field(topo, policy);

    field.WritePoint(CellIndex{ 2, 2 }, 1.0f);
    field.Tick();
    // After first tick, (2,2) already written; tick again so neighbours see it
    field.Tick();

    EXPECT_GT(field.GetValue(CellIndex{ 2, 3 }), 0.0f);
    EXPECT_GT(field.GetValue(CellIndex{ 3, 2 }), 0.0f);
}

// ---------------------------------------------------------------------------
// GetCellCount / GetTopology
// ---------------------------------------------------------------------------

TEST(ScalarFieldCore, GetCellCount_MatchesTopology)
{
    SquareScalarField field(SquareFieldTopology(4, 6));
    EXPECT_EQ(field.GetCellCount(), 24);
}

TEST(ScalarFieldCore, GetTopology_ReturnsSameTopology)
{
    SquareFieldTopology topo(7, 3);
    SquareScalarField field(topo);
    EXPECT_EQ(field.GetTopology().GetWidth(),  7);
    EXPECT_EQ(field.GetTopology().GetHeight(), 3);
}
