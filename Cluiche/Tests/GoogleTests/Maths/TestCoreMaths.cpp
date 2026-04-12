// TestCoreMaths.cpp - Google Test unit tests for CoreMaths
//
// Tests CoreMaths utility functions from DiaMaths

#include <gtest/gtest.h>
#include <DiaMaths/Core/CoreMaths.h>
#include <DiaMaths/Core/FloatMaths.h>

using namespace Dia::Maths;

// ==============================================================================
// Clamp Tests
// ==============================================================================

TEST(CoreMaths, Clamp_WithinRange_ReturnsValue)
{
    int result = Clamp(3, 1, 5);

    EXPECT_EQ(result, 3);
}

TEST(CoreMaths, Clamp_BelowMin_ReturnsMin)
{
    int result = Clamp(0, 1, 5);

    EXPECT_EQ(result, 1);
}

TEST(CoreMaths, Clamp_AboveMax_ReturnsMax)
{
    int result = Clamp(6, 1, 5);

    EXPECT_EQ(result, 5);
}

// ==============================================================================
// Abs Tests
// ==============================================================================

TEST(CoreMaths, Abs_Zero_ReturnsZero)
{
    int result = Abs(0);

    EXPECT_EQ(result, 0);
}

TEST(CoreMaths, Abs_NegativeValue_ReturnsPositive)
{
    int result = Abs(-11);

    EXPECT_EQ(result, 11);
}

TEST(CoreMaths, Abs_PositiveValue_ReturnsPositive)
{
    int result = Abs(11);

    EXPECT_EQ(result, 11);
}

// ==============================================================================
// Negative Tests
// ==============================================================================

TEST(CoreMaths, Negative_Zero_ReturnsZero)
{
    int result = Negative(0);

    EXPECT_EQ(result, 0);
}

TEST(CoreMaths, Negative_NegativeValue_RemainsNegative)
{
    int result = Negative(-11);

    EXPECT_EQ(result, -11);
}

TEST(CoreMaths, Negative_PositiveValue_BecomesNegative)
{
    int result = Negative(11);

    EXPECT_EQ(result, -11);
}

// ==============================================================================
// Min Tests
// ==============================================================================

TEST(CoreMaths, Min_FirstSmaller_ReturnsFirst)
{
    int result = Min(1, 2);

    EXPECT_EQ(result, 1);
}

TEST(CoreMaths, Min_SecondSmaller_ReturnsSecond)
{
    int result = Min(2, 1);

    EXPECT_EQ(result, 1);
}

TEST(CoreMaths, Min_EqualValues_ReturnsValue)
{
    int result = Min(1, 1);

    EXPECT_EQ(result, 1);
}

// ==============================================================================
// Max Tests
// ==============================================================================

TEST(CoreMaths, Max_FirstLarger_ReturnsFirst)
{
    int result = Max(2, 1);

    EXPECT_EQ(result, 2);
}

TEST(CoreMaths, Max_SecondLarger_ReturnsSecond)
{
    int result = Max(1, 2);

    EXPECT_EQ(result, 2);
}

TEST(CoreMaths, Max_EqualValues_ReturnsValue)
{
    int result = Max(1, 1);

    EXPECT_EQ(result, 1);
}

// ==============================================================================
// Square Tests
// ==============================================================================

TEST(CoreMaths, Square_Integer_ReturnsSquare)
{
    int result = Square(2);

    EXPECT_EQ(result, 4);
}

TEST(CoreMaths, Square_Float_ReturnsSquare)
{
    float result = Square(2.5f);

    EXPECT_FLOAT_EQ(result, 6.25f);
}

TEST(CoreMaths, Square_Zero_ReturnsZero)
{
    int result = Square(0);

    EXPECT_EQ(result, 0);
}

TEST(CoreMaths, Square_NegativeOne_ReturnsOne)
{
    int result = Square(-1);

    EXPECT_EQ(result, 1);
}

// ==============================================================================
// Power Tests
// ==============================================================================

TEST(CoreMaths, Power_TwoToThree_ReturnsEight)
{
    int result = Power(2, 3);

    EXPECT_EQ(result, 8);
}

TEST(CoreMaths, Power_FloatBase_ReturnsCorrectPower)
{
    float result = Power(2.5f, 3);

    EXPECT_FLOAT_EQ(result, 15.625f);
}

TEST(CoreMaths, Power_ExponentOne_ReturnsBase)
{
    int result = Power(2, 1);

    EXPECT_EQ(result, 2);
}

TEST(CoreMaths, Power_ExponentZero_ReturnsOne)
{
    int result = Power(2, 0);

    EXPECT_EQ(result, 1);
}

TEST(CoreMaths, Power_BaseZero_ReturnsZero)
{
    int result = Power(0, 2);

    EXPECT_EQ(result, 0);
}

TEST(CoreMaths, Power_NegativeBaseEvenExponent_ReturnsPositive)
{
    int result = Power(-1, 2);

    EXPECT_EQ(result, 1);
}

TEST(CoreMaths, Power_NegativeBaseOddExponent_ReturnsNegative)
{
    int result = Power(-1, 3);

    EXPECT_EQ(result, -1);
}

// ==============================================================================
// Swap Tests
// ==============================================================================

TEST(CoreMaths, Swap_ExchangesValues)
{
    int a = 2;
    int b = 3;

    Swap(a, b);

    EXPECT_EQ(a, 3);
    EXPECT_EQ(b, 2);
}

// ==============================================================================
// SquareRoot Tests
// ==============================================================================

TEST(CoreMaths, SquareRoot_Nine_ReturnsThree)
{
    float result = SquareRoot(9.0f);

    EXPECT_FLOAT_EQ(result, 3.0f);
}

TEST(CoreMaths, SquareRoot_NonPerfectSquare_ReturnsApproximation)
{
    float result = SquareRoot(5.0f);

    EXPECT_TRUE(Float::FEqual(result, 2.2360679f));
}

TEST(CoreMaths, SquareRoot_Zero_ReturnsZero)
{
    float result = SquareRoot(0.0f);

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(CoreMaths, SquareRoot_NegativeValue_Asserts)
{
    EXPECT_DEATH(SquareRoot(-1.0f), ".*");
}

// ==============================================================================
// Factorial Tests
// ==============================================================================

TEST(CoreMaths, Factorial_Four_ReturnsTwentyFour)
{
    int result = Factorial(4);

    EXPECT_EQ(result, 24);
}

TEST(CoreMaths, Factorial_One_ReturnsOne)
{
    int result = Factorial(1);

    EXPECT_EQ(result, 1);
}

TEST(CoreMaths, Factorial_Zero_ReturnsOne)
{
    int result = Factorial(0);

    EXPECT_EQ(result, 1);
}

TEST(CoreMaths, Factorial_NegativeValue_Asserts)
{
    EXPECT_DEATH(Factorial(-2), ".*");
}
