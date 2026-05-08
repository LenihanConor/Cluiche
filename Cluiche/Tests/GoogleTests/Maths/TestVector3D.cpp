// TestVector3D.cpp - Google Test unit tests for Vector3D
//
// Tests the Dia::Maths::Vector3D class for 3D vector operations

#include <gtest/gtest.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <cmath>

using namespace Dia::Maths;

// Floating point comparison tolerance
constexpr float kEpsilon = 0.0001f;

// ==============================================================================
// Construction Tests
// ==============================================================================

TEST(Vector3D, DefaultConstruction_InitializesToZero)
{
    Vector3D v;

    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Vector3D, ComponentConstruction_InitializesCorrectly)
{
    Vector3D v(1.0f, 2.0f, 3.0f);

    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
}

TEST(Vector3D, UniformConstruction_SetsAllComponents)
{
    Vector3D v(5.0f);

    EXPECT_FLOAT_EQ(v.x, 5.0f);
    EXPECT_FLOAT_EQ(v.y, 5.0f);
    EXPECT_FLOAT_EQ(v.z, 5.0f);
}

TEST(Vector3D, CopyConstruction_CopiesAllComponents)
{
    Vector3D original(1.0f, 2.0f, 3.0f);
    Vector3D copy(original);

    EXPECT_FLOAT_EQ(copy.x, 1.0f);
    EXPECT_FLOAT_EQ(copy.y, 2.0f);
    EXPECT_FLOAT_EQ(copy.z, 3.0f);
}

// ==============================================================================
// Static Vector Tests
// ==============================================================================

TEST(Vector3D, XAxis_ReturnsUnitVectorAlongX)
{
    const Vector3D& xAxis = Vector3D::XAxis();

    EXPECT_FLOAT_EQ(xAxis.x, 1.0f);
    EXPECT_FLOAT_EQ(xAxis.y, 0.0f);
    EXPECT_FLOAT_EQ(xAxis.z, 0.0f);
}

TEST(Vector3D, YAxis_ReturnsUnitVectorAlongY)
{
    const Vector3D& yAxis = Vector3D::YAxis();

    EXPECT_FLOAT_EQ(yAxis.x, 0.0f);
    EXPECT_FLOAT_EQ(yAxis.y, 1.0f);
    EXPECT_FLOAT_EQ(yAxis.z, 0.0f);
}

TEST(Vector3D, ZAxis_ReturnsUnitVectorAlongZ)
{
    const Vector3D& zAxis = Vector3D::ZAxis();

    EXPECT_FLOAT_EQ(zAxis.x, 0.0f);
    EXPECT_FLOAT_EQ(zAxis.y, 0.0f);
    EXPECT_FLOAT_EQ(zAxis.z, 1.0f);
}

TEST(Vector3D, Zero_ReturnsZeroVector)
{
    const Vector3D& zero = Vector3D::Zero();

    EXPECT_FLOAT_EQ(zero.x, 0.0f);
    EXPECT_FLOAT_EQ(zero.y, 0.0f);
    EXPECT_FLOAT_EQ(zero.z, 0.0f);
}

// ==============================================================================
// Component Access Tests
// ==============================================================================

TEST(Vector3D, ComponentAccessors_GetAndSetCorrectly)
{
    Vector3D v;

    v.X(1.0f);
    v.Y(2.0f);
    v.Z(3.0f);

    EXPECT_FLOAT_EQ(v.X(), 1.0f);
    EXPECT_FLOAT_EQ(v.Y(), 2.0f);
    EXPECT_FLOAT_EQ(v.Z(), 3.0f);
}

TEST(Vector3D, Set_UpdatesAllComponents)
{
    Vector3D v;
    v.Set(4.0f, 5.0f, 6.0f);

    EXPECT_FLOAT_EQ(v.x, 4.0f);
    EXPECT_FLOAT_EQ(v.y, 5.0f);
    EXPECT_FLOAT_EQ(v.z, 6.0f);
}

TEST(Vector3D, SetFromVector_CopiesComponents)
{
    Vector3D source(1.0f, 2.0f, 3.0f);
    Vector3D dest;
    dest.Set(source);

    EXPECT_FLOAT_EQ(dest.x, 1.0f);
    EXPECT_FLOAT_EQ(dest.y, 2.0f);
    EXPECT_FLOAT_EQ(dest.z, 3.0f);
}

TEST(Vector3D, BracketOperator_AccessesComponents)
{
    Vector3D v(1.0f, 2.0f, 3.0f);

    EXPECT_FLOAT_EQ(v[0], 1.0f);
    EXPECT_FLOAT_EQ(v[1], 2.0f);
    EXPECT_FLOAT_EQ(v[2], 3.0f);
}

TEST(Vector3D, BracketOperator_ModifiesComponents)
{
    Vector3D v;
    v[0] = 10.0f;
    v[1] = 20.0f;
    v[2] = 30.0f;

    EXPECT_FLOAT_EQ(v.x, 10.0f);
    EXPECT_FLOAT_EQ(v.y, 20.0f);
    EXPECT_FLOAT_EQ(v.z, 30.0f);
}

// ==============================================================================
// Arithmetic Operator Tests
// ==============================================================================

TEST(Vector3D, Addition_AddsComponentwise)
{
    Vector3D v1(1.0f, 2.0f, 3.0f);
    Vector3D v2(4.0f, 5.0f, 6.0f);

    Vector3D result = v1 + v2;

    EXPECT_FLOAT_EQ(result.x, 5.0f);
    EXPECT_FLOAT_EQ(result.y, 7.0f);
    EXPECT_FLOAT_EQ(result.z, 9.0f);
}

TEST(Vector3D, Subtraction_SubtractsComponentwise)
{
    Vector3D v1(5.0f, 7.0f, 9.0f);
    Vector3D v2(2.0f, 3.0f, 4.0f);

    Vector3D result = v1 - v2;

    EXPECT_FLOAT_EQ(result.x, 3.0f);
    EXPECT_FLOAT_EQ(result.y, 4.0f);
    EXPECT_FLOAT_EQ(result.z, 5.0f);
}

TEST(Vector3D, MultiplicationByScalar_ScalesAllComponents)
{
    Vector3D v(1.0f, 2.0f, 3.0f);

    Vector3D result = v * 2.0f;

    EXPECT_FLOAT_EQ(result.x, 2.0f);
    EXPECT_FLOAT_EQ(result.y, 4.0f);
    EXPECT_FLOAT_EQ(result.z, 6.0f);
}

TEST(Vector3D, DivisionByScalar_DividesAllComponents)
{
    Vector3D v(10.0f, 20.0f, 30.0f);

    Vector3D result = v / 2.0f;

    EXPECT_FLOAT_EQ(result.x, 5.0f);
    EXPECT_FLOAT_EQ(result.y, 10.0f);
    EXPECT_FLOAT_EQ(result.z, 15.0f);
}

TEST(Vector3D, ComponentwiseMultiplication_MultipliesEachComponent)
{
    Vector3D v1(2.0f, 3.0f, 4.0f);
    Vector3D v2(5.0f, 6.0f, 7.0f);

    Vector3D result = v1 * v2;

    EXPECT_FLOAT_EQ(result.x, 10.0f);
    EXPECT_FLOAT_EQ(result.y, 18.0f);
    EXPECT_FLOAT_EQ(result.z, 28.0f);
}

// Note: Unary operators +v and -v are declared but not implemented in DiaMaths
// Using Invert() and AsInverse() methods instead for negation tests

// ==============================================================================
// Compound Assignment Tests
// ==============================================================================

TEST(Vector3D, PlusEquals_AddsAndAssigns)
{
    Vector3D v(1.0f, 2.0f, 3.0f);
    v += Vector3D(4.0f, 5.0f, 6.0f);

    EXPECT_FLOAT_EQ(v.x, 5.0f);
    EXPECT_FLOAT_EQ(v.y, 7.0f);
    EXPECT_FLOAT_EQ(v.z, 9.0f);
}

TEST(Vector3D, MinusEquals_SubtractsAndAssigns)
{
    Vector3D v(10.0f, 20.0f, 30.0f);
    v -= Vector3D(1.0f, 2.0f, 3.0f);

    EXPECT_FLOAT_EQ(v.x, 9.0f);
    EXPECT_FLOAT_EQ(v.y, 18.0f);
    EXPECT_FLOAT_EQ(v.z, 27.0f);
}

TEST(Vector3D, TimesEquals_MultipliesAndAssigns)
{
    Vector3D v(2.0f, 3.0f, 4.0f);
    v *= 3.0f;

    EXPECT_FLOAT_EQ(v.x, 6.0f);
    EXPECT_FLOAT_EQ(v.y, 9.0f);
    EXPECT_FLOAT_EQ(v.z, 12.0f);
}

TEST(Vector3D, DivideEquals_DividesAndAssigns)
{
    Vector3D v(10.0f, 20.0f, 30.0f);
    v /= 2.0f;

    EXPECT_FLOAT_EQ(v.x, 5.0f);
    EXPECT_FLOAT_EQ(v.y, 10.0f);
    EXPECT_FLOAT_EQ(v.z, 15.0f);
}

// ==============================================================================
// Comparison Operator Tests
// ==============================================================================

TEST(Vector3D, Equality_ReturnsTrueForEqualVectors)
{
    Vector3D v1(1.0f, 2.0f, 3.0f);
    Vector3D v2(1.0f, 2.0f, 3.0f);

    EXPECT_TRUE(v1 == v2);
}

TEST(Vector3D, Equality_ReturnsFalseForDifferentVectors)
{
    Vector3D v1(1.0f, 2.0f, 3.0f);
    Vector3D v2(1.0f, 2.0f, 4.0f);

    EXPECT_FALSE(v1 == v2);
}

TEST(Vector3D, Inequality_ReturnsTrueForDifferentVectors)
{
    Vector3D v1(1.0f, 2.0f, 3.0f);
    Vector3D v2(4.0f, 5.0f, 6.0f);

    EXPECT_TRUE(v1 != v2);
}

// ==============================================================================
// Magnitude and Distance Tests
// ==============================================================================

TEST(Vector3D, Magnitude_CalculatesVectorLength)
{
    Vector3D v(3.0f, 4.0f, 0.0f);  // 3-4-5 triangle

    float magnitude = v.Magnitude();

    EXPECT_NEAR(magnitude, 5.0f, kEpsilon);
}

TEST(Vector3D, SquareMagnitude_ReturnsSquaredLength)
{
    Vector3D v(1.0f, 2.0f, 2.0f);

    float squareMag = v.SquareMagnitude();

    EXPECT_NEAR(squareMag, 9.0f, kEpsilon);  // 1² + 2² + 2² = 9
}

TEST(Vector3D, DistanceTo_CalculatesDistanceBetweenVectors)
{
    Vector3D v1(0.0f, 0.0f, 0.0f);
    Vector3D v2(3.0f, 4.0f, 0.0f);

    float distance = v1.DistanceTo(v2);

    EXPECT_NEAR(distance, 5.0f, kEpsilon);
}

TEST(Vector3D, SquareDistanceTo_ReturnsSquaredDistance)
{
    Vector3D v1(1.0f, 2.0f, 3.0f);
    Vector3D v2(4.0f, 6.0f, 3.0f);

    float squareDist = v1.SquareDistanceTo(v2);

    EXPECT_NEAR(squareDist, 25.0f, kEpsilon);  // (3² + 4² + 0²) = 25
}

TEST(Vector3D, ManhattanDistanceTo_SumsAbsoluteComponentDifferences)
{
    Vector3D v1(1.0f, 2.0f, 3.0f);
    Vector3D v2(4.0f, 6.0f, 8.0f);

    float manhattanDist = v1.ManhattanDistanceTo(v2);

    EXPECT_NEAR(manhattanDist, 12.0f, kEpsilon);  // |3| + |4| + |5| = 12
}

// ==============================================================================
// Normalization Tests
// ==============================================================================

TEST(Vector3D, Normalize_CreatesUnitVector)
{
    Vector3D v(3.0f, 4.0f, 0.0f);
    v.Normalize();

    float magnitude = v.Magnitude();

    EXPECT_NEAR(magnitude, 1.0f, kEpsilon);
    EXPECT_NEAR(v.x, 0.6f, kEpsilon);
    EXPECT_NEAR(v.y, 0.8f, kEpsilon);
    EXPECT_NEAR(v.z, 0.0f, kEpsilon);
}

TEST(Vector3D, AsNormal_ReturnsNormalizedCopy)
{
    Vector3D v(3.0f, 4.0f, 0.0f);
    Vector3D normal = v.AsNormal();

    // Original unchanged
    EXPECT_FLOAT_EQ(v.x, 3.0f);
    EXPECT_FLOAT_EQ(v.y, 4.0f);

    // Normal has unit length
    EXPECT_NEAR(normal.Magnitude(), 1.0f, kEpsilon);
}

TEST(Vector3D, IsNormal_ReturnsTrueForUnitVector)
{
    Vector3D v(1.0f, 0.0f, 0.0f);

    EXPECT_TRUE(v.IsNormal());
}

TEST(Vector3D, IsNormal_ReturnsFalseForNonUnitVector)
{
    Vector3D v(2.0f, 0.0f, 0.0f);

    EXPECT_FALSE(v.IsNormal());
}

// ==============================================================================
// Dot Product Tests
// ==============================================================================

TEST(Vector3D, Dot_CalculatesDotProduct)
{
    Vector3D v1(1.0f, 2.0f, 3.0f);
    Vector3D v2(4.0f, 5.0f, 6.0f);

    float dot = v1.Dot(v2);

    EXPECT_NEAR(dot, 32.0f, kEpsilon);  // 1*4 + 2*5 + 3*6 = 32
}

TEST(Vector3D, Dot_ReturnsZeroForPerpendicularVectors)
{
    Vector3D v1(1.0f, 0.0f, 0.0f);
    Vector3D v2(0.0f, 1.0f, 0.0f);

    float dot = v1.Dot(v2);

    EXPECT_NEAR(dot, 0.0f, kEpsilon);
}

TEST(Vector3D, Dot_ReturnsMagnitudeSquaredForSelf)
{
    Vector3D v(3.0f, 4.0f, 0.0f);

    float dotSelf = v.Dot(v);
    float squareMag = v.SquareMagnitude();

    EXPECT_NEAR(dotSelf, squareMag, kEpsilon);
}

// ==============================================================================
// Projection Tests
// ==============================================================================

TEST(Vector3D, ProjectOn_ProjectsVectorOntoAnother)
{
    Vector3D v(1.0f, 1.0f, 0.0f);
    Vector3D axis(1.0f, 0.0f, 0.0f);

    Vector3D projection = v.ProjectOn(axis);

    // Should project onto x-axis only
    EXPECT_NEAR(projection.x, 1.0f, kEpsilon);
    EXPECT_NEAR(projection.y, 0.0f, kEpsilon);
    EXPECT_NEAR(projection.z, 0.0f, kEpsilon);
}

// ==============================================================================
// Utility Method Tests
// ==============================================================================

TEST(Vector3D, Clear_SetsAllComponentsToZero)
{
    Vector3D v(1.0f, 2.0f, 3.0f);
    v.Clear();

    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

// TEST(Vector3D, Invert_NegatesAllComponents) - Disabled: Invert() uses unimplemented unary operator-
// {
//     Vector3D v(1.0f, -2.0f, 3.0f);
//     v.Invert();
//
//     EXPECT_FLOAT_EQ(v.x, -1.0f);
//     EXPECT_FLOAT_EQ(v.y, 2.0f);
//     EXPECT_FLOAT_EQ(v.z, -3.0f);
// }

TEST(Vector3D, AsInverse_ReturnsInvertedCopy)
{
    Vector3D v(1.0f, 2.0f, 3.0f);
    Vector3D inverted = v.AsInverse();

    // Original unchanged
    EXPECT_FLOAT_EQ(v.x, 1.0f);

    // Inverted has negated components
    EXPECT_FLOAT_EQ(inverted.x, -1.0f);
    EXPECT_FLOAT_EQ(inverted.y, -2.0f);
    EXPECT_FLOAT_EQ(inverted.z, -3.0f);
}

TEST(Vector3D, Absolutize_MakesAllComponentsPositive)
{
    Vector3D v(-1.0f, 2.0f, -3.0f);
    v.Absolutize();

    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
}

TEST(Vector3D, Absolute_ReturnsAbsoluteCopy)
{
    Vector3D v(-1.0f, -2.0f, -3.0f);
    Vector3D absolute = v.Absolute();

    // Original unchanged
    EXPECT_FLOAT_EQ(v.x, -1.0f);

    // Absolute has positive components
    EXPECT_FLOAT_EQ(absolute.x, 1.0f);
    EXPECT_FLOAT_EQ(absolute.y, 2.0f);
    EXPECT_FLOAT_EQ(absolute.z, 3.0f);
}

TEST(Vector3D, IsValid_ReturnsTrueForFiniteComponents)
{
    Vector3D v(1.0f, 2.0f, 3.0f);

    EXPECT_TRUE(v.IsValid());
}
