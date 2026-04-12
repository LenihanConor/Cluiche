// TestVector2D.cpp - Google Test unit tests for Vector2D
//
// Tests Vector2D from DiaMaths

#include <gtest/gtest.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Core/FloatMaths.h>
#include <DiaMaths/Core/Angle.h>

using namespace Dia::Maths;

// ==============================================================================
// Vector2D Construction Tests
// ==============================================================================

TEST(Vector2D, DefaultConstruction_ZeroVector)
{
    Vector2D vector;

    EXPECT_FLOAT_EQ(vector.x, 0.0f);
    EXPECT_FLOAT_EQ(vector.y, 0.0f);
}

TEST(Vector2D, ConstructWithTwoValues_SetsComponents)
{
    Vector2D vector(1.0f, 2.0f);

    EXPECT_FLOAT_EQ(vector.x, 1.0f);
    EXPECT_FLOAT_EQ(vector.y, 2.0f);
}

TEST(Vector2D, ConstructWithSingleValue_SetsBothComponents)
{
    Vector2D vector(3.0f);

    EXPECT_FLOAT_EQ(vector.x, 3.0f);
    EXPECT_FLOAT_EQ(vector.y, 3.0f);
}

TEST(Vector2D, CopyConstructor_CopiesComponents)
{
    Vector2D vector1(3.0f, 4.0f);
    Vector2D vector2(vector1);

    EXPECT_FLOAT_EQ(vector2.x, 3.0f);
    EXPECT_FLOAT_EQ(vector2.y, 4.0f);
}

TEST(Vector2D, AssignmentOperator_CopiesComponents)
{
    Vector2D vector1(3.0f, 4.0f);
    Vector2D vector2;

    vector2 = vector1;

    EXPECT_FLOAT_EQ(vector2.x, 3.0f);
    EXPECT_FLOAT_EQ(vector2.y, 4.0f);
}

// ==============================================================================
// Vector2D Static Constants Tests
// ==============================================================================

TEST(Vector2D, XAxis_ReturnsUnitX)
{
    Vector2D xAxis = Vector2D::XAxis();

    EXPECT_FLOAT_EQ(xAxis.x, 1.0f);
    EXPECT_FLOAT_EQ(xAxis.y, 0.0f);
}

TEST(Vector2D, YAxis_ReturnsUnitY)
{
    Vector2D yAxis = Vector2D::YAxis();

    EXPECT_FLOAT_EQ(yAxis.x, 0.0f);
    EXPECT_FLOAT_EQ(yAxis.y, 1.0f);
}

TEST(Vector2D, Zero_ReturnsZeroVector)
{
    Vector2D zero = Vector2D::Zero();

    EXPECT_FLOAT_EQ(zero.x, 0.0f);
    EXPECT_FLOAT_EQ(zero.y, 0.0f);
}

TEST(Vector2D, Max_ReturnsMaxValues)
{
    Vector2D max = Vector2D::Max();

    EXPECT_FLOAT_EQ(max.x, Float::Max());
    EXPECT_FLOAT_EQ(max.y, Float::Max());
}

TEST(Vector2D, Min_ReturnsMinValues)
{
    Vector2D min = Vector2D::Min();

    EXPECT_FLOAT_EQ(min.x, Float::Min());
    EXPECT_FLOAT_EQ(min.y, Float::Min());
}

// ==============================================================================
// Vector2D Component Access Tests
// ==============================================================================

TEST(Vector2D, ArrayAccessor_SetsAndGets)
{
    Vector2D vector;

    vector[0] = 1.0f;
    vector[1] = 2.0f;

    EXPECT_FLOAT_EQ(vector.x, 1.0f);
    EXPECT_FLOAT_EQ(vector.y, 2.0f);
    EXPECT_FLOAT_EQ(vector[0], vector.x);
    EXPECT_FLOAT_EQ(vector[1], vector.y);
}

TEST(Vector2D, ArrayAccessor_OutOfBounds_Asserts)
{
    Vector2D vector;

    EXPECT_DEATH(vector[-1] = 3.0f, ".*");
    EXPECT_DEATH(vector[2] = 3.0f, ".*");
}

TEST(Vector2D, XYAccessors_SetAndGet)
{
    Vector2D vector;

    vector.X(1.0f);
    vector.Y(2.0f);

    EXPECT_FLOAT_EQ(vector.x, 1.0f);
    EXPECT_FLOAT_EQ(vector.y, 2.0f);
    EXPECT_FLOAT_EQ(vector.X(), vector.x);
    EXPECT_FLOAT_EQ(vector.Y(), vector.y);
}

TEST(Vector2D, Set_WithTwoValues_SetsComponents)
{
    Vector2D vector;

    vector.Set(1.0f, 2.0f);

    EXPECT_FLOAT_EQ(vector.x, 1.0f);
    EXPECT_FLOAT_EQ(vector.y, 2.0f);
}

TEST(Vector2D, Set_WithVector_CopiesComponents)
{
    Vector2D vector1(1.0f, 2.0f);
    Vector2D vector2;

    vector2.Set(vector1);

    EXPECT_FLOAT_EQ(vector2.x, 1.0f);
    EXPECT_FLOAT_EQ(vector2.y, 2.0f);
}

// ==============================================================================
// Vector2D Unary Operators Tests
// ==============================================================================

TEST(Vector2D, UnaryMinus_NegatesComponents)
{
    Vector2D vector1(1.0f, 2.0f);
    Vector2D vector2 = -vector1;

    EXPECT_FLOAT_EQ(vector2.x, -1.0f);
    EXPECT_FLOAT_EQ(vector2.y, -2.0f);
}

// ==============================================================================
// Vector2D Comparison Operators Tests
// ==============================================================================

TEST(Vector2D, EqualityOperator_SameVectors_ReturnsTrue)
{
    Vector2D vector1(1.0f, 2.0f);
    Vector2D vector2(1.0f, 2.0f);

    EXPECT_TRUE(vector1 == vector2);
}

TEST(Vector2D, InequalityOperator_DifferentVectors_ReturnsTrue)
{
    Vector2D vector1(1.0f, 2.0f);
    Vector2D vector2(2.0f, 3.0f);

    EXPECT_TRUE(vector1 != vector2);
}

// ==============================================================================
// Vector2D Compound Assignment Tests
// ==============================================================================

TEST(Vector2D, PlusEquals_AddsVector)
{
    Vector2D vector1(1.0f, 2.0f);
    Vector2D vector2(1.0f, 1.0f);

    vector2 += vector1;

    EXPECT_FLOAT_EQ(vector1.x, 1.0f);
    EXPECT_FLOAT_EQ(vector1.y, 2.0f);
    EXPECT_FLOAT_EQ(vector2.x, 2.0f);
    EXPECT_FLOAT_EQ(vector2.y, 3.0f);
}

TEST(Vector2D, MinusEquals_SubtractsVector)
{
    Vector2D vector1(1.0f, 2.0f);
    Vector2D vector2(2.0f, 3.0f);

    vector2 -= vector1;

    EXPECT_FLOAT_EQ(vector1.x, 1.0f);
    EXPECT_FLOAT_EQ(vector1.y, 2.0f);
    EXPECT_FLOAT_EQ(vector2.x, 1.0f);
    EXPECT_FLOAT_EQ(vector2.y, 1.0f);
}

TEST(Vector2D, MultiplyEquals_Vector_MultipliesComponents)
{
    Vector2D vector1(1.0f, 2.0f);
    Vector2D vector2(2.0f, 3.0f);

    vector2 *= vector1;

    EXPECT_FLOAT_EQ(vector1.x, 1.0f);
    EXPECT_FLOAT_EQ(vector1.y, 2.0f);
    EXPECT_FLOAT_EQ(vector2.x, 2.0f);
    EXPECT_FLOAT_EQ(vector2.y, 6.0f);
}

TEST(Vector2D, MultiplyEquals_Scalar_ScalesVector)
{
    Vector2D vector(2.0f, 3.0f);

    vector *= 1.5f;

    EXPECT_FLOAT_EQ(vector.x, 3.0f);
    EXPECT_FLOAT_EQ(vector.y, 4.5f);
}

TEST(Vector2D, DivideEquals_Scalar_ScalesVector)
{
    Vector2D vector(3.0f, 4.5f);

    vector /= 1.5f;

    EXPECT_FLOAT_EQ(vector.x, 2.0f);
    EXPECT_FLOAT_EQ(vector.y, 3.0f);
}

// ==============================================================================
// Vector2D Binary Arithmetic Tests
// ==============================================================================

TEST(Vector2D, Addition_AddsVectors)
{
    Vector2D vector1(1.0f, 2.0f);
    Vector2D vector2(1.0f, 1.0f);

    Vector2D result = vector1 + vector2;

    EXPECT_FLOAT_EQ(result.x, 2.0f);
    EXPECT_FLOAT_EQ(result.y, 3.0f);
    EXPECT_FLOAT_EQ(vector1.x, 1.0f);
    EXPECT_FLOAT_EQ(vector2.x, 1.0f);
}

TEST(Vector2D, Subtraction_SubtractsVectors)
{
    Vector2D vector1(1.0f, 2.0f);
    Vector2D vector2(1.0f, 1.0f);

    Vector2D result = vector1 - vector2;

    EXPECT_FLOAT_EQ(result.x, 0.0f);
    EXPECT_FLOAT_EQ(result.y, 1.0f);
}

TEST(Vector2D, Multiplication_Vector_MultipliesComponents)
{
    Vector2D vector1(1.0f, 2.0f);
    Vector2D vector2(2.0f, 2.0f);

    Vector2D result = vector1 * vector2;

    EXPECT_FLOAT_EQ(result.x, 2.0f);
    EXPECT_FLOAT_EQ(result.y, 4.0f);
}

TEST(Vector2D, Division_Vector_DividesComponents)
{
    Vector2D vector1(1.0f, 2.0f);
    Vector2D vector2(2.0f, 2.0f);

    Vector2D result = vector1 / vector2;

    EXPECT_FLOAT_EQ(result.x, 0.5f);
    EXPECT_FLOAT_EQ(result.y, 1.0f);
}

TEST(Vector2D, Multiplication_Scalar_ScalesVector)
{
    Vector2D vector(1.0f, 2.0f);

    Vector2D result = vector * 2.0f;

    EXPECT_FLOAT_EQ(result.x, 2.0f);
    EXPECT_FLOAT_EQ(result.y, 4.0f);
}

TEST(Vector2D, Division_Scalar_ScalesVector)
{
    Vector2D vector(1.0f, 2.0f);

    Vector2D result = vector / 2.0f;

    EXPECT_FLOAT_EQ(result.x, 0.5f);
    EXPECT_FLOAT_EQ(result.y, 1.0f);
}

// ==============================================================================
// Vector2D Utility Methods Tests
// ==============================================================================

TEST(Vector2D, AsInverse_ReturnsNegated)
{
    Vector2D vector1(1.0f, 2.0f);
    Vector2D vector2 = vector1.AsInverse();

    EXPECT_FLOAT_EQ(vector2.x, -1.0f);
    EXPECT_FLOAT_EQ(vector2.y, -2.0f);
}

TEST(Vector2D, Invert_NegatesInPlace)
{
    Vector2D vector(1.0f, 2.0f);

    vector.Invert();

    EXPECT_FLOAT_EQ(vector.x, -1.0f);
    EXPECT_FLOAT_EQ(vector.y, -2.0f);
}

TEST(Vector2D, IsValid_ValidVector_ReturnsTrue)
{
    Vector2D vector(1.0f, 2.0f);

    EXPECT_TRUE(vector.IsValid());
}

TEST(Vector2D, Clear_SetsToZero)
{
    Vector2D vector(1.0f, 2.0f);

    vector.Clear();

    EXPECT_FLOAT_EQ(vector.x, 0.0f);
    EXPECT_FLOAT_EQ(vector.y, 0.0f);
}

// ==============================================================================
// Vector2D Length Tests
// ==============================================================================

TEST(Vector2D, SquareMagnitude_ReturnsSquaredLength)
{
    Vector2D vector(3.0f, 4.0f);

    float sqrMag = vector.SquareMagnitude();

    EXPECT_FLOAT_EQ(sqrMag, 25.0f);  // 3^2 + 4^2 = 25
}

TEST(Vector2D, Magnitude_ReturnsLength)
{
    Vector2D vector(3.0f, 4.0f);

    float mag = vector.Magnitude();

    EXPECT_FLOAT_EQ(mag, 5.0f);  // sqrt(9 + 16) = 5
}

TEST(Vector2D, Normalize_MakesUnitVector)
{
    Vector2D vector(3.0f, 4.0f);

    vector.Normalize();

    EXPECT_FLOAT_EQ(vector.x, 0.6f);
    EXPECT_FLOAT_EQ(vector.y, 0.8f);
    EXPECT_NEAR(vector.Magnitude(), 1.0f, 0.0001f);
}

TEST(Vector2D, AsNormal_ReturnsUnitVector)
{
    Vector2D vector(3.0f, 4.0f);

    Vector2D normalized = vector.AsNormal();

    EXPECT_FLOAT_EQ(normalized.x, 0.6f);
    EXPECT_FLOAT_EQ(normalized.y, 0.8f);
    EXPECT_NEAR(normalized.Magnitude(), 1.0f, 0.0001f);
    // Original unchanged
    EXPECT_FLOAT_EQ(vector.x, 3.0f);
    EXPECT_FLOAT_EQ(vector.y, 4.0f);
}

// ==============================================================================
// Vector2D Dot Product Tests
// ==============================================================================

TEST(Vector2D, Dot_General_ReturnsProduct)
{
    Vector2D vector1(1.0f, 2.0f);
    Vector2D vector2(2.0f, 3.0f);

    float dot = vector1.Dot(vector2);

    EXPECT_FLOAT_EQ(dot, 8.0f);  // 1*2 + 2*3 = 8
}
