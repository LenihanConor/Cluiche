// TestFloatMathsBoundary.cpp - Boundary tests for DiaMaths float operations
//
// Covers NaN/Inf propagation through FloatMaths, CoreMaths Clamp extremes,
// Vector2D normalize edge cases, and float comparison functions with special values.

#include <gtest/gtest.h>
#include <limits>
#include <cmath>
#include <DiaMaths/Core/MathsDefines.h>
#include <DiaMaths/Core/CoreMaths.h>
#include <DiaMaths/Core/FloatMaths.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Maths;
using namespace Dia::Maths::Float;

// ==============================================================================
// FloatMaths — NaN and Inf propagation
// ==============================================================================

// FAbs of NaN must remain NaN (fabsf propagates NaN per IEEE 754)
TEST(DiaMaths_FloatBoundary, FAbs_NaN_PropagatesNaN)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    float result = FAbs(nan);
    EXPECT_TRUE(std::isnan(result));
}

// FAbs of positive infinity returns positive infinity
TEST(DiaMaths_FloatBoundary, FAbs_PositiveInf_ReturnsPositiveInf)
{
    const float inf = std::numeric_limits<float>::infinity();
    float result = FAbs(inf);
    EXPECT_TRUE(std::isinf(result));
    EXPECT_GT(result, 0.0f);
}

// FAbs of negative infinity returns positive infinity
TEST(DiaMaths_FloatBoundary, FAbs_NegativeInf_ReturnsPositiveInf)
{
    const float inf = std::numeric_limits<float>::infinity();
    float result = FAbs(-inf);
    EXPECT_TRUE(std::isinf(result));
    EXPECT_GT(result, 0.0f);
}

// FSquare of infinity must be infinity (inf * inf = inf)
TEST(DiaMaths_FloatBoundary, FSquare_Inf_ReturnsInf)
{
    const float inf = std::numeric_limits<float>::infinity();
    float result = FSquare(inf);
    EXPECT_TRUE(std::isinf(result));
}

// FSelect branches on compare >= 0.0f; NaN comparisons are always false,
// so NaN compare must select the second argument (y).
TEST(DiaMaths_FloatBoundary, FSelect_NaNComparator_SelectsSecondArg)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    float result = FSelect(nan, 1.0f, 2.0f);
    EXPECT_FLOAT_EQ(result, 2.0f);
}

// ==============================================================================
// FSquareRoot — negative input asserts
// ==============================================================================

// FSquareRoot has DIA_ASSERT(number >= 0) — a negative input must trip the assert.
TEST(DiaMaths_FloatBoundary, FSquareRoot_NegativeInput_Asserts)
{
    EXPECT_DEATH(FSquareRoot(-1.0f), ".*");
}

// FSquareRoot of positive infinity is well-defined: sqrt(+inf) = +inf
TEST(DiaMaths_FloatBoundary, FSquareRoot_PositiveInf_ReturnsInf)
{
    const float inf = std::numeric_limits<float>::infinity();
    float result = FSquareRoot(inf);
    EXPECT_TRUE(std::isinf(result));
    EXPECT_GT(result, 0.0f);
}

// ==============================================================================
// CoreMaths::Clamp — float extremes
// ==============================================================================

// Clamp(+Inf, 0, 1) must return 1.0f because +inf > max.
TEST(DiaMaths_FloatBoundary, Clamp_PositiveInf_ClampsToMax)
{
    const float inf = std::numeric_limits<float>::infinity();
    float result = Clamp(inf, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(result, 1.0f);
}

// Clamp(-Inf, 0, 1) must return 0.0f because -inf < min.
TEST(DiaMaths_FloatBoundary, Clamp_NegativeInf_ClampsToMin)
{
    const float inf = std::numeric_limits<float>::infinity();
    float result = Clamp(-inf, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

// Clamp(NaN, 0, 1) — NaN comparisons always return false, so NaN passes both
// guards and is returned unchanged. The test just verifies there is no crash.
TEST(DiaMaths_FloatBoundary, Clamp_NaN_DoesNotCrash)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    // No assertion about the return value — only that calling it is safe.
    EXPECT_NO_FATAL_FAILURE(Clamp(nan, 0.0f, 1.0f));
}

// ==============================================================================
// Float comparison functions — NaN and Inf inputs
// ==============================================================================

// FEqual computes FAbs(a - b) < epsilon.  NaN arithmetic yields NaN, and
// NaN < epsilon is false, so FEqual(NaN, NaN) must return false.
TEST(DiaMaths_FloatBoundary, FEqual_NaN_ReturnsFalse)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(FEqual(nan, nan));
}

// FEqual(Inf, Inf): FAbs(inf - inf) = NaN, NaN < epsilon is false → false.
TEST(DiaMaths_FloatBoundary, FEqual_InfVsInf_ReturnsFalse)
{
    const float inf = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(FEqual(inf, inf));
}

// FEqual(Inf, 0): FAbs(inf - 0) = inf, inf < epsilon is false → false.
TEST(DiaMaths_FloatBoundary, FEqual_InfVsZero_ReturnsFalse)
{
    const float inf = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(FEqual(inf, 0.0f));
}

// ==============================================================================
// Vector2D — normalize of zero vector
// ==============================================================================

// Vector2D::Normalize() calls DIA_ASSERT(mag > 0) — normalizing a zero vector asserts.
TEST(DiaMaths_FloatBoundary, Vector2D_Normalize_ZeroVector_Asserts)
{
    Vector2D v(0.0f, 0.0f);
    EXPECT_DEATH(v.Normalize(), ".*");
}

// Vector2D::NormalizeSafe() on a zero vector must not assert and must return
// the safe fallback (1, 0) as defined in Vector2D.cpp.
TEST(DiaMaths_FloatBoundary, Vector2D_NormalizeSafe_ZeroVector_ReturnsFallback)
{
    Vector2D v(0.0f, 0.0f);
    v.NormalizeSafe();
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
}
