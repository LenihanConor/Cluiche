// Suite: ScalarFieldGradient

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
// Helper: 5x5 8-connected field with no diffusion/decay
// ---------------------------------------------------------------------------

static SquareScalarField MakeGradientTestField()
{
    SquareFieldTopology topo(5, 5, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.0f, 0.0f });
    return SquareScalarField(topo, policy);
}

// ---------------------------------------------------------------------------
// Gradient tests
// ---------------------------------------------------------------------------

TEST(ScalarFieldGradient, GetGradient_AllEqualNeighbours_ReturnsZero)
{
    SquareScalarField field = MakeGradientTestField();
    // Write same value to all cells, tick, then check gradient at center
    for (int y = 0; y < 5; ++y)
    {
        for (int x = 0; x < 5; ++x)
        {
            field.WritePoint(CellIndex{ x, y }, 0.5f);
        }
    }
    field.Tick();

    Vector2D grad = field.GetGradient(CellIndex{ 2, 2 });
    EXPECT_NEAR(grad.X(), 0.0f, 1e-4f);
    EXPECT_NEAR(grad.Y(), 0.0f, 1e-4f);
}

TEST(ScalarFieldGradient, GetGradient_HighValueToRight_PointsRight)
{
    SquareScalarField field = MakeGradientTestField();

    // Create gradient: higher value on the right side of center
    field.WritePoint(CellIndex{ 1, 2 }, 0.1f);
    field.WritePoint(CellIndex{ 2, 2 }, 0.5f);
    field.WritePoint(CellIndex{ 3, 2 }, 0.9f);
    field.WritePoint(CellIndex{ 2, 1 }, 0.5f);
    field.WritePoint(CellIndex{ 2, 3 }, 0.5f);
    field.Tick();

    // Gradient at (2,2) should point toward +x (right)
    Vector2D expectedDir(1.0f, 0.0f);
    Dia::ScalarField::Testing::AssertGradientDirection(field, CellIndex{ 2, 2 }, expectedDir, 45.0f);
}

TEST(ScalarFieldGradient, GetGradient_HighValueAbove_PointsUp)
{
    SquareScalarField field = MakeGradientTestField();

    // Create gradient: higher value above center (larger y = "up" in this grid)
    field.WritePoint(CellIndex{ 2, 1 }, 0.1f);
    field.WritePoint(CellIndex{ 2, 2 }, 0.5f);
    field.WritePoint(CellIndex{ 2, 3 }, 0.9f);
    field.WritePoint(CellIndex{ 1, 2 }, 0.5f);
    field.WritePoint(CellIndex{ 3, 2 }, 0.5f);
    field.Tick();

    // Gradient at (2,2) should point toward +y (up)
    Vector2D expectedDir(0.0f, 1.0f);
    Dia::ScalarField::Testing::AssertGradientDirection(field, CellIndex{ 2, 2 }, expectedDir, 45.0f);
}

TEST(ScalarFieldGradient, GetGradient_BoundaryCell_DoesNotCrash)
{
    SquareScalarField field = MakeGradientTestField();
    field.WritePoint(CellIndex{ 1, 0 }, 0.8f);
    field.Tick();
    EXPECT_NO_FATAL_FAILURE(field.GetGradient(CellIndex{ 0, 0 }));
}
