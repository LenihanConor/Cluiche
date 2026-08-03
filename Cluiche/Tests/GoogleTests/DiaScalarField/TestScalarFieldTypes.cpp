// Suite: ScalarFieldTypes

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
// CellIndex
// ---------------------------------------------------------------------------

TEST(ScalarFieldTypes, CellIndex_DefaultConstruct_IsZeroZero)
{
    CellIndex c{};
    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, 0);
}

TEST(ScalarFieldTypes, CellIndex_Equality_SameCoordsAreEqual)
{
    CellIndex a{ 3, 5 };
    CellIndex b{ 3, 5 };
    EXPECT_EQ(a, b);
}

TEST(ScalarFieldTypes, CellIndex_Equality_DiffCoordsNotEqual)
{
    CellIndex a{ 1, 2 };
    CellIndex b{ 2, 1 };
    EXPECT_NE(a, b);
}

// ---------------------------------------------------------------------------
// SquareFieldTopology
// ---------------------------------------------------------------------------

TEST(ScalarFieldTypes, SquareFieldTopology_GetCellCount_MatchesWidthTimesHeight)
{
    SquareFieldTopology topo(4, 3);
    EXPECT_EQ(topo.GetCellCount(), 12);
}

TEST(ScalarFieldTypes, SquareFieldTopology_ForEachNeighbour_CentreCell_8Connected_Returns8)
{
    SquareFieldTopology topo(3, 3, SquareConnectivity::k8Connected);
    int count = 0;
    topo.ForEachNeighbour(CellIndex{ 1, 1 }, [&](CellIndex) { ++count; });
    EXPECT_EQ(count, 8);
}

TEST(ScalarFieldTypes, SquareFieldTopology_ForEachNeighbour_4Connected_Returns4)
{
    SquareFieldTopology topo(3, 3, SquareConnectivity::k4Connected);
    int count = 0;
    topo.ForEachNeighbour(CellIndex{ 1, 1 }, [&](CellIndex) { ++count; });
    EXPECT_EQ(count, 4);
}

TEST(ScalarFieldTypes, SquareFieldTopology_ForEachNeighbour_CornerCell_LessNeighbours)
{
    SquareFieldTopology topo(3, 3, SquareConnectivity::k8Connected);
    int count = 0;
    topo.ForEachNeighbour(CellIndex{ 0, 0 }, [&](CellIndex) { ++count; });
    EXPECT_EQ(count, 3);
}

// ---------------------------------------------------------------------------
// HexFieldTopology
// ---------------------------------------------------------------------------

TEST(ScalarFieldTypes, HexFieldTopology_GetCellCount_Radius1_Returns7)
{
    HexFieldTopology topo(1);
    EXPECT_EQ(topo.GetCellCount(), 7);
}

TEST(ScalarFieldTypes, HexFieldTopology_GetCellCount_Radius2_Returns19)
{
    HexFieldTopology topo(2);
    EXPECT_EQ(topo.GetCellCount(), 19);
}

TEST(ScalarFieldTypes, HexFieldTopology_ForEachNeighbour_Origin_Returns6)
{
    HexFieldTopology topo(2);
    int count = 0;
    topo.ForEachNeighbour(CellIndex{ 0, 0 }, [&](CellIndex) { ++count; });
    EXPECT_EQ(count, 6);
}

TEST(ScalarFieldTypes, HexFieldTopology_ForEachNeighbour_BoundaryCell_LessNeighbours)
{
    // Radius=2: cell at (2,0) is on the boundary (|q|=2=R)
    HexFieldTopology topo(2);
    int count = 0;
    topo.ForEachNeighbour(CellIndex{ 2, 0 }, [&](CellIndex) { ++count; });
    EXPECT_LT(count, 6);
}

// ---------------------------------------------------------------------------
// CFieldTopology concept
// ---------------------------------------------------------------------------

TEST(ScalarFieldTypes, CFieldTopologyConcept_Square_Satisfied)
{
    static_assert(CFieldTopology<SquareFieldTopology>,
                  "SquareFieldTopology must satisfy CFieldTopology");
    SUCCEED();
}

TEST(ScalarFieldTypes, CFieldTopologyConcept_Hex_Satisfied)
{
    static_assert(CFieldTopology<HexFieldTopology>,
                  "HexFieldTopology must satisfy CFieldTopology");
    SUCCEED();
}
