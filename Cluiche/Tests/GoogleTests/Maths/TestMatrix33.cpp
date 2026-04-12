// TestMatrix33.cpp - Google Test unit tests for Matrix33
//
// Tests the Dia::Maths::Matrix33 class for 2D transformations

#include <gtest/gtest.h>
#include <DiaMaths/Matrix/Matrix33.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Core/Angle.h>

using namespace Dia::Maths;

constexpr float kEpsilon = 0.0001f;

// ==============================================================================
// Construction Tests
// ==============================================================================

TEST(Matrix33, DefaultConstruction_CreatesZeroMatrix)
{
    Matrix33 m;

    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            EXPECT_FLOAT_EQ(m(row, col), 0.0f) << "Element at (" << row << "," << col << ") should be 0";
        }
    }
}

TEST(Matrix33, ComponentConstruction_InitializesAllElements)
{
    Matrix33 m(1, 2, 3,
               4, 5, 6,
               7, 8, 9);

    EXPECT_FLOAT_EQ(m(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(m(0, 1), 2.0f);
    EXPECT_FLOAT_EQ(m(0, 2), 3.0f);
    EXPECT_FLOAT_EQ(m(1, 0), 4.0f);
    EXPECT_FLOAT_EQ(m(1, 1), 5.0f);
    EXPECT_FLOAT_EQ(m(1, 2), 6.0f);
    EXPECT_FLOAT_EQ(m(2, 0), 7.0f);
    EXPECT_FLOAT_EQ(m(2, 1), 8.0f);
    EXPECT_FLOAT_EQ(m(2, 2), 9.0f);
}

TEST(Matrix33, CopyConstruction_CopiesAllElements)
{
    Matrix33 original(1, 2, 3, 4, 5, 6, 7, 8, 9);
    Matrix33 copy(original);

    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            EXPECT_FLOAT_EQ(copy(row, col), original(row, col));
        }
    }
}

// ==============================================================================
// Factory Method Tests
// ==============================================================================

TEST(Matrix33, Identity_CreatesIdentityMatrix)
{
    Matrix33 identity = Matrix33::Identity();

    // Diagonal should be 1
    EXPECT_FLOAT_EQ(identity(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(identity(1, 1), 1.0f);
    EXPECT_FLOAT_EQ(identity(2, 2), 1.0f);

    // Off-diagonal should be 0
    EXPECT_FLOAT_EQ(identity(0, 1), 0.0f);
    EXPECT_FLOAT_EQ(identity(0, 2), 0.0f);
    EXPECT_FLOAT_EQ(identity(1, 0), 0.0f);
    EXPECT_FLOAT_EQ(identity(1, 2), 0.0f);
    EXPECT_FLOAT_EQ(identity(2, 0), 0.0f);
    EXPECT_FLOAT_EQ(identity(2, 1), 0.0f);
}

TEST(Matrix33, FromTranslation_CreatesTranslationMatrix)
{
    Vector2D translation(10.0f, 20.0f);
    Matrix33 m = Matrix33::FromTranslation(translation);

    // Translation should be in the last column
    EXPECT_FLOAT_EQ(m(0, 2), 10.0f);
    EXPECT_FLOAT_EQ(m(1, 2), 20.0f);

    // Should be identity with translation
    EXPECT_FLOAT_EQ(m(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(m(1, 1), 1.0f);
    EXPECT_FLOAT_EQ(m(2, 2), 1.0f);
}

TEST(Matrix33, FromScale_CreatesScaleMatrixFromVector)
{
    Vector2D scale(2.0f, 3.0f);
    Matrix33 m = Matrix33::FromScale(scale);

    // Scale should be on diagonal
    EXPECT_FLOAT_EQ(m(0, 0), 2.0f);
    EXPECT_FLOAT_EQ(m(1, 1), 3.0f);
    EXPECT_FLOAT_EQ(m(2, 2), 1.0f);
}

TEST(Matrix33, FromScale_CreatesUniformScaleMatrix)
{
    Matrix33 m = Matrix33::FromScale(5.0f);

    // Uniform scale on diagonal
    EXPECT_FLOAT_EQ(m(0, 0), 5.0f);
    EXPECT_FLOAT_EQ(m(1, 1), 5.0f);
    EXPECT_FLOAT_EQ(m(2, 2), 1.0f);
}

TEST(Matrix33, FromRotation_CreatesRotationMatrix)
{
    // 90 degree rotation
    Angle angle = Angle::FromDegrees(90.0f);
    Matrix33 m = Matrix33::FromRotation(angle);

    // cos(90°) = 0, sin(90°) = 1
    EXPECT_NEAR(m(0, 0), 0.0f, kEpsilon);   // cos
    EXPECT_NEAR(m(0, 1), -1.0f, kEpsilon);  // -sin
    EXPECT_NEAR(m(1, 0), 1.0f, kEpsilon);   // sin
    EXPECT_NEAR(m(1, 1), 0.0f, kEpsilon);   // cos
}

// ==============================================================================
// Assignment Operator Tests
// ==============================================================================

TEST(Matrix33, AssignmentOperator_CopiesAllElements)
{
    Matrix33 source(1, 2, 3, 4, 5, 6, 7, 8, 9);
    Matrix33 dest;

    dest = source;

    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            EXPECT_FLOAT_EQ(dest(row, col), source(row, col));
        }
    }
}

// ==============================================================================
// Matrix Arithmetic Tests
// ==============================================================================

TEST(Matrix33, Addition_AddsMatricesElementwise)
{
    Matrix33 m1(1, 2, 3, 4, 5, 6, 7, 8, 9);
    Matrix33 m2(9, 8, 7, 6, 5, 4, 3, 2, 1);

    Matrix33 result = m1 + m2;

    EXPECT_FLOAT_EQ(result(0, 0), 10.0f);
    EXPECT_FLOAT_EQ(result(1, 1), 10.0f);
    EXPECT_FLOAT_EQ(result(2, 2), 10.0f);
}

TEST(Matrix33, Subtraction_SubtractsMatricesElementwise)
{
    Matrix33 m1(10, 10, 10, 10, 10, 10, 10, 10, 10);
    Matrix33 m2(1, 2, 3, 4, 5, 6, 7, 8, 9);

    Matrix33 result = m1 - m2;

    EXPECT_FLOAT_EQ(result(0, 0), 9.0f);
    EXPECT_FLOAT_EQ(result(0, 1), 8.0f);
    EXPECT_FLOAT_EQ(result(2, 2), 1.0f);
}

TEST(Matrix33, ScalarMultiplication_ScalesAllElements)
{
    Matrix33 m(1, 2, 3, 4, 5, 6, 7, 8, 9);

    Matrix33 result = m * 2.0f;

    EXPECT_FLOAT_EQ(result(0, 0), 2.0f);
    EXPECT_FLOAT_EQ(result(1, 1), 10.0f);
    EXPECT_FLOAT_EQ(result(2, 2), 18.0f);
}

TEST(Matrix33, MatrixMultiplication_MultipliesMatrices)
{
    Matrix33 identity = Matrix33::Identity();
    Matrix33 m(1, 2, 3, 4, 5, 6, 7, 8, 9);

    Matrix33 result = identity * m;

    // Identity * M = M
    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            EXPECT_FLOAT_EQ(result(row, col), m(row, col));
        }
    }
}

TEST(Matrix33, Negation_NegatesAllElements)
{
    Matrix33 m(1, -2, 3, -4, 5, -6, 7, -8, 9);

    Matrix33 result = -m;

    EXPECT_FLOAT_EQ(result(0, 0), -1.0f);
    EXPECT_FLOAT_EQ(result(0, 1), 2.0f);
    EXPECT_FLOAT_EQ(result(1, 0), 4.0f);
}

// ==============================================================================
// Compound Assignment Tests
// ==============================================================================

TEST(Matrix33, PlusEquals_AddsAndAssigns)
{
    Matrix33 m1(1, 2, 3, 4, 5, 6, 7, 8, 9);
    Matrix33 m2(1, 1, 1, 1, 1, 1, 1, 1, 1);

    m1 += m2;

    EXPECT_FLOAT_EQ(m1(0, 0), 2.0f);
    EXPECT_FLOAT_EQ(m1(1, 1), 6.0f);
}

TEST(Matrix33, TimesEquals_MultipliesAndAssigns)
{
    Matrix33 m = Matrix33::Identity();
    float scalar = 5.0f;

    m *= scalar;

    EXPECT_FLOAT_EQ(m(0, 0), 5.0f);
    EXPECT_FLOAT_EQ(m(1, 1), 5.0f);
    EXPECT_FLOAT_EQ(m(2, 2), 5.0f);
}

// ==============================================================================
// Comparison Operator Tests
// ==============================================================================

TEST(Matrix33, Equality_ReturnsTrueForEqualMatrices)
{
    Matrix33 m1(1, 2, 3, 4, 5, 6, 7, 8, 9);
    Matrix33 m2(1, 2, 3, 4, 5, 6, 7, 8, 9);

    EXPECT_TRUE(m1 == m2);
}

TEST(Matrix33, Inequality_ReturnsTrueForDifferentMatrices)
{
    Matrix33 m1(1, 2, 3, 4, 5, 6, 7, 8, 9);
    Matrix33 m2(9, 8, 7, 6, 5, 4, 3, 2, 1);

    EXPECT_TRUE(m1 != m2);
}

// ==============================================================================
// Element Access Tests
// ==============================================================================

TEST(Matrix33, ParenthesisOperator_AccessesElements)
{
    Matrix33 m(1, 2, 3, 4, 5, 6, 7, 8, 9);

    EXPECT_FLOAT_EQ(m(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(m(1, 2), 6.0f);
    EXPECT_FLOAT_EQ(m(2, 1), 8.0f);
}

TEST(Matrix33, ParenthesisOperator_ModifiesElements)
{
    Matrix33 m;

    m(0, 0) = 1.0f;
    m(1, 1) = 5.0f;
    m(2, 2) = 9.0f;

    EXPECT_FLOAT_EQ(m(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(m(1, 1), 5.0f);
    EXPECT_FLOAT_EQ(m(2, 2), 9.0f);
}

// ==============================================================================
// Vector Transformation Tests
// ==============================================================================

TEST(Matrix33, TransformPoint_AppliesTransformation)
{
    Vector2D translation(10.0f, 20.0f);
    Matrix33 m = Matrix33::FromTranslation(translation);

    Vector2D point(5.0f, 5.0f);
    Vector2D transformed = m.TransformPoint(point);

    EXPECT_NEAR(transformed.x, 15.0f, kEpsilon);
    EXPECT_NEAR(transformed.y, 25.0f, kEpsilon);
}

TEST(Matrix33, TransformDirection_IgnoresTranslation)
{
    Vector2D translation(10.0f, 20.0f);
    Matrix33 m = Matrix33::FromTranslation(translation);

    Vector2D direction(1.0f, 0.0f);
    Vector2D transformed = m.TransformDirection(direction);

    // Direction should be unchanged (translation doesn't affect directions)
    EXPECT_NEAR(transformed.x, 1.0f, kEpsilon);
    EXPECT_NEAR(transformed.y, 0.0f, kEpsilon);
}

TEST(Matrix33, TransformPoint_AppliesScale)
{
    Matrix33 m = Matrix33::FromScale(Vector2D(2.0f, 3.0f));

    Vector2D point(10.0f, 10.0f);
    Vector2D scaled = m.TransformPoint(point);

    EXPECT_NEAR(scaled.x, 20.0f, kEpsilon);
    EXPECT_NEAR(scaled.y, 30.0f, kEpsilon);
}

// ==============================================================================
// Matrix Property Tests
// ==============================================================================

TEST(Matrix33, SetIdentity_SetsToIdentityMatrix)
{
    Matrix33 m(1, 2, 3, 4, 5, 6, 7, 8, 9);
    m.SetIdentity();

    EXPECT_FLOAT_EQ(m(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(m(1, 1), 1.0f);
    EXPECT_FLOAT_EQ(m(2, 2), 1.0f);
    EXPECT_FLOAT_EQ(m(0, 1), 0.0f);
    EXPECT_FLOAT_EQ(m(1, 0), 0.0f);
}

TEST(Matrix33, Transpose_SwapsRowsAndColumns)
{
    Matrix33 m(1, 2, 3,
               4, 5, 6,
               7, 8, 9);

    Matrix33 transposed = m.Transpose();

    EXPECT_FLOAT_EQ(transposed(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(transposed(0, 1), 4.0f);
    EXPECT_FLOAT_EQ(transposed(0, 2), 7.0f);
    EXPECT_FLOAT_EQ(transposed(1, 0), 2.0f);
    EXPECT_FLOAT_EQ(transposed(1, 1), 5.0f);
    EXPECT_FLOAT_EQ(transposed(1, 2), 8.0f);
}

TEST(Matrix33, Determinant_CalculatesCorrectly)
{
    Matrix33 identity = Matrix33::Identity();

    float det = identity.Determinant();

    EXPECT_NEAR(det, 1.0f, kEpsilon);
}

TEST(Matrix33, Inverse_CreatesInverseMatrix)
{
    Matrix33 m = Matrix33::FromTranslation(Vector2D(10.0f, 20.0f));
    Matrix33 inverse = m.Inverse();

    // M * M^-1 = I
    Matrix33 product = m * inverse;

    EXPECT_NEAR(product(0, 0), 1.0f, kEpsilon);
    EXPECT_NEAR(product(1, 1), 1.0f, kEpsilon);
    EXPECT_NEAR(product(2, 2), 1.0f, kEpsilon);
    EXPECT_NEAR(product(0, 1), 0.0f, kEpsilon);
    EXPECT_NEAR(product(1, 0), 0.0f, kEpsilon);
}

// ==============================================================================
// Component Extraction Tests
// ==============================================================================

TEST(Matrix33, GetTranslation_ExtractsTranslationComponent)
{
    Vector2D translation(15.0f, 25.0f);
    Matrix33 m = Matrix33::FromTranslation(translation);

    Vector2D extracted = m.GetTranslation();

    EXPECT_NEAR(extracted.x, 15.0f, kEpsilon);
    EXPECT_NEAR(extracted.y, 25.0f, kEpsilon);
}

TEST(Matrix33, GetScale_ExtractsScaleComponent)
{
    Vector2D scale(3.0f, 4.0f);
    Matrix33 m = Matrix33::FromScale(scale);

    Vector2D extracted = m.GetScale();

    EXPECT_NEAR(extracted.x, 3.0f, kEpsilon);
    EXPECT_NEAR(extracted.y, 4.0f, kEpsilon);
}

// ==============================================================================
// Combined Transformation Tests (TRS)
// ==============================================================================

TEST(Matrix33, FromTRS_CombinesTransformations)
{
    Vector2D translation(10.0f, 20.0f);
    Angle rotation = Angle::FromDegrees(0.0f);  // No rotation
    Vector2D scale(2.0f, 2.0f);

    Matrix33 trs = Matrix33::FromTRS(translation, rotation, scale);

    // Apply to a point
    Vector2D point(1.0f, 1.0f);
    Vector2D transformed = trs.TransformPoint(point);

    // Should be scaled then translated: (1*2 + 10, 1*2 + 20) = (12, 22)
    EXPECT_NEAR(transformed.x, 12.0f, kEpsilon);
    EXPECT_NEAR(transformed.y, 22.0f, kEpsilon);
}
