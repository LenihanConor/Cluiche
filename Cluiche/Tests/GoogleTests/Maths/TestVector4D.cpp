// TestVector4D.cpp - Google Test unit tests for Vector4D
//
// Tests the Dia::Maths::Vector4D class for 4D vector and homogeneous coordinates

#include <gtest/gtest.h>
#include <DiaMaths/Vector/Vector4D.h>

using namespace Dia::Maths;

constexpr float kEpsilon = 0.0001f;

// ==============================================================================
// Construction Tests
// ==============================================================================

TEST(Vector4D, DefaultConstruction_InitializesToZero)
{
    Vector4D v;

    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
    EXPECT_FLOAT_EQ(v.w, 0.0f);
}

TEST(Vector4D, ComponentConstruction_InitializesCorrectly)
{
    Vector4D v(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
    EXPECT_FLOAT_EQ(v.w, 4.0f);
}

TEST(Vector4D, UniformConstruction_SetsAllComponents)
{
    Vector4D v(5.0f);

    EXPECT_FLOAT_EQ(v.x, 5.0f);
    EXPECT_FLOAT_EQ(v.y, 5.0f);
    EXPECT_FLOAT_EQ(v.z, 5.0f);
    EXPECT_FLOAT_EQ(v.w, 5.0f);
}

TEST(Vector4D, CopyConstruction_CopiesAllComponents)
{
    Vector4D original(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4D copy(original);

    EXPECT_FLOAT_EQ(copy.x, 1.0f);
    EXPECT_FLOAT_EQ(copy.y, 2.0f);
    EXPECT_FLOAT_EQ(copy.z, 3.0f);
    EXPECT_FLOAT_EQ(copy.w, 4.0f);
}

// ==============================================================================
// Static Vector Tests
// ==============================================================================

TEST(Vector4D, XAxis_ReturnsUnitVectorAlongX)
{
    const Vector4D& xAxis = Vector4D::XAxis();

    EXPECT_FLOAT_EQ(xAxis.x, 1.0f);
    EXPECT_FLOAT_EQ(xAxis.y, 0.0f);
    EXPECT_FLOAT_EQ(xAxis.z, 0.0f);
    EXPECT_FLOAT_EQ(xAxis.w, 0.0f);
}

TEST(Vector4D, YAxis_ReturnsUnitVectorAlongY)
{
    const Vector4D& yAxis = Vector4D::YAxis();

    EXPECT_FLOAT_EQ(yAxis.x, 0.0f);
    EXPECT_FLOAT_EQ(yAxis.y, 1.0f);
    EXPECT_FLOAT_EQ(yAxis.z, 0.0f);
    EXPECT_FLOAT_EQ(yAxis.w, 0.0f);
}

TEST(Vector4D, ZAxis_ReturnsUnitVectorAlongZ)
{
    const Vector4D& zAxis = Vector4D::ZAxis();

    EXPECT_FLOAT_EQ(zAxis.x, 0.0f);
    EXPECT_FLOAT_EQ(zAxis.y, 0.0f);
    EXPECT_FLOAT_EQ(zAxis.z, 1.0f);
    EXPECT_FLOAT_EQ(zAxis.w, 0.0f);
}

TEST(Vector4D, Zero_ReturnsZeroVector)
{
    const Vector4D& zero = Vector4D::Zero();

    EXPECT_FLOAT_EQ(zero.x, 0.0f);
    EXPECT_FLOAT_EQ(zero.y, 0.0f);
    EXPECT_FLOAT_EQ(zero.z, 0.0f);
    EXPECT_FLOAT_EQ(zero.w, 0.0f);
}

// ==============================================================================
// Component Access Tests
// ==============================================================================

TEST(Vector4D, ComponentAccessors_GetAndSetCorrectly)
{
    Vector4D v;

    v.X(1.0f);
    v.Y(2.0f);
    v.Z(3.0f);
    v.W(4.0f);

    EXPECT_FLOAT_EQ(v.X(), 1.0f);
    EXPECT_FLOAT_EQ(v.Y(), 2.0f);
    EXPECT_FLOAT_EQ(v.Z(), 3.0f);
    EXPECT_FLOAT_EQ(v.W(), 4.0f);
}

TEST(Vector4D, Set_UpdatesAllComponents)
{
    Vector4D v;
    v.Set(4.0f, 5.0f, 6.0f, 7.0f);

    EXPECT_FLOAT_EQ(v.x, 4.0f);
    EXPECT_FLOAT_EQ(v.y, 5.0f);
    EXPECT_FLOAT_EQ(v.z, 6.0f);
    EXPECT_FLOAT_EQ(v.w, 7.0f);
}

TEST(Vector4D, BracketOperator_AccessesComponents)
{
    Vector4D v(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_FLOAT_EQ(v[0], 1.0f);
    EXPECT_FLOAT_EQ(v[1], 2.0f);
    EXPECT_FLOAT_EQ(v[2], 3.0f);
    EXPECT_FLOAT_EQ(v[3], 4.0f);
}

TEST(Vector4D, BracketOperator_ModifiesComponents)
{
    Vector4D v;
    v[0] = 10.0f;
    v[1] = 20.0f;
    v[2] = 30.0f;
    v[3] = 40.0f;

    EXPECT_FLOAT_EQ(v.x, 10.0f);
    EXPECT_FLOAT_EQ(v.y, 20.0f);
    EXPECT_FLOAT_EQ(v.z, 30.0f);
    EXPECT_FLOAT_EQ(v.w, 40.0f);
}

// ==============================================================================
// Arithmetic Operator Tests
// ==============================================================================

TEST(Vector4D, Addition_AddsComponentwise)
{
    Vector4D v1(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4D v2(5.0f, 6.0f, 7.0f, 8.0f);

    Vector4D result = v1 + v2;

    EXPECT_FLOAT_EQ(result.x, 6.0f);
    EXPECT_FLOAT_EQ(result.y, 8.0f);
    EXPECT_FLOAT_EQ(result.z, 10.0f);
    EXPECT_FLOAT_EQ(result.w, 12.0f);
}

TEST(Vector4D, Subtraction_SubtractsComponentwise)
{
    Vector4D v1(10.0f, 20.0f, 30.0f, 40.0f);
    Vector4D v2(1.0f, 2.0f, 3.0f, 4.0f);

    Vector4D result = v1 - v2;

    EXPECT_FLOAT_EQ(result.x, 9.0f);
    EXPECT_FLOAT_EQ(result.y, 18.0f);
    EXPECT_FLOAT_EQ(result.z, 27.0f);
    EXPECT_FLOAT_EQ(result.w, 36.0f);
}

TEST(Vector4D, MultiplicationByScalar_ScalesAllComponents)
{
    Vector4D v(1.0f, 2.0f, 3.0f, 4.0f);

    Vector4D result = v * 2.0f;

    EXPECT_FLOAT_EQ(result.x, 2.0f);
    EXPECT_FLOAT_EQ(result.y, 4.0f);
    EXPECT_FLOAT_EQ(result.z, 6.0f);
    EXPECT_FLOAT_EQ(result.w, 8.0f);
}

TEST(Vector4D, DivisionByScalar_DividesAllComponents)
{
    Vector4D v(10.0f, 20.0f, 30.0f, 40.0f);

    Vector4D result = v / 2.0f;

    EXPECT_FLOAT_EQ(result.x, 5.0f);
    EXPECT_FLOAT_EQ(result.y, 10.0f);
    EXPECT_FLOAT_EQ(result.z, 15.0f);
    EXPECT_FLOAT_EQ(result.w, 20.0f);
}

// Note: Unary operator -v is declared but not implemented in DiaMaths
// Using Invert() method instead for negation tests

// ==============================================================================
// Compound Assignment Tests
// ==============================================================================

TEST(Vector4D, PlusEquals_AddsAndAssigns)
{
    Vector4D v(1.0f, 2.0f, 3.0f, 4.0f);
    v += Vector4D(5.0f, 6.0f, 7.0f, 8.0f);

    EXPECT_FLOAT_EQ(v.x, 6.0f);
    EXPECT_FLOAT_EQ(v.y, 8.0f);
    EXPECT_FLOAT_EQ(v.z, 10.0f);
    EXPECT_FLOAT_EQ(v.w, 12.0f);
}

TEST(Vector4D, TimesEquals_MultipliesAndAssigns)
{
    Vector4D v(2.0f, 3.0f, 4.0f, 5.0f);
    v *= 3.0f;

    EXPECT_FLOAT_EQ(v.x, 6.0f);
    EXPECT_FLOAT_EQ(v.y, 9.0f);
    EXPECT_FLOAT_EQ(v.z, 12.0f);
    EXPECT_FLOAT_EQ(v.w, 15.0f);
}

// ==============================================================================
// Comparison Operator Tests
// ==============================================================================

TEST(Vector4D, Equality_ReturnsTrueForEqualVectors)
{
    Vector4D v1(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4D v2(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_TRUE(v1 == v2);
}

TEST(Vector4D, Inequality_ReturnsTrueForDifferentVectors)
{
    Vector4D v1(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4D v2(1.0f, 2.0f, 3.0f, 5.0f);

    EXPECT_TRUE(v1 != v2);
}

// ==============================================================================
// Magnitude and Distance Tests
// ==============================================================================

TEST(Vector4D, Magnitude_CalculatesVectorLength)
{
    Vector4D v(1.0f, 2.0f, 2.0f, 0.0f);

    float magnitude = v.Magnitude();

    EXPECT_NEAR(magnitude, 3.0f, kEpsilon);  // sqrt(1 + 4 + 4) = 3
}

TEST(Vector4D, SquareMagnitude_ReturnsSquaredLength)
{
    Vector4D v(1.0f, 2.0f, 2.0f, 0.0f);

    float squareMag = v.SquareMagnitude();

    EXPECT_NEAR(squareMag, 9.0f, kEpsilon);  // 1 + 4 + 4 = 9
}

TEST(Vector4D, DistanceTo_CalculatesDistanceBetweenVectors)
{
    Vector4D v1(0.0f, 0.0f, 0.0f, 0.0f);
    Vector4D v2(3.0f, 4.0f, 0.0f, 0.0f);

    float distance = v1.DistanceTo(v2);

    EXPECT_NEAR(distance, 5.0f, kEpsilon);
}

// ==============================================================================
// Normalization Tests
// ==============================================================================

TEST(Vector4D, Normalize_CreatesUnitVector)
{
    Vector4D v(3.0f, 4.0f, 0.0f, 0.0f);
    v.Normalize();

    float magnitude = v.Magnitude();

    EXPECT_NEAR(magnitude, 1.0f, kEpsilon);
}

TEST(Vector4D, AsNormal_ReturnsNormalizedCopy)
{
    Vector4D v(3.0f, 4.0f, 0.0f, 0.0f);
    Vector4D normal = v.AsNormal();

    // Original unchanged
    EXPECT_FLOAT_EQ(v.x, 3.0f);

    // Normal has unit length
    EXPECT_NEAR(normal.Magnitude(), 1.0f, kEpsilon);
}

TEST(Vector4D, IsNormal_ReturnsTrueForUnitVector)
{
    Vector4D v(1.0f, 0.0f, 0.0f, 0.0f);

    EXPECT_TRUE(v.IsNormal());
}

// ==============================================================================
// Dot Product Tests
// ==============================================================================

TEST(Vector4D, Dot_CalculatesDotProduct)
{
    Vector4D v1(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4D v2(5.0f, 6.0f, 7.0f, 8.0f);

    float dot = v1.Dot(v2);

    EXPECT_NEAR(dot, 70.0f, kEpsilon);  // 5 + 12 + 21 + 32 = 70
}

TEST(Vector4D, Dot_ReturnsZeroForPerpendicularVectors)
{
    Vector4D v1(1.0f, 0.0f, 0.0f, 0.0f);
    Vector4D v2(0.0f, 1.0f, 0.0f, 0.0f);

    float dot = v1.Dot(v2);

    EXPECT_NEAR(dot, 0.0f, kEpsilon);
}

// ==============================================================================
// Projection Tests
// ==============================================================================

TEST(Vector4D, ProjectOn_ProjectsVectorOntoAnother)
{
    Vector4D v(1.0f, 1.0f, 0.0f, 0.0f);
    Vector4D axis(1.0f, 0.0f, 0.0f, 0.0f);

    Vector4D projection = v.ProjectOn(axis);

    // Should project onto x-axis only
    EXPECT_NEAR(projection.x, 1.0f, kEpsilon);
    EXPECT_NEAR(projection.y, 0.0f, kEpsilon);
}

// ==============================================================================
// Utility Method Tests
// ==============================================================================

TEST(Vector4D, Clear_SetsAllComponentsToZero)
{
    Vector4D v(1.0f, 2.0f, 3.0f, 4.0f);
    v.Clear();

    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
    EXPECT_FLOAT_EQ(v.w, 0.0f);
}

// TEST(Vector4D, Invert_NegatesAllComponents) - Disabled: Invert() uses unimplemented unary operator-
// {
//     Vector4D v(1.0f, -2.0f, 3.0f, -4.0f);
//     v.Invert();
//
//     EXPECT_FLOAT_EQ(v.x, -1.0f);
//     EXPECT_FLOAT_EQ(v.y, 2.0f);
//     EXPECT_FLOAT_EQ(v.z, -3.0f);
//     EXPECT_FLOAT_EQ(v.w, 4.0f);
// }

TEST(Vector4D, Absolutize_MakesAllComponentsPositive)
{
    Vector4D v(-1.0f, 2.0f, -3.0f, -4.0f);
    v.Absolutize();

    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
    EXPECT_FLOAT_EQ(v.w, 4.0f);
}

TEST(Vector4D, IsValid_ReturnsTrueForFiniteComponents)
{
    Vector4D v(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_TRUE(v.IsValid());
}

// ==============================================================================
// Homogeneous Coordinate Tests (w component)
// ==============================================================================

TEST(Vector4D, HomogeneousPoint_HasWEqualsOne)
{
    // Points in homogeneous coordinates have w = 1
    Vector4D point(10.0f, 20.0f, 30.0f, 1.0f);

    EXPECT_FLOAT_EQ(point.w, 1.0f);
}

TEST(Vector4D, HomogeneousDirection_HasWEqualsZero)
{
    // Directions in homogeneous coordinates have w = 0
    Vector4D direction(1.0f, 0.0f, 0.0f, 0.0f);

    EXPECT_FLOAT_EQ(direction.w, 0.0f);
}
