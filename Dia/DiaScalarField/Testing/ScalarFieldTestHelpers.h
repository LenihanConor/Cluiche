#pragma once

#include <gtest/gtest.h>
#include <DiaScalarField/DiaScalarField.h>
#include <DiaScalarField/SquareFieldTopology.h>
#include <DiaScalarField/HexFieldTopology.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <cmath>

namespace Dia { namespace ScalarField { namespace Testing {

    // A minimal propagation policy for tests.
    // Stores the arguments from the last ComputeCell call for inspection
    // and returns a configurable constant value.
    struct MockPropagationPolicy
    {
        float returnValue = 0.5f;

        mutable int   lastCellIndex      = -1;
        mutable float lastCurrent        = 0.0f;
        mutable float lastStaticModifier = 0.0f;
        mutable float lastNeighbourSum   = 0.0f;

        float ComputeCell(int cell, float current, float staticModifier, float neighbourSum) const
        {
            lastCellIndex      = cell;
            lastCurrent        = current;
            lastStaticModifier = staticModifier;
            lastNeighbourSum   = neighbourSum;
            return returnValue;
        }
    };

    // Assert that the value at `cell` is within `tolerance` of `expectedValue`.
    // Call from within a TEST() body — uses EXPECT_NEAR.
    template<typename T, typename P>
    inline void AssertCellValue(const Dia::ScalarField::DiaScalarField<T, P>& field,
                                 Dia::ScalarField::CellIndex cell,
                                 float expectedValue,
                                 float tolerance = 1e-4f)
    {
        const float actual = field.GetValue(cell);
        EXPECT_NEAR(actual, expectedValue, tolerance)
            << "Cell (" << cell.x << "," << cell.y << ") value mismatch";
    }

    // Assert that the gradient at `cell` points within `toleranceDeg` degrees
    // of `expectedDir`.  Call from within a TEST() body — uses EXPECT_GE.
    template<typename T, typename P>
    inline void AssertGradientDirection(const Dia::ScalarField::DiaScalarField<T, P>& field,
                                         Dia::ScalarField::CellIndex cell,
                                         Dia::Maths::Vector2D expectedDir,
                                         float toleranceDeg = 15.0f)
    {
        Dia::Maths::Vector2D grad = field.GetGradient(cell);
        float cosThresh = std::cosf(toleranceDeg * 3.14159265f / 180.0f);
        float dot = grad.Dot(expectedDir);
        EXPECT_GE(dot, cosThresh)
            << "Cell (" << cell.x << "," << cell.y << ") gradient direction mismatch";
    }

}}} // namespace Dia::ScalarField::Testing

// Verify MockPropagationPolicy is a valid DiaScalarField policy for SquareFieldTopology.
static_assert(sizeof(Dia::ScalarField::DiaScalarField<Dia::ScalarField::SquareFieldTopology,
                                                       Dia::ScalarField::Testing::MockPropagationPolicy>) > 0,
              "MockPropagationPolicy must be a valid DiaScalarField policy");
