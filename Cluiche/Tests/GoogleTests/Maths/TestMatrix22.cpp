// TestMatrix22.cpp - Google Test unit tests for Matrix22
//
// Tests Matrix22 from DiaMaths

#include <gtest/gtest.h>
#include <DiaMaths/Matrix/Matrix22.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Core/FloatMaths.h>

using namespace Dia::Maths;

// ==============================================================================
// Matrix22 Construction Tests
// ==============================================================================

TEST(Matrix22, DefaultConstruction_CreatesIdentity)
{
    Matrix22 m;

    EXPECT_FLOAT_EQ(m[0], 1.0f);
    EXPECT_FLOAT_EQ(m[1], 0.0f);
    EXPECT_FLOAT_EQ(m[2], 0.0f);
    EXPECT_FLOAT_EQ(m[3], 1.0f);
}

TEST(Matrix22, ConstructWithValues_SetsElements)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_FLOAT_EQ(m[0], 1.0f);
    EXPECT_FLOAT_EQ(m[1], 2.0f);
    EXPECT_FLOAT_EQ(m[2], 3.0f);
    EXPECT_FLOAT_EQ(m[3], 4.0f);
}

TEST(Matrix22, SetWithValues_SetsElements)
{
    Matrix22 m;
    m.Set(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_FLOAT_EQ(m[0], 1.0f);
    EXPECT_FLOAT_EQ(m[1], 2.0f);
    EXPECT_FLOAT_EQ(m[2], 3.0f);
    EXPECT_FLOAT_EQ(m[3], 4.0f);
}

TEST(Matrix22, ConstructWithVectors_SetsColumns)
{
    Matrix22 m(Vector2D(4.0f, 3.0f), Vector2D(2.0f, 1.0f));

    EXPECT_FLOAT_EQ(m[0], 4.0f);
    EXPECT_FLOAT_EQ(m[1], 3.0f);
    EXPECT_FLOAT_EQ(m[2], 2.0f);
    EXPECT_FLOAT_EQ(m[3], 1.0f);
}

TEST(Matrix22, SetWithVectors_SetsColumns)
{
    Matrix22 m;
    m.Set(Vector2D(4.0f, 3.0f), Vector2D(2.0f, 1.0f));

    EXPECT_FLOAT_EQ(m[0], 4.0f);
    EXPECT_FLOAT_EQ(m[1], 3.0f);
    EXPECT_FLOAT_EQ(m[2], 2.0f);
    EXPECT_FLOAT_EQ(m[3], 1.0f);
}

TEST(Matrix22, CopyConstructor_CopiesElements)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 m2(m1);

    EXPECT_FLOAT_EQ(m2[0], 1.0f);
    EXPECT_FLOAT_EQ(m2[1], 2.0f);
    EXPECT_FLOAT_EQ(m2[2], 3.0f);
    EXPECT_FLOAT_EQ(m2[3], 4.0f);
}

TEST(Matrix22, SetWithMatrix_CopiesElements)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 m2;
    m2.Set(m1);

    EXPECT_FLOAT_EQ(m2[0], 1.0f);
    EXPECT_FLOAT_EQ(m2[1], 2.0f);
    EXPECT_FLOAT_EQ(m2[2], 3.0f);
    EXPECT_FLOAT_EQ(m2[3], 4.0f);
}

TEST(Matrix22, AssignmentOperator_CopiesElements)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 m2;
    m2 = m1;

    EXPECT_FLOAT_EQ(m2[0], 1.0f);
    EXPECT_FLOAT_EQ(m2[1], 2.0f);
    EXPECT_FLOAT_EQ(m2[2], 3.0f);
    EXPECT_FLOAT_EQ(m2[3], 4.0f);
}

// ==============================================================================
// Matrix22 Comparison Tests
// ==============================================================================

TEST(Matrix22, EqualityOperator_SameMatrix_ReturnsTrue)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 m2(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_TRUE(m1 == m1);
    EXPECT_TRUE(m1 == m2);
}

TEST(Matrix22, InequalityOperator_DifferentMatrix_ReturnsTrue)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 m3(1.0f, 2.0f, 3.0f, 0.0f);

    EXPECT_TRUE(m1 != m3);
}

// ==============================================================================
// Matrix22 Element Access Tests
// ==============================================================================

TEST(Matrix22, ArrayAccessor_OutOfBounds_Asserts)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_DEATH({ m[4] = 3.0f; }, ".*");
    EXPECT_DEATH({ float a = m[4]; }, ".*");
}

TEST(Matrix22, Element_OutOfBounds_Asserts)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_DEATH({ float a = m.Element(4); }, ".*");
    EXPECT_DEATH({ float a = m.Element(4, 0); }, ".*");
    EXPECT_DEATH({ float a = m.Element(0, 4); }, ".*");
}

TEST(Matrix22, Element_SingleIndex_ReturnsCorrectValue)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_FLOAT_EQ(m.Element(0), 1.0f);
    EXPECT_FLOAT_EQ(m.Element(1), 2.0f);
    EXPECT_FLOAT_EQ(m.Element(2), 3.0f);
    EXPECT_FLOAT_EQ(m.Element(3), 4.0f);
}

TEST(Matrix22, Element_RowColumn_ReturnsCorrectValue)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_FLOAT_EQ(m.Element(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(m.Element(0, 1), 2.0f);
    EXPECT_FLOAT_EQ(m.Element(1, 0), 3.0f);
    EXPECT_FLOAT_EQ(m.Element(1, 1), 4.0f);
}

TEST(Matrix22, SetElement_OutOfBounds_Asserts)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_DEATH(m.SetElement(4, 3.0f), ".*");
    EXPECT_DEATH(m.SetElement(4, 0, 3.0f), ".*");
    EXPECT_DEATH(m.SetElement(0, 4, 3.0f), ".*");
}

TEST(Matrix22, SetElement_SetsCorrectValue)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    m.SetElement(1, 5.0f);
    m.SetElement(1, 0, 6.0f);
    m[3] = 7.0f;

    EXPECT_FLOAT_EQ(m.Element(0), 1.0f);
    EXPECT_FLOAT_EQ(m.Element(1), 5.0f);
    EXPECT_FLOAT_EQ(m.Element(2), 6.0f);
    EXPECT_FLOAT_EQ(m.Element(3), 7.0f);
}

TEST(Matrix22, XAxis_GetsColumn)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);
    Vector2D xAxis;

    m.XAxis(xAxis);

    EXPECT_FLOAT_EQ(xAxis.x, 1.0f);
    EXPECT_FLOAT_EQ(xAxis.y, 2.0f);
}

TEST(Matrix22, YAxis_GetsColumn)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);
    Vector2D yAxis;

    m.YAxis(yAxis);

    EXPECT_FLOAT_EQ(yAxis.x, 3.0f);
    EXPECT_FLOAT_EQ(yAxis.y, 4.0f);
}

// ==============================================================================
// Matrix22 Clear Test
// ==============================================================================

TEST(Matrix22, Clear_SetsToZero)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    m.Clear();

    EXPECT_FLOAT_EQ(m.Element(0), 0.0f);
    EXPECT_FLOAT_EQ(m.Element(1), 0.0f);
    EXPECT_FLOAT_EQ(m.Element(2), 0.0f);
    EXPECT_FLOAT_EQ(m.Element(3), 0.0f);
}

// ==============================================================================
// Matrix22 Unary Operators Tests
// ==============================================================================

TEST(Matrix22, UnaryMinus_NegatesElements)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 m2 = -m1;

    EXPECT_FLOAT_EQ(m2.Element(0), -1.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), -2.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), -3.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), -4.0f);
}

// ==============================================================================
// Matrix22 Addition Tests
// ==============================================================================

TEST(Matrix22, Addition_AddsMatrices)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 m2(5.0f, 6.0f, 7.0f, 8.0f);

    Matrix22 m3 = m2 + m1;

    EXPECT_FLOAT_EQ(m3.Element(0), 6.0f);
    EXPECT_FLOAT_EQ(m3.Element(1), 8.0f);
    EXPECT_FLOAT_EQ(m3.Element(2), 10.0f);
    EXPECT_FLOAT_EQ(m3.Element(3), 12.0f);
}

TEST(Matrix22, PlusEquals_AddsMatrices)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 m2(5.0f, 6.0f, 7.0f, 8.0f);

    m2 += m1;

    EXPECT_FLOAT_EQ(m2.Element(0), 6.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 8.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 10.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), 12.0f);
}

// ==============================================================================
// Matrix22 Subtraction Tests
// ==============================================================================

TEST(Matrix22, Subtraction_SubtractsMatrices)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 m2(5.0f, 6.0f, 7.0f, 8.0f);

    Matrix22 m3 = m2 - m1;

    EXPECT_FLOAT_EQ(m3.Element(0), 4.0f);
    EXPECT_FLOAT_EQ(m3.Element(1), 4.0f);
    EXPECT_FLOAT_EQ(m3.Element(2), 4.0f);
    EXPECT_FLOAT_EQ(m3.Element(3), 4.0f);
}

TEST(Matrix22, MinusEquals_SubtractsMatrices)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 m2(5.0f, 6.0f, 7.0f, 8.0f);

    m2 -= m1;

    EXPECT_FLOAT_EQ(m2.Element(0), 4.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 4.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 4.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), 4.0f);
}

// ==============================================================================
// Matrix22 Multiplication Tests
// ==============================================================================

TEST(Matrix22, Multiplication_MultipliesMatrices)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 m2(5.0f, 6.0f, 7.0f, 8.0f);

    Matrix22 m3 = m2 * m1;

    EXPECT_FLOAT_EQ(m3.Element(0), 23.0f);
    EXPECT_FLOAT_EQ(m3.Element(1), 34.0f);
    EXPECT_FLOAT_EQ(m3.Element(2), 31.0f);
    EXPECT_FLOAT_EQ(m3.Element(3), 46.0f);
}

TEST(Matrix22, MultiplyEquals_MultipliesMatrices)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);
    Matrix22 m2(5.0f, 6.0f, 7.0f, 8.0f);

    m2 *= m1;

    EXPECT_FLOAT_EQ(m2.Element(0), 23.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 34.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 31.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), 46.0f);
}

TEST(Matrix22, MultiplicationScalar_ScalesMatrix)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    Matrix22 m2 = m * 2.0f;

    EXPECT_FLOAT_EQ(m2.Element(0), 2.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 4.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 6.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), 8.0f);
}

TEST(Matrix22, MultiplyEqualsScalar_ScalesMatrix)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    m *= 2.0f;

    EXPECT_FLOAT_EQ(m.Element(0), 2.0f);
    EXPECT_FLOAT_EQ(m.Element(1), 4.0f);
    EXPECT_FLOAT_EQ(m.Element(2), 6.0f);
    EXPECT_FLOAT_EQ(m.Element(3), 8.0f);
}

// ==============================================================================
// Matrix22 Division Tests
// ==============================================================================

TEST(Matrix22, Division_DivideByZero_Asserts)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_DEATH({ m /= 0.0f; }, ".*");
    EXPECT_DEATH({ m = m / 0.0f; }, ".*");
}

TEST(Matrix22, DivisionScalar_DividesMatrix)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    Matrix22 m2 = m / 2.0f;

    EXPECT_FLOAT_EQ(m2.Element(0), 0.5f);
    EXPECT_FLOAT_EQ(m2.Element(1), 1.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 1.5f);
    EXPECT_FLOAT_EQ(m2.Element(3), 2.0f);
}

TEST(Matrix22, DivideEqualsScalar_DividesMatrix)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    m /= 2.0f;

    EXPECT_FLOAT_EQ(m.Element(0), 0.5f);
    EXPECT_FLOAT_EQ(m.Element(1), 1.0f);
    EXPECT_FLOAT_EQ(m.Element(2), 1.5f);
    EXPECT_FLOAT_EQ(m.Element(3), 2.0f);
}

// ==============================================================================
// Matrix22 Negation Tests
// ==============================================================================

TEST(Matrix22, AsNegative_ReturnsNegated)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);

    Matrix22 m2 = m1.AsNegative();

    EXPECT_FLOAT_EQ(m2.Element(0), -1.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), -2.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), -3.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), -4.0f);
}

TEST(Matrix22, Negative_NegatesInPlace)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    m.Negative();

    EXPECT_FLOAT_EQ(m.Element(0), -1.0f);
    EXPECT_FLOAT_EQ(m.Element(1), -2.0f);
    EXPECT_FLOAT_EQ(m.Element(2), -3.0f);
    EXPECT_FLOAT_EQ(m.Element(3), -4.0f);
}

TEST(Matrix22, Negative_DoubleNegation_ReturnsOriginal)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);

    m1.Negative();
    m1.Negative();

    EXPECT_FLOAT_EQ(m1.Element(0), 1.0f);
    EXPECT_FLOAT_EQ(m1.Element(1), 2.0f);
    EXPECT_FLOAT_EQ(m1.Element(2), 3.0f);
    EXPECT_FLOAT_EQ(m1.Element(3), 4.0f);
}

// ==============================================================================
// Matrix22 Transpose Tests
// ==============================================================================

TEST(Matrix22, AsTranspose_ReturnsTransposed)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);

    Matrix22 m2 = m1.AsTranspose();

    EXPECT_FLOAT_EQ(m2.Element(0), 1.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 3.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 2.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), 4.0f);
}

TEST(Matrix22, Transpose_TransposesInPlace)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    m.Transpose();

    EXPECT_FLOAT_EQ(m.Element(0), 1.0f);
    EXPECT_FLOAT_EQ(m.Element(1), 3.0f);
    EXPECT_FLOAT_EQ(m.Element(2), 2.0f);
    EXPECT_FLOAT_EQ(m.Element(3), 4.0f);
}

// ==============================================================================
// Matrix22 Scale Tests
// ==============================================================================

TEST(Matrix22, UniformScale_ScalesMatrix)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    Matrix22 m2 = m.UniformScale(2.0f);

    EXPECT_FLOAT_EQ(m2.Element(0), 2.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 2.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 3.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), 8.0f);
}

TEST(Matrix22, UniformScaleAndAsUniformScale_BothProduceSameResult)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);

    Matrix22 m2 = m1.UniformScale(2.0f);
    m1.AsUniformScale(2.0f);

    EXPECT_EQ(m1, m2);
    EXPECT_FLOAT_EQ(m2.Element(0), 2.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 2.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 3.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), 8.0f);
}

TEST(Matrix22, UniformScale_NegativeScale)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    Matrix22 m2 = m.UniformScale(-2.0f);

    EXPECT_FLOAT_EQ(m2.Element(0), -2.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 2.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 3.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), -8.0f);
}

TEST(Matrix22, UniformScale_FractionalScale)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    Matrix22 m2 = m.UniformScale(0.5f);

    EXPECT_FLOAT_EQ(m2.Element(0), 0.5f);
    EXPECT_FLOAT_EQ(m2.Element(1), 2.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 3.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), 2.0f);
}

// ==============================================================================
// Matrix22 Inverse Tests
// ==============================================================================

TEST(Matrix22, AsInverse_SingularMatrix_Asserts)
{
    Matrix22 m(0.0f, 0.0f, 0.0f, 1.0f);

    EXPECT_DEATH(m.AsInverse(), ".*");
}

TEST(Matrix22, Invert_SingularMatrix_Asserts)
{
    Matrix22 m(0.0f, 0.0f, 0.0f, 1.0f);

    EXPECT_DEATH(m.Invert(), ".*");
}

TEST(Matrix22, Invert_ReturnsInverse)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    Matrix22 m2 = m.Invert();

    EXPECT_FLOAT_EQ(m2.Element(0), -2.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 1.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 1.5f);
    EXPECT_FLOAT_EQ(m2.Element(3), -0.5f);
}

TEST(Matrix22, InvertAndAsInverse_BothProduceInverse)
{
    Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f);

    Matrix22 m2 = m1.Invert();
    m1.AsInverse();

    EXPECT_EQ(m1, m2);
    EXPECT_FLOAT_EQ(m2.Element(0), -2.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 1.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 1.5f);
    EXPECT_FLOAT_EQ(m2.Element(3), -0.5f);
}

// ==============================================================================
// Matrix22 Inverse Orthogonal Tests
// ==============================================================================

TEST(Matrix22, InvertOrthogonal_NonOrthogonal_Asserts)
{
    Matrix22 m(0.0f, 0.0f, 0.0f, 1.0f);

    EXPECT_DEATH(m.InvertOrthogonal(), ".*");
}

TEST(Matrix22, AsInverseOrthogonal_NonOrthogonal_Asserts)
{
    Matrix22 m(0.0f, 0.0f, 0.0f, 1.0f);

    EXPECT_DEATH(m.AsInverseOrthogonal(), ".*");
}

TEST(Matrix22, InvertOrthogonal_Identity_ReturnsIdentity)
{
    Matrix22 m(1.0f, 0.0f, 0.0f, 1.0f);

    Matrix22 m2 = m.Invert();

    EXPECT_FLOAT_EQ(m2.Element(0), 1.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 0.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 0.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), 1.0f);
}

// ==============================================================================
// Matrix22 Trace Test
// ==============================================================================

TEST(Matrix22, Trace_ReturnsSumOfDiagonal)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    float trace = m.Trace();

    EXPECT_FLOAT_EQ(trace, 5.0f);
}

// ==============================================================================
// Matrix22 Determinant Test
// ==============================================================================

TEST(Matrix22, Determinant_ReturnsCorrectValue)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    float det = m.Determinant();

    EXPECT_FLOAT_EQ(det, -2.0f);
}

// ==============================================================================
// Matrix22 Eigenvalues Test
// ==============================================================================

TEST(Matrix22, Eigenvalues_ComputesCorrectly)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    float eigenValue1, eigenValue2;
    m.Eigenvalues(eigenValue1, eigenValue2);

    EXPECT_TRUE(Float::FEqual(eigenValue1, 5.372281323269014f));
    EXPECT_TRUE(Float::FEqual(eigenValue2, -0.3722813232690143f));
}

// ==============================================================================
// Matrix22 Property Tests
// ==============================================================================

TEST(Matrix22, IsSymmetric_SymmetricMatrix_ReturnsTrue)
{
    Matrix22 m(1.0f, 3.0f, 3.0f, 4.0f);

    EXPECT_TRUE(m.IsSymmetric());
}

TEST(Matrix22, IsSymmetric_NonSymmetricMatrix_ReturnsFalse)
{
    Matrix22 m(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_FALSE(m.IsSymmetric());
}

TEST(Matrix22, IsSkewSymmetric_SkewSymmetricMatrix_ReturnsTrue)
{
    Matrix22 m(0.0f, -3.0f, 3.0f, 0.0f);

    EXPECT_TRUE(m.IsSkewSymmetric());
}

TEST(Matrix22, IsSkewSymmetric_NonSkewSymmetric_ReturnsFalse)
{
    Matrix22 m1(0.0f, 2.0f, 3.0f, 0.0f);
    Matrix22 m2(0.0f, 3.0f, 3.0f, 0.0f);

    EXPECT_FALSE(m1.IsSkewSymmetric());
    EXPECT_FALSE(m2.IsSkewSymmetric());
}

TEST(Matrix22, IsIdentity_IdentityMatrix_ReturnsTrue)
{
    Matrix22 m(1.0f, 0.0f, 0.0f, 1.0f);

    EXPECT_TRUE(m.IsIdentity());
}

TEST(Matrix22, IsIdentity_NonIdentity_ReturnsFalse)
{
    Matrix22 m(1.0f, -3.0f, 3.0f, 4.0f);

    EXPECT_FALSE(m.IsIdentity());
}

TEST(Matrix22, IsDiagonalMatrix_DiagonalMatrix_ReturnsTrue)
{
    Matrix22 m(1.0f, 0.0f, 0.0f, 4.0f);

    EXPECT_TRUE(m.IsDiagonalMatrix());
}

TEST(Matrix22, IsDiagonalMatrix_NonDiagonal_ReturnsFalse)
{
    Matrix22 m(1.0f, 1.0f, 0.0f, 1.0f);

    EXPECT_FALSE(m.IsDiagonalMatrix());
}

TEST(Matrix22, IsOrthogonal_OrthogonalMatrix_ReturnsTrue)
{
    Matrix22 m(1.0f, 0.0f, 0.0f, 1.0f);

    EXPECT_TRUE(m.IsOrthogonal());
}

TEST(Matrix22, IsOrthogonal_NonOrthogonal_ReturnsFalse)
{
    Matrix22 m(1.0f, 1.0f, 0.0f, 1.0f);

    EXPECT_FALSE(m.IsOrthogonal());
}

TEST(Matrix22, IsScaled_ScaledMatrix_ReturnsTrue)
{
    Matrix22 m1(2.0f, 0.0f, 0.0f, 2.0f);
    Matrix22 m2(1.0f, 1.0f, 0.0f, 1.0f);
    Matrix22 m3(2.0f, 0.0f, 0.0f, 1.0f);

    EXPECT_TRUE(m1.IsScaled());
    EXPECT_TRUE(m2.IsScaled());
    EXPECT_TRUE(m3.IsScaled());
}

TEST(Matrix22, IsScaled_Identity_ReturnsFalse)
{
    Matrix22 m(1.0f, 0.0f, 0.0f, 1.0f);

    EXPECT_FALSE(m.IsScaled());
}

TEST(Matrix22, IsUniformScale_UniformScaledMatrix_ReturnsTrue)
{
    Matrix22 m(2.0f, 0.0f, 0.0f, 2.0f);

    EXPECT_TRUE(m.IsUniformScale());
}

TEST(Matrix22, IsUniformScale_NonUniform_ReturnsFalse)
{
    Matrix22 m1(1.0f, 1.0f, 0.0f, 1.0f);
    Matrix22 m2(1.0f, 0.0f, 0.0f, 1.0f);
    Matrix22 m3(2.0f, 0.0f, 0.0f, 1.0f);

    EXPECT_FALSE(m1.IsUniformScale());
    EXPECT_FALSE(m2.IsUniformScale());
    EXPECT_FALSE(m3.IsUniformScale());
}

// ==============================================================================
// Matrix22 GetScale Tests
// ==============================================================================

TEST(Matrix22, GetScale_ReturnsScaleValues)
{
    Matrix22 m1(2.0f, 0.0f, 0.0f, 2.0f);
    float s1, s2;
    m1.GetScale(s1, s2);

    EXPECT_FLOAT_EQ(s1, 2.0f);
    EXPECT_FLOAT_EQ(s2, 2.0f);
}

TEST(Matrix22, GetScale_NonUniform_ReturnsCorrectValues)
{
    Matrix22 m(1.0f, 1.0f, 0.0f, 1.0f);
    float s1, s2;
    m.GetScale(s1, s2);

    EXPECT_FLOAT_EQ(s1, 1.4142135f);
    EXPECT_FLOAT_EQ(s2, 1.0f);
}

TEST(Matrix22, GetScale_Identity_ReturnsOne)
{
    Matrix22 m(1.0f, 0.0f, 0.0f, 1.0f);
    float s1, s2;
    m.GetScale(s1, s2);

    EXPECT_FLOAT_EQ(s1, 1.0f);
    EXPECT_FLOAT_EQ(s2, 1.0f);
}

TEST(Matrix22, GetScale_NonUniformXY_ReturnsCorrectValues)
{
    Matrix22 m(2.0f, 0.0f, 0.0f, 1.0f);
    float s1, s2;
    m.GetScale(s1, s2);

    EXPECT_FLOAT_EQ(s1, 2.0f);
    EXPECT_FLOAT_EQ(s2, 1.0f);
}

// ==============================================================================
// Matrix22 Rotation Tests - Counter Clockwise
// ==============================================================================

TEST(Matrix22, AsRotateCounterClockwise_AllAngles_CorrectRotation)
{
    Matrix22 matrix(Matrix22::Identity);

    for (int i = 0; i <= 360; i++)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << i);

        Angle testAngle = Angle::FromDegrees(static_cast<float>(i));
        Matrix22 resultMatrix = matrix.AsRotateCounterClockwise(testAngle);

        Angle resultAngle;
        resultMatrix.GetRotationCounterClockwise(resultAngle);

        EXPECT_EQ(resultAngle, testAngle);
    }
}

TEST(Matrix22, AsRotateCounterClockwise_Accumulate_CorrectRotation)
{
    Matrix22 matrix = Matrix22::Identity;

    for (int i = 1; i <= 360; i++)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << i);

        Angle testAngle = Angle::FromDegrees(static_cast<float>(i));
        matrix = matrix.AsRotateCounterClockwise(Angle::Deg1);

        Angle resultAngle;
        matrix.GetRotationCounterClockwise(resultAngle);

        EXPECT_EQ(resultAngle, testAngle);
    }
}

TEST(Matrix22, RotateCounterClockwise_AllAngles_CorrectRotation)
{
    Matrix22 matrixBase(Matrix22::Identity);

    for (int i = 0; i <= 360; i++)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << i);

        Angle testAngle = Angle::FromDegrees(static_cast<float>(i));
        Matrix22 matrix = matrixBase;
        matrix.RotateCounterClockwise(testAngle);

        Angle resultAngle;
        matrix.GetRotationCounterClockwise(resultAngle);

        EXPECT_EQ(resultAngle, testAngle);
    }
}

TEST(Matrix22, RotateCounterClockwise_Accumulate_CorrectRotation)
{
    Matrix22 matrix = Matrix22::Identity;

    for (int i = 1; i <= 360; i++)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << i);

        Angle testAngle = Angle::FromDegrees(static_cast<float>(i));
        matrix.RotateCounterClockwise(Angle::Deg1);

        Angle resultAngle;
        matrix.GetRotationCounterClockwise(resultAngle);

        EXPECT_EQ(resultAngle, testAngle);
    }
}

// ==============================================================================
// Matrix22 Rotation Tests - Clockwise
// ==============================================================================

TEST(Matrix22, AsRotateClockwise_AllAngles_CorrectRotation)
{
    Matrix22 matrix(Matrix22::Identity);

    for (int i = 0; i <= 360; i++)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << i);

        Angle testAngle = Angle::FromDegrees(static_cast<float>(i));
        Matrix22 resultMatrix = matrix.AsRotateClockwise(testAngle);

        Angle resultAngle;
        resultMatrix.GetRotationClockwise(resultAngle);

        EXPECT_EQ(resultAngle, testAngle);
    }
}

TEST(Matrix22, AsRotateClockwise_Accumulate_CorrectRotation)
{
    Matrix22 matrix = Matrix22::Identity;

    for (int i = 1; i <= 360; i++)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << i);

        Angle testAngle = Angle::FromDegrees(static_cast<float>(i));
        matrix = matrix.AsRotateClockwise(Angle::Deg1);

        Angle resultAngle;
        matrix.GetRotationClockwise(resultAngle);

        EXPECT_EQ(resultAngle, testAngle);
    }
}

TEST(Matrix22, RotateClockwise_AllAngles_CorrectRotation)
{
    Matrix22 matrixBase(Matrix22::Identity);

    for (int i = 0; i <= 360; i++)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << i);

        Angle testAngle = Angle::FromDegrees(static_cast<float>(i));
        Matrix22 matrix = matrixBase;
        matrix.RotateClockwise(testAngle);

        Angle resultAngle;
        matrix.GetRotationClockwise(resultAngle);

        EXPECT_EQ(resultAngle, testAngle);
    }
}

TEST(Matrix22, RotateClockwise_Accumulate_CorrectRotation)
{
    Matrix22 matrix = Matrix22::Identity;

    for (int i = 1; i <= 360; i++)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << i);

        Angle testAngle = Angle::FromDegrees(static_cast<float>(i));
        matrix.RotateClockwise(Angle::Deg1);

        Angle resultAngle;
        matrix.GetRotationClockwise(resultAngle);

        EXPECT_EQ(resultAngle, testAngle);
    }
}

// ==============================================================================
// Matrix22 Reflection Tests
// ==============================================================================

TEST(Matrix22, AsReflectArbitraryAxis_XAxis_ReflectsCorrectly)
{
    Matrix22 m(1.0f, 0.0f, 0.0f, 1.0f);
    Matrix22 m2 = m.AsReflectArbitraryAxis(Vector2D(1.0f, 0.0f));

    EXPECT_FLOAT_EQ(m2.Element(0), -1.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 0.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 0.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), 1.0f);
}

TEST(Matrix22, ReflectArbitraryAxis_XAxis_ReflectsInPlace)
{
    Matrix22 m(1.0f, 0.0f, 0.0f, 1.0f);
    m.ReflectArbitraryAxis(Vector2D(1.0f, 0.0f));

    EXPECT_FLOAT_EQ(m.Element(0), -1.0f);
    EXPECT_FLOAT_EQ(m.Element(1), 0.0f);
    EXPECT_FLOAT_EQ(m.Element(2), 0.0f);
    EXPECT_FLOAT_EQ(m.Element(3), 1.0f);
}

TEST(Matrix22, AsReflectArbitraryAxis_YAxis_ReflectsCorrectly)
{
    Matrix22 m(1.0f, 0.0f, 0.0f, 1.0f);
    Matrix22 m2 = m.AsReflectArbitraryAxis(Vector2D(0.0f, 1.0f));

    EXPECT_FLOAT_EQ(m2.Element(0), 1.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 0.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 0.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), -1.0f);
}

TEST(Matrix22, ReflectArbitraryAxis_YAxis_ReflectsInPlace)
{
    Matrix22 m(1.0f, 0.0f, 0.0f, 1.0f);
    m.ReflectArbitraryAxis(Vector2D(0.0f, 1.0f));

    EXPECT_FLOAT_EQ(m.Element(0), 1.0f);
    EXPECT_FLOAT_EQ(m.Element(1), 0.0f);
    EXPECT_FLOAT_EQ(m.Element(2), 0.0f);
    EXPECT_FLOAT_EQ(m.Element(3), -1.0f);
}

// ==============================================================================
// Matrix22 Projection Tests
// ==============================================================================

TEST(Matrix22, AsProjectArbitraryAxis_XAxis_ProjectsCorrectly)
{
    Matrix22 m(Matrix22::Identity);
    Matrix22 m2 = m.AsProjectArbitraryAxis(Vector2D(1.0f, 0.0f));

    EXPECT_FLOAT_EQ(m2.Element(0), 0.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 0.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 0.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), 1.0f);
}

TEST(Matrix22, ProjectArbitraryAxis_XAxis_ProjectsInPlace)
{
    Matrix22 m(Matrix22::Identity);
    m.ProjectArbitraryAxis(Vector2D(1.0f, 0.0f));

    EXPECT_FLOAT_EQ(m.Element(0), 0.0f);
    EXPECT_FLOAT_EQ(m.Element(1), 0.0f);
    EXPECT_FLOAT_EQ(m.Element(2), 0.0f);
    EXPECT_FLOAT_EQ(m.Element(3), 1.0f);
}

TEST(Matrix22, AsProjectArbitraryAxis_YAxis_ProjectsCorrectly)
{
    Matrix22 m(Matrix22::Identity);
    Matrix22 m2 = m.AsProjectArbitraryAxis(Vector2D(0.0f, 1.0f));

    EXPECT_FLOAT_EQ(m2.Element(0), 1.0f);
    EXPECT_FLOAT_EQ(m2.Element(1), 0.0f);
    EXPECT_FLOAT_EQ(m2.Element(2), 0.0f);
    EXPECT_FLOAT_EQ(m2.Element(3), 0.0f);
}

TEST(Matrix22, ProjectArbitraryAxis_YAxis_ProjectsInPlace)
{
    Matrix22 m(Matrix22::Identity);
    m.ProjectArbitraryAxis(Vector2D(0.0f, 1.0f));

    EXPECT_FLOAT_EQ(m.Element(0), 1.0f);
    EXPECT_FLOAT_EQ(m.Element(1), 0.0f);
    EXPECT_FLOAT_EQ(m.Element(2), 0.0f);
    EXPECT_FLOAT_EQ(m.Element(3), 0.0f);
}

// ==============================================================================
// Matrix22 Handedness Tests
// ==============================================================================

TEST(Matrix22, IsLeftHanded_LeftHandedMatrices_ReturnsTrue)
{
    Vector2D v1(1.0f, 0.0f), v3(0.0f, 1.0f);
    Vector2D v2 = Vector2D(1.0f, 1.0f).AsNormal();
    Vector2D v4 = Vector2D(-1.0f, 1.0f).AsNormal();
    Vector2D v5(-1.0f, 0.0f), v7(0.0f, -1.0f);
    Vector2D v6 = Vector2D(-1.0f, -1.0f).AsNormal();
    Vector2D v8 = Vector2D(1.0f, -1.0f).AsNormal();

    Matrix22 m1(v1, v3), m2(v2, v4), m3(v3, v5), m4(v4, v6);
    Matrix22 m5(v5, v7), m6(v6, v8), m7(v7, v1), m8(v8, v2);

    EXPECT_TRUE(m1.IsLeftHanded());
    EXPECT_TRUE(m2.IsLeftHanded());
    EXPECT_TRUE(m3.IsLeftHanded());
    EXPECT_TRUE(m4.IsLeftHanded());
    EXPECT_TRUE(m5.IsLeftHanded());
    EXPECT_TRUE(m6.IsLeftHanded());
    EXPECT_TRUE(m7.IsLeftHanded());
    EXPECT_TRUE(m8.IsLeftHanded());
}

TEST(Matrix22, IsRightHanded_LeftHandedMatrices_ReturnsFalse)
{
    Vector2D v1(1.0f, 0.0f), v3(0.0f, 1.0f);
    Vector2D v2 = Vector2D(1.0f, 1.0f).AsNormal();

    Matrix22 m(v1, v3);

    EXPECT_FALSE(m.IsRightHanded());
}

TEST(Matrix22, RightHanded_ConvertsToRightHanded)
{
    Vector2D v1(1.0f, 0.0f), v3(0.0f, 1.0f);
    Matrix22 m(v1, v3);

    m.RightHanded();

    EXPECT_TRUE(m.IsRightHanded());
    EXPECT_FALSE(m.IsLeftHanded());
}

TEST(Matrix22, LeftHanded_ConvertsToLeftHanded)
{
    Vector2D v1(1.0f, 0.0f), v3(0.0f, 1.0f);
    Matrix22 m(v1, v3);

    m.RightHanded();
    m.LeftHanded();

    EXPECT_TRUE(m.IsLeftHanded());
    EXPECT_FALSE(m.IsRightHanded());
}

TEST(Matrix22, AsRightHanded_ReturnsRightHandedMatrix)
{
    Vector2D v1(1.0f, 0.0f), v3(0.0f, 1.0f);
    Matrix22 m(v1, v3);

    Matrix22 m2 = m.AsRightHanded();

    EXPECT_TRUE(m2.IsRightHanded());
    EXPECT_FALSE(m2.IsLeftHanded());
}

TEST(Matrix22, AsLeftHanded_ReturnsLeftHandedMatrix)
{
    Vector2D v1(1.0f, 0.0f), v3(0.0f, 1.0f);
    Matrix22 m(v1, v3);

    m.RightHanded();
    Matrix22 m2 = m.AsLeftHanded();

    EXPECT_TRUE(m2.IsLeftHanded());
    EXPECT_FALSE(m2.IsRightHanded());
}

// ==============================================================================
// Matrix22 Axis Normal Tests
// ==============================================================================

TEST(Matrix22, IsAxisNormal_NormalizedAxes_ReturnsTrue)
{
    Vector2D v1(1.0f, 0.0f), v3(0.0f, 1.0f);
    Vector2D v2 = Vector2D(1.0f, 1.0f).AsNormal();
    Vector2D v4 = Vector2D(-1.0f, 1.0f).AsNormal();

    Matrix22 m1(v1, v3);
    Matrix22 m2(v2, v4);

    EXPECT_TRUE(m1.IsAxisNormal());
    EXPECT_TRUE(m2.IsAxisNormal());
}

TEST(Matrix22, IsAxisNormal_NonNormalizedAxes_ReturnsFalse)
{
    Vector2D v2(1.0f, 1.0f), v4(-1.0f, 1.0f);

    Matrix22 m(v2, v4);

    EXPECT_FALSE(m.IsAxisNormal());
}

TEST(Matrix22, NormalAxis_NormalizesAxes)
{
    Vector2D v2(1.0f, 1.0f), v4(-1.0f, 1.0f);
    Matrix22 m(v2, v4);

    m.NormalAxis();

    EXPECT_TRUE(m.IsAxisNormal());
}

TEST(Matrix22, AsNormalAxis_ReturnsNormalizedMatrix)
{
    Vector2D v2(1.0f, 1.0f), v4(-1.0f, 1.0f);
    Matrix22 m(v2, v4);

    Matrix22 m2 = m.AsNormalAxis();

    EXPECT_FALSE(m.IsAxisNormal());  // Original unchanged
    EXPECT_TRUE(m2.IsAxisNormal());
}

// ==============================================================================
// Matrix22 Orthogonalize Tests
// ==============================================================================

TEST(Matrix22, Orthogonalize_NonOrthogonal_BecomesOrthogonal)
{
    Vector2D v0(0.0f, 0.0f);
    Vector2D v1(1.0f, 0.0f);

    Matrix22 m(v1, v0);

    EXPECT_FALSE(m.IsOrthogonal());

    m.Orthogonalize();

    EXPECT_TRUE(m.IsOrthogonal());
}

TEST(Matrix22, AsOrthogonal_ReturnsOrthogonalMatrix)
{
    Vector2D v0(0.0f, 0.0f);
    Vector2D v1(1.0f, 0.0f);

    Matrix22 m(v1, v0);

    Matrix22 m2 = m.AsOrthogonal();

    EXPECT_FALSE(m.IsOrthogonal());  // Original unchanged
    EXPECT_TRUE(m2.IsOrthogonal());
}

// ==============================================================================
// Matrix22 LookAt Tests
// ==============================================================================

TEST(Matrix22, LookAtRotation_EightDirections_CorrectRotation)
{
    Vector2D v1(1.0f, 0.0f), v3(0.0f, 1.0f);
    Vector2D v2 = Vector2D(1.0f, 1.0f).AsNormal();
    Vector2D v4 = Vector2D(-1.0f, 1.0f).AsNormal();
    Vector2D v5(-1.0f, 0.0f), v7(0.0f, -1.0f);
    Vector2D v6 = Vector2D(-1.0f, -1.0f).AsNormal();
    Vector2D v8 = Vector2D(1.0f, -1.0f).AsNormal();

    Matrix22 expected1(v1, v3), expected2(v2, v4), expected3(v3, v5), expected4(v4, v6);
    Matrix22 expected5(v5, v7), expected6(v6, v8), expected7(v7, v1), expected8(v8, v2);

    Vector2D lookFrom(1.0f, 1.0f);
    Vector2D lookAt1(3.0f, 1.0f), lookAt2(3.0f, 3.0f), lookAt3(1.0f, 3.0f), lookAt4(-1.0f, 3.0f);
    Vector2D lookAt5(-1.0f, 1.0f), lookAt6(-1.0f, -1.0f), lookAt7(1.0f, -1.0f), lookAt8(3.0f, -1.0f);

    Matrix22 m1, m2, m3, m4, m5, m6, m7, m8;

    m1.LookAtRotation(lookFrom, lookAt1);
    m2.LookAtRotation(lookFrom, lookAt2);
    m3.LookAtRotation(lookFrom, lookAt3);
    m4.LookAtRotation(lookFrom, lookAt4);
    m5.LookAtRotation(lookFrom, lookAt5);
    m6.LookAtRotation(lookFrom, lookAt6);
    m7.LookAtRotation(lookFrom, lookAt7);
    m8.LookAtRotation(lookFrom, lookAt8);

    EXPECT_EQ(expected1, m1);
    EXPECT_EQ(expected2, m2);
    EXPECT_EQ(expected3, m3);
    EXPECT_EQ(expected4, m4);
    EXPECT_EQ(expected5, m5);
    EXPECT_EQ(expected6, m6);
    EXPECT_EQ(expected7, m7);
    EXPECT_EQ(expected8, m8);
}
