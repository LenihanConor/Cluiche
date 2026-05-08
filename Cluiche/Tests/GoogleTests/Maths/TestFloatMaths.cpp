// TestFloatMaths.cpp - Google Test unit tests for FloatMaths
//
// Tests FloatMaths utility functions from DiaMaths

#include <gtest/gtest.h>
#include <DiaMaths/Core/FloatMaths.h>

using namespace Dia::Maths;
using namespace Dia::Maths::Float;

// ==============================================================================
// FPower Tests - Float Exponent
// ==============================================================================

TEST(FloatMaths, FPower_FloatExponent_TwoToThree_ReturnsEight)
{
    float result = FPower(2.0f, 3.0f);

    EXPECT_FLOAT_EQ(result, 8.0f);
}

TEST(FloatMaths, FPower_FloatExponent_FloatBase_ReturnsCorrectPower)
{
    float result = FPower(2.5f, 3.0f);

    EXPECT_FLOAT_EQ(result, 15.625f);
}

TEST(FloatMaths, FPower_FloatExponent_ExponentOne_ReturnsBase)
{
    float result = FPower(2.0f, 1.0f);

    EXPECT_FLOAT_EQ(result, 2.0f);
}

TEST(FloatMaths, FPower_FloatExponent_ExponentZero_ReturnsOne)
{
    float result = FPower(2.0f, 0.0f);

    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(FloatMaths, FPower_FloatExponent_BaseZero_ReturnsZero)
{
    float result = FPower(0.0f, 2.0f);

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(FloatMaths, FPower_FloatExponent_NegativeBaseEvenExponent_ReturnsPositive)
{
    float result = FPower(-1.0f, 2.0f);

    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(FloatMaths, FPower_FloatExponent_NegativeBaseOddExponent_ReturnsNegative)
{
    float result = FPower(-1.0f, 3.0f);

    EXPECT_FLOAT_EQ(result, -1.0f);
}

// ==============================================================================
// FPower Tests - Integer Exponent
// ==============================================================================

TEST(FloatMaths, FPower_IntExponent_TwoToThree_ReturnsEight)
{
    float result = FPower(2.0f, 3);

    EXPECT_FLOAT_EQ(result, 8.0f);
}

TEST(FloatMaths, FPower_IntExponent_FloatBase_ReturnsCorrectPower)
{
    float result = FPower(2.5f, 3);

    EXPECT_FLOAT_EQ(result, 15.625f);
}

TEST(FloatMaths, FPower_IntExponent_ExponentOne_ReturnsBase)
{
    float result = FPower(2.0f, 1);

    EXPECT_FLOAT_EQ(result, 2.0f);
}

TEST(FloatMaths, FPower_IntExponent_ExponentZero_ReturnsOne)
{
    float result = FPower(2.0f, 0);

    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(FloatMaths, FPower_IntExponent_BaseZero_ReturnsZero)
{
    float result = FPower(0.0f, 2);

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(FloatMaths, FPower_IntExponent_NegativeBaseEvenExponent_ReturnsPositive)
{
    float result = FPower(-1.0f, 2);

    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(FloatMaths, FPower_IntExponent_NegativeBaseOddExponent_ReturnsNegative)
{
    float result = FPower(-1.0f, 3);

    EXPECT_FLOAT_EQ(result, -1.0f);
}

// ==============================================================================
// FSquare Tests
// ==============================================================================

TEST(FloatMaths, FSquare_Two_ReturnsFour)
{
    float result = FSquare(2.0f);

    EXPECT_FLOAT_EQ(result, 4.0f);
}

TEST(FloatMaths, FSquare_FloatValue_ReturnsSquare)
{
    float result = FSquare(2.5f);

    EXPECT_FLOAT_EQ(result, 6.25f);
}

TEST(FloatMaths, FSquare_Zero_ReturnsZero)
{
    float result = FSquare(0.0f);

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(FloatMaths, FSquare_NegativeOne_ReturnsOne)
{
    float result = FSquare(-1.0f);

    EXPECT_FLOAT_EQ(result, 1.0f);
}

// ==============================================================================
// FSquareRoot Tests
// ==============================================================================

TEST(FloatMaths, FSquareRoot_Nine_ReturnsThree)
{
    float result = FSquareRoot(9.0f);

    EXPECT_FLOAT_EQ(result, 3.0f);
}

TEST(FloatMaths, FSquareRoot_NonPerfectSquare_ReturnsApproximation)
{
    float result = FSquareRoot(5.0f);

    EXPECT_TRUE(FEqual(result, 2.2360679f));
}

TEST(FloatMaths, FSquareRoot_Zero_ReturnsZero)
{
    float result = FSquareRoot(0.0f);

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(FloatMaths, FSquareRoot_NegativeValue_Asserts)
{
    EXPECT_DEATH(Dia::Maths::SquareRoot(-1.0f), ".*");
}

// ==============================================================================
// FLog Tests
// ==============================================================================

TEST(FloatMaths, FLog_Two_ReturnsNaturalLog)
{
    float result = FLog(2.0f);

    EXPECT_TRUE(FEqual(result, 0.6931471805f));
}

TEST(FloatMaths, FLog_Zero_Asserts)
{
    EXPECT_DEATH(FLog(0.0f), ".*");
}

TEST(FloatMaths, FLog_NegativeValue_Asserts)
{
    EXPECT_DEATH(FLog(-1.0f), ".*");
}

// ==============================================================================
// FLog10 Tests
// ==============================================================================

TEST(FloatMaths, FLog10_Two_ReturnsLog10)
{
    float result = FLog10(2.0f);

    EXPECT_TRUE(FEqual(result, 0.3010299f));
}

TEST(FloatMaths, FLog10_Zero_Asserts)
{
    EXPECT_DEATH(FLog10(0.0f), ".*");
}

TEST(FloatMaths, FLog10_NegativeValue_Asserts)
{
    EXPECT_DEATH(FLog10(-1.0f), ".*");
}

// ==============================================================================
// FLogBase Tests
// ==============================================================================

TEST(FloatMaths, FLogBase_ThreeBaseTwo_ReturnsLogBaseTwo)
{
    float result = FLogBase(3.0f, 2.0f);

    EXPECT_TRUE(FEqual(result, 1.5849625f));
}

TEST(FloatMaths, FLogBase_ZeroValue_Asserts)
{
    EXPECT_DEATH(FLogBase(0.0f, 2.0f), ".*");
}

TEST(FloatMaths, FLogBase_NegativeValue_Asserts)
{
    EXPECT_DEATH(FLogBase(-1.0f, 2.0f), ".*");
}

TEST(FloatMaths, FLogBase_ZeroBase_Asserts)
{
    EXPECT_DEATH(FLogBase(2.0f, 0.0f), ".*");
}

TEST(FloatMaths, FLogBase_NegativeBase_Asserts)
{
    EXPECT_DEATH(FLogBase(2.0f, -1.0f), ".*");
}

// ==============================================================================
// FExp Tests
// ==============================================================================

TEST(FloatMaths, FExp_NegativeOne_ReturnsExp)
{
    float result = FExp(-1.0f);

    EXPECT_TRUE(FEqual(result, 0.367879441f));
}

TEST(FloatMaths, FExp_Zero_ReturnsOne)
{
    float result = FExp(0.0f);

    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(FloatMaths, FExp_One_ReturnsE)
{
    float result = FExp(1.0f);

    EXPECT_TRUE(FEqual(result, 2.71828183f));
}

// ==============================================================================
// FAbs Tests
// ==============================================================================

TEST(FloatMaths, FAbs_Zero_ReturnsZero)
{
    float result = FAbs(0.0f);

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(FloatMaths, FAbs_NegativeValue_ReturnsPositive)
{
    float result = FAbs(-11.0f);

    EXPECT_FLOAT_EQ(result, 11.0f);
}

TEST(FloatMaths, FAbs_PositiveValue_ReturnsPositive)
{
    float result = FAbs(11.0f);

    EXPECT_FLOAT_EQ(result, 11.0f);
}

// ==============================================================================
// FNegative Tests
// ==============================================================================

TEST(FloatMaths, FNegative_Zero_ReturnsZero)
{
    float result = FNegative(0.0f);

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(FloatMaths, FNegative_NegativeValue_RemainsNegative)
{
    float result = FNegative(-11.0f);

    EXPECT_FLOAT_EQ(result, -11.0f);
}

TEST(FloatMaths, FNegative_PositiveValue_BecomesNegative)
{
    float result = FNegative(11.0f);

    EXPECT_FLOAT_EQ(result, -11.0f);
}

// ==============================================================================
// FFloor Tests
// ==============================================================================

TEST(FloatMaths, FFloor_Zero_ReturnsZero)
{
    float result = FFloor(0.0f);

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(FloatMaths, FFloor_PositiveSmallFraction_ReturnsFloor)
{
    float result = FFloor(1.1f);

    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(FloatMaths, FFloor_PositiveLargeFraction_ReturnsFloor)
{
    float result = FFloor(1.9f);

    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(FloatMaths, FFloor_NegativeSmallFraction_ReturnsFloor)
{
    float result = FFloor(-1.1f);

    EXPECT_FLOAT_EQ(result, -2.0f);
}

TEST(FloatMaths, FFloor_NegativeLargeFraction_ReturnsFloor)
{
    float result = FFloor(-1.9f);

    EXPECT_FLOAT_EQ(result, -2.0f);
}

// ==============================================================================
// FCeiling Tests
// ==============================================================================

TEST(FloatMaths, FCeiling_Zero_ReturnsZero)
{
    float result = FCeiling(0.0f);

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(FloatMaths, FCeiling_PositiveSmallFraction_ReturnsCeiling)
{
    float result = FCeiling(1.1f);

    EXPECT_FLOAT_EQ(result, 2.0f);
}

TEST(FloatMaths, FCeiling_PositiveLargeFraction_ReturnsCeiling)
{
    float result = FCeiling(1.9f);

    EXPECT_FLOAT_EQ(result, 2.0f);
}

TEST(FloatMaths, FCeiling_NegativeSmallFraction_ReturnsCeiling)
{
    float result = FCeiling(-1.1f);

    EXPECT_FLOAT_EQ(result, -1.0f);
}

TEST(FloatMaths, FCeiling_NegativeLargeFraction_ReturnsCeiling)
{
    float result = FCeiling(-1.9f);

    EXPECT_FLOAT_EQ(result, -1.0f);
}

// ==============================================================================
// FRound Tests
// ==============================================================================

TEST(FloatMaths, FRound_Zero_ReturnsZero)
{
    float result = FRound(0.0f);

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(FloatMaths, FRound_PositiveSmallFraction_RoundsDown)
{
    float result = FRound(1.1f);

    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(FloatMaths, FRound_PositiveLargeFraction_RoundsUp)
{
    float result = FRound(1.9f);

    EXPECT_FLOAT_EQ(result, 2.0f);
}

TEST(FloatMaths, FRound_NegativeSmallFraction_RoundsDown)
{
    float result = FRound(-1.1f);

    EXPECT_FLOAT_EQ(result, -1.0f);
}

TEST(FloatMaths, FRound_NegativeLargeFraction_RoundsDown)
{
    float result = FRound(-1.9f);

    EXPECT_FLOAT_EQ(result, -2.0f);
}

// ==============================================================================
// FTruncate Tests
// ==============================================================================

TEST(FloatMaths, FTruncate_OneDecimalPlace_Truncates)
{
    float result = FTruncate(1.23456789f, 1);

    EXPECT_FLOAT_EQ(result, 1.2f);
}

TEST(FloatMaths, FTruncate_ThreeDecimalPlaces_Truncates)
{
    float result = FTruncate(1.23456789f, 3);

    EXPECT_FLOAT_EQ(result, 1.234f);
}

TEST(FloatMaths, FTruncate_NegativeValue_Truncates)
{
    float result = FTruncate(-1.23456789f, 1);

    EXPECT_FLOAT_EQ(result, -1.2f);
}

TEST(FloatMaths, FTruncate_ZeroDecimalPlaces_ReturnsInteger)
{
    float result = FTruncate(1.23456789f, 0);

    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(FloatMaths, FTruncate_NegativeDecimalPlaces_Asserts)
{
    EXPECT_DEATH(FTruncate(1.23456789f, -1), ".*");
}

// ==============================================================================
// FEqual Tests
// ==============================================================================

TEST(FloatMaths, FEqual_SameValue_ReturnsTrue)
{
    EXPECT_TRUE(FEqual(1.0f, 1.0f));
}

TEST(FloatMaths, FEqual_DifferentValues_ReturnsFalse)
{
    EXPECT_FALSE(FEqual(1.0f, 1.2f));
}

TEST(FloatMaths, FEqual_SameNegativeValue_ReturnsTrue)
{
    EXPECT_TRUE(FEqual(-1.0f, -1.0f));
}

TEST(FloatMaths, FEqual_DifferentNegativeValues_ReturnsFalse)
{
    EXPECT_FALSE(FEqual(-1.0f, -1.2f));
}

// ==============================================================================
// FLess Tests
// ==============================================================================

TEST(FloatMaths, FLess_FirstSmaller_ReturnsTrue)
{
    EXPECT_TRUE(FLess(1.0f, 1.2f));
}

TEST(FloatMaths, FLess_FirstLarger_ReturnsFalse)
{
    EXPECT_FALSE(FLess(1.2f, 1.0f));
}

TEST(FloatMaths, FLess_NegativeFirstSmaller_ReturnsTrue)
{
    EXPECT_TRUE(FLess(-1.2f, -1.0f));
}

TEST(FloatMaths, FLess_NegativeFirstLarger_ReturnsFalse)
{
    EXPECT_FALSE(FLess(-1.0f, -1.2f));
}

// ==============================================================================
// FGreater Tests
// ==============================================================================

TEST(FloatMaths, FGreater_FirstLarger_ReturnsTrue)
{
    EXPECT_TRUE(FGreater(1.2f, 1.0f));
}

TEST(FloatMaths, FGreater_FirstSmaller_ReturnsFalse)
{
    EXPECT_FALSE(FGreater(1.0f, 1.2f));
}

TEST(FloatMaths, FGreater_NegativeFirstLarger_ReturnsTrue)
{
    EXPECT_TRUE(FGreater(-1.0f, -1.2f));
}

TEST(FloatMaths, FGreater_NegativeFirstSmaller_ReturnsFalse)
{
    EXPECT_FALSE(FGreater(-1.2f, -1.0f));
}

// ==============================================================================
// FLessEqual Tests
// ==============================================================================

TEST(FloatMaths, FLessEqual_FirstSmaller_ReturnsTrue)
{
    EXPECT_TRUE(FLessEqual(1.0f, 1.2f));
}

TEST(FloatMaths, FLessEqual_Equal_ReturnsTrue)
{
    EXPECT_TRUE(FLessEqual(1.0f, 1.0f));
}

TEST(FloatMaths, FLessEqual_FirstLarger_ReturnsFalse)
{
    EXPECT_FALSE(FLessEqual(1.2f, 1.0f));
}

TEST(FloatMaths, FLessEqual_NegativeFirstSmaller_ReturnsTrue)
{
    EXPECT_TRUE(FLessEqual(-1.2f, -1.0f));
}

TEST(FloatMaths, FLessEqual_NegativeEqual_ReturnsTrue)
{
    EXPECT_TRUE(FLessEqual(-1.0f, -1.0f));
}

TEST(FloatMaths, FLessEqual_NegativeFirstLarger_ReturnsFalse)
{
    EXPECT_FALSE(FLessEqual(-1.0f, -1.2f));
}

// ==============================================================================
// FGreaterEqual Tests
// ==============================================================================

TEST(FloatMaths, FGreaterEqual_FirstLarger_ReturnsTrue)
{
    EXPECT_TRUE(FGreaterEqual(1.2f, 1.0f));
}

TEST(FloatMaths, FGreaterEqual_Equal_ReturnsTrue)
{
    EXPECT_TRUE(FGreaterEqual(1.0f, 1.0f));
}

TEST(FloatMaths, FGreaterEqual_FirstSmaller_ReturnsFalse)
{
    EXPECT_FALSE(FGreaterEqual(1.0f, 1.2f));
}

TEST(FloatMaths, FGreaterEqual_NegativeFirstLarger_ReturnsTrue)
{
    EXPECT_TRUE(FGreaterEqual(-1.0f, -1.2f));
}

TEST(FloatMaths, FGreaterEqual_NegativeEqual_ReturnsTrue)
{
    EXPECT_TRUE(FGreaterEqual(-1.0f, -1.0f));
}

TEST(FloatMaths, FGreaterEqual_NegativeFirstSmaller_ReturnsFalse)
{
    EXPECT_FALSE(FGreaterEqual(-1.2f, -1.0f));
}

// ==============================================================================
// FEqualRelative Tests
// ==============================================================================

TEST(FloatMaths, FEqualRelative_SameValue_ReturnsTrue)
{
    EXPECT_TRUE(FEqualRelative(1.0f, 1.0f));
}

TEST(FloatMaths, FEqualRelative_DifferentValues_ReturnsFalse)
{
    EXPECT_FALSE(FEqualRelative(1.0f, 1.2f));
}

TEST(FloatMaths, FEqualRelative_SameNegativeValue_ReturnsTrue)
{
    EXPECT_TRUE(FEqualRelative(-1.0f, -1.0f));
}

TEST(FloatMaths, FEqualRelative_DifferentNegativeValues_ReturnsFalse)
{
    EXPECT_FALSE(FEqualRelative(-1.0f, -1.2f));
}

// ==============================================================================
// FLessRelative Tests
// ==============================================================================

TEST(FloatMaths, FLessRelative_FirstSmaller_ReturnsTrue)
{
    EXPECT_TRUE(FLessRelative(1.0f, 1.2f));
}

TEST(FloatMaths, FLessRelative_FirstLarger_ReturnsFalse)
{
    EXPECT_FALSE(FLessRelative(1.2f, 1.0f));
}

TEST(FloatMaths, FLessRelative_NegativeFirstSmaller_ReturnsTrue)
{
    EXPECT_TRUE(FLessRelative(-1.2f, -1.0f));
}

TEST(FloatMaths, FLessRelative_NegativeFirstLarger_ReturnsFalse)
{
    EXPECT_FALSE(FLessRelative(-1.0f, -1.2f));
}

// ==============================================================================
// FGreaterRelative Tests
// ==============================================================================

TEST(FloatMaths, FGreaterRelative_FirstLarger_ReturnsTrue)
{
    EXPECT_TRUE(FGreaterRelative(1.2f, 1.0f));
}

TEST(FloatMaths, FGreaterRelative_FirstSmaller_ReturnsFalse)
{
    EXPECT_FALSE(FGreaterRelative(1.0f, 1.2f));
}

TEST(FloatMaths, FGreaterRelative_NegativeFirstLarger_ReturnsTrue)
{
    EXPECT_TRUE(FGreaterRelative(-1.0f, -1.2f));
}

TEST(FloatMaths, FGreaterRelative_NegativeFirstSmaller_ReturnsFalse)
{
    EXPECT_FALSE(FGreaterRelative(-1.2f, -1.0f));
}

// ==============================================================================
// FLessEqualRelative Tests
// ==============================================================================

TEST(FloatMaths, FLessEqualRelative_FirstSmaller_ReturnsTrue)
{
    EXPECT_TRUE(FLessEqualRelative(1.0f, 1.2f));
}

TEST(FloatMaths, FLessEqualRelative_Equal_ReturnsTrue)
{
    EXPECT_TRUE(FLessEqualRelative(1.0f, 1.0f));
}

TEST(FloatMaths, FLessEqualRelative_FirstLarger_ReturnsFalse)
{
    EXPECT_FALSE(FLessEqualRelative(1.2f, 1.0f));
}

TEST(FloatMaths, FLessEqualRelative_NegativeFirstSmaller_ReturnsTrue)
{
    EXPECT_TRUE(FLessEqualRelative(-1.2f, -1.0f));
}

TEST(FloatMaths, FLessEqualRelative_NegativeEqual_ReturnsTrue)
{
    EXPECT_TRUE(FLessEqualRelative(-1.0f, -1.0f));
}

TEST(FloatMaths, FLessEqualRelative_NegativeFirstLarger_ReturnsFalse)
{
    EXPECT_FALSE(FLessEqualRelative(-1.0f, -1.2f));
}

// ==============================================================================
// FGreaterEqualRelative Tests
// ==============================================================================

TEST(FloatMaths, FGreaterEqualRelative_FirstLarger_ReturnsTrue)
{
    EXPECT_TRUE(FGreaterEqualRelative(1.2f, 1.0f));
}

TEST(FloatMaths, FGreaterEqualRelative_Equal_ReturnsTrue)
{
    EXPECT_TRUE(FGreaterEqualRelative(1.0f, 1.0f));
}

TEST(FloatMaths, FGreaterEqualRelative_FirstSmaller_ReturnsFalse)
{
    EXPECT_FALSE(FGreaterEqualRelative(1.0f, 1.2f));
}

TEST(FloatMaths, FGreaterEqualRelative_NegativeFirstLarger_ReturnsTrue)
{
    EXPECT_TRUE(FGreaterEqualRelative(-1.0f, -1.2f));
}

TEST(FloatMaths, FGreaterEqualRelative_NegativeEqual_ReturnsTrue)
{
    EXPECT_TRUE(FGreaterEqualRelative(-1.0f, -1.0f));
}

TEST(FloatMaths, FGreaterEqualRelative_NegativeFirstSmaller_ReturnsFalse)
{
    EXPECT_FALSE(FGreaterEqualRelative(-1.2f, -1.0f));
}

// ==============================================================================
// FInRange Tests
// ==============================================================================

TEST(FloatMaths, FInRange_ValueInRange_ReturnsTrue)
{
    EXPECT_TRUE(FInRange(1.0f, 1.5f, 1.25f));
}

TEST(FloatMaths, FInRange_ValueOutOfRange_ReturnsFalse)
{
    EXPECT_FALSE(FInRange(1.0f, 1.2f, 1.5f));
}

// ==============================================================================
// FInRangeRelative Tests
// ==============================================================================

TEST(FloatMaths, FInRangeRelative_ValueInRange_ReturnsTrue)
{
    EXPECT_TRUE(FInRangeRelative(1.0f, 1.5f, 1.25f));
}

TEST(FloatMaths, FInRangeRelative_ValueOutOfRange_ReturnsFalse)
{
    EXPECT_FALSE(FInRangeRelative(1.0f, 1.2f, 1.5f));
}

// ==============================================================================
// FMod Tests
// ==============================================================================

TEST(FloatMaths, FMod_PositiveValues_ReturnsRemainder)
{
    float result = FMod(10.0f, 4.0f);

    EXPECT_FLOAT_EQ(result, 2.0f);
}

TEST(FloatMaths, FMod_NegativeValue_ReturnsNegativeRemainder)
{
    float result = FMod(-10.0f, 4.0f);

    EXPECT_FLOAT_EQ(result, -2.0f);
}

TEST(FloatMaths, FMod_NegativeDivisor_ReturnsPositiveRemainder)
{
    float result = FMod(10.0f, -4.0f);

    EXPECT_FLOAT_EQ(result, 2.0f);
}

TEST(FloatMaths, FMod_BothNegative_ReturnsNegativeRemainder)
{
    float result = FMod(-10.0f, -4.0f);

    EXPECT_FLOAT_EQ(result, -2.0f);
}

// ==============================================================================
// FSelect Tests
// ==============================================================================

TEST(FloatMaths, FSelect_PositiveComparator_ReturnsFirst)
{
    float result = FSelect(10.0f, 1.0f, 2.0f);

    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(FloatMaths, FSelect_NegativeComparator_ReturnsSecond)
{
    float result = FSelect(-10.0f, 1.0f, 2.0f);

    EXPECT_FLOAT_EQ(result, 2.0f);
}

TEST(FloatMaths, FSelect_ZeroComparator_ReturnsFirst)
{
    float result = FSelect(0.0f, 1.0f, 2.0f);

    EXPECT_FLOAT_EQ(result, 1.0f);
}
