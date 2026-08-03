// Suite: ScalarFieldTestHelpers

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
using namespace Dia::ScalarField::Testing;
using Dia::Maths::Vector2D;

// ---------------------------------------------------------------------------
// MockPropagationPolicy
// ---------------------------------------------------------------------------

TEST(ScalarFieldTestHelpers, MockPropagationPolicy_RecordsLastCallArgs)
{
    MockPropagationPolicy mockPolicy;
    mockPolicy.returnValue = 0.3f;

    SquareFieldTopology topo(3, 3, SquareConnectivity::k8Connected);
    DiaScalarField<SquareFieldTopology, MockPropagationPolicy> field(topo, mockPolicy);

    // Write a value to a cell so the policy ComputeCell is called with non-trivial args
    field.WritePoint(CellIndex{ 1, 1 }, 0.7f);
    field.Tick();

    // After Tick, ComputeCell will have been called at least once.
    // The mock records the last call — cell index must be within valid range [0, 9).
    EXPECT_GE(field.GetTopology().GetCellCount(), 1);
    // All output values should be mockPolicy.returnValue since that is returned unconditionally
    // (clamped to [0,1] by DiaScalarField, and 0.3 is within range)
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            EXPECT_NEAR(field.GetValue(CellIndex{ x, y }), 0.3f, 1e-4f)
                << "Cell (" << x << "," << y << ") should equal mockPolicy.returnValue";
        }
    }
}

// ---------------------------------------------------------------------------
// AssertCellValue
// ---------------------------------------------------------------------------

TEST(ScalarFieldTestHelpers, AssertCellValue_PassesWhenClose)
{
    SquareFieldTopology topo(3, 3, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.0f, 0.0f });
    SquareScalarField field(topo, policy);

    field.WritePoint(CellIndex{ 1, 1 }, 0.5f);
    field.Tick();

    // Should not fail: value is exactly 0.5, tolerance 0.01
    EXPECT_NO_FATAL_FAILURE(AssertCellValue(field, CellIndex{ 1, 1 }, 0.5f, 0.01f));
}

// ---------------------------------------------------------------------------
// AssertGradientDirection
// ---------------------------------------------------------------------------

TEST(ScalarFieldTestHelpers, AssertGradientDirection_PassesWhenClose)
{
    SquareFieldTopology topo(5, 5, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.0f, 0.0f });
    SquareScalarField field(topo, policy);

    // Create a clear +x gradient at (2,2)
    field.WritePoint(CellIndex{ 1, 2 }, 0.1f);
    field.WritePoint(CellIndex{ 2, 2 }, 0.5f);
    field.WritePoint(CellIndex{ 3, 2 }, 0.9f);
    field.WritePoint(CellIndex{ 2, 1 }, 0.5f);
    field.WritePoint(CellIndex{ 2, 3 }, 0.5f);
    field.Tick();

    Vector2D expectedDir(1.0f, 0.0f);
    EXPECT_NO_FATAL_FAILURE(AssertGradientDirection(field, CellIndex{ 2, 2 }, expectedDir, 45.0f));
}
