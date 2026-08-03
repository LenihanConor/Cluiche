// Suite: ScalarFieldInvariant

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
// Helper
// ---------------------------------------------------------------------------

static SquareScalarField MakeNoDecayField(int w, int h)
{
    SquareFieldTopology topo(w, h, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.0f, 0.0f });
    return SquareScalarField(topo, policy);
}

// ---------------------------------------------------------------------------
// Invariants
// ---------------------------------------------------------------------------

TEST(ScalarFieldInvariant, Tick_ConservesClampBounds)
{
    SquareFieldTopology topo(5, 5, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.5f, 0.02f });
    SquareScalarField field(topo, policy);
    field.SetClampRange(0.0f, 0.8f);

    field.WritePoint(CellIndex{ 2, 2 }, 1.0f);

    for (int tick = 0; tick < 20; ++tick)
    {
        field.WritePoint(CellIndex{ 2, 2 }, 1.0f);
        field.Tick();
        for (int y = 0; y < 5; ++y)
        {
            for (int x = 0; x < 5; ++x)
            {
                const float v = field.GetValue(CellIndex{ x, y });
                EXPECT_GE(v, 0.0f) << "Tick " << tick << " cell (" << x << "," << y << ") below min";
                EXPECT_LE(v, 0.8f) << "Tick " << tick << " cell (" << x << "," << y << ") above max";
            }
        }
    }
}

TEST(ScalarFieldInvariant, Tick_BlockedCellsNeverGainValue)
{
    SquareFieldTopology topo(5, 5, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.5f, 0.0f });
    SquareScalarField field(topo, policy);

    // Block several cells
    field.SetBlocked(CellIndex{ 1, 1 }, true);
    field.SetBlocked(CellIndex{ 2, 3 }, true);
    field.SetBlocked(CellIndex{ 4, 0 }, true);

    for (int tick = 0; tick < 20; ++tick)
    {
        field.WritePoint(CellIndex{ 2, 2 }, 1.0f);
        field.Tick();

        EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ 1, 1 }), 0.0f)
            << "Blocked cell (1,1) gained value on tick " << tick;
        EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ 2, 3 }), 0.0f)
            << "Blocked cell (2,3) gained value on tick " << tick;
        EXPECT_FLOAT_EQ(field.GetValue(CellIndex{ 4, 0 }), 0.0f)
            << "Blocked cell (4,0) gained value on tick " << tick;
    }
}

TEST(ScalarFieldInvariant, Tick_WriteQueueClearedAfterTick)
{
    SquareScalarField field = MakeNoDecayField(5, 5);

    // First write + tick
    field.WritePoint(CellIndex{ 2, 2 }, 0.9f);
    field.Tick();
    const float afterFirstTick = field.GetValue(CellIndex{ 2, 2 });
    EXPECT_NEAR(afterFirstTick, 0.9f, 1e-4f);

    // Second tick with no new write — pending queue was cleared, so value should
    // not be re-applied.  With zero decay the value stays via current*1.0 + 0 = 0.9,
    // but it was already in the read buffer; so second tick produces the same value.
    // The important thing is the write is not re-queued (no double-application).
    field.Tick();
    const float afterSecondTick = field.GetValue(CellIndex{ 2, 2 });
    // Value should equal first tick result (decay=0 so no change), not be doubled.
    EXPECT_NEAR(afterSecondTick, afterFirstTick, 1e-4f);
}

// ---------------------------------------------------------------------------
// Type alias construction
// ---------------------------------------------------------------------------

TEST(ScalarFieldInvariant, SquareScalarField_TypeAlias_Constructs)
{
    SquareScalarField f(SquareFieldTopology(4, 4));
    EXPECT_EQ(f.GetCellCount(), 16);
}

TEST(ScalarFieldInvariant, HexScalarField_TypeAlias_Constructs)
{
    HexScalarField f(HexFieldTopology(2));
    EXPECT_EQ(f.GetCellCount(), 19);
}
