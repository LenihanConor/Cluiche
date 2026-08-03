// Suite: ScalarFieldSpatialQueries

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
// Helper
// ---------------------------------------------------------------------------

static SquareScalarField MakeNoDecayField(int w, int h)
{
    SquareFieldTopology topo(w, h, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.0f, 0.0f });
    return SquareScalarField(topo, policy);
}

static bool ContainsCell(const ResultArray& arr, CellIndex target)
{
    for (int i = 0; i < arr.Size(); ++i)
    {
        if (arr[i] == target)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// FindLocalMaxima
// ---------------------------------------------------------------------------

TEST(ScalarFieldSpatialQueries, FindLocalMaxima_SinglePeak_FindsIt)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    // Write a clear peak at (2,2) and lower values around it
    field.WritePoint(CellIndex{ 2, 2 }, 1.0f);
    field.WritePoint(CellIndex{ 1, 2 }, 0.3f);
    field.WritePoint(CellIndex{ 3, 2 }, 0.3f);
    field.WritePoint(CellIndex{ 2, 1 }, 0.3f);
    field.WritePoint(CellIndex{ 2, 3 }, 0.3f);
    field.Tick();

    ResultArray results;
    field.FindLocalMaxima(CellIndex{ 0, 0 }, 5, 5, results);

    EXPECT_TRUE(ContainsCell(results, CellIndex{ 2, 2 }))
        << "Peak at (2,2) should be found as local maximum";
}

TEST(ScalarFieldSpatialQueries, FindLocalMaxima_NoPeak_ReturnsEmpty)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    // Uniform field — no local maxima
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            field.WritePoint(CellIndex{ x, y }, 0.5f);
    field.Tick();

    ResultArray results;
    field.FindLocalMaxima(CellIndex{ 0, 0 }, 5, 5, results);

    EXPECT_EQ(results.Size(), 0);
}

TEST(ScalarFieldSpatialQueries, FindLocalMaxima_RegionFilter_ExcludesOutsideRegion)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    // Peak at (0,0) — outside query region
    field.WritePoint(CellIndex{ 0, 0 }, 1.0f);
    field.Tick();

    ResultArray results;
    // Query region starts at (2,2), size 3x3 — does not include (0,0)
    field.FindLocalMaxima(CellIndex{ 2, 2 }, 3, 3, results);

    EXPECT_FALSE(ContainsCell(results, CellIndex{ 0, 0 }));
}

// ---------------------------------------------------------------------------
// FindCellsAboveThreshold
// ---------------------------------------------------------------------------

TEST(ScalarFieldSpatialQueries, FindCellsAboveThreshold_FindsAll)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    field.WritePoint(CellIndex{ 0, 0 }, 0.8f);
    field.WritePoint(CellIndex{ 1, 1 }, 0.8f);
    field.WritePoint(CellIndex{ 4, 4 }, 0.8f);
    field.Tick();

    ResultArray results;
    field.FindCellsAboveThreshold(0.5f, CellIndex{ 0, 0 }, 5, 5, results);

    EXPECT_TRUE(ContainsCell(results, CellIndex{ 0, 0 }));
    EXPECT_TRUE(ContainsCell(results, CellIndex{ 1, 1 }));
    EXPECT_TRUE(ContainsCell(results, CellIndex{ 4, 4 }));
}

TEST(ScalarFieldSpatialQueries, FindCellsAboveThreshold_NoneAbove_ReturnsEmpty)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    // All values are 0 — nothing above threshold 0.5
    field.Tick();

    ResultArray results;
    field.FindCellsAboveThreshold(0.5f, CellIndex{ 0, 0 }, 5, 5, results);

    EXPECT_EQ(results.Size(), 0);
}

TEST(ScalarFieldSpatialQueries, FindCellsAboveThreshold_RegionFilter_WorksCorrectly)
{
    SquareScalarField field = MakeNoDecayField(5, 5);
    // High value at (0,0) but query region is (2,2) to (4,4)
    field.WritePoint(CellIndex{ 0, 0 }, 0.9f);
    field.WritePoint(CellIndex{ 3, 3 }, 0.9f);
    field.Tick();

    ResultArray results;
    field.FindCellsAboveThreshold(0.5f, CellIndex{ 2, 2 }, 3, 3, results);

    EXPECT_FALSE(ContainsCell(results, CellIndex{ 0, 0 }));
    EXPECT_TRUE(ContainsCell(results, CellIndex{ 3, 3 }));
}
