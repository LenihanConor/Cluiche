// TestAngle.cpp - Google Test unit tests for Angle
//
// Tests Angle from DiaMaths

#include <gtest/gtest.h>
#include <DiaMaths/Core/Angle.h>

using namespace Dia::Maths;

// ==============================================================================
// Angle Constants Tests
// ==============================================================================

TEST(Angle, Constants_MatchFromDegrees)
{
    EXPECT_EQ(Angle::Deg0, Angle::FromDegrees(0.0f));
    EXPECT_EQ(Angle::DegHalf, Angle::FromDegrees(0.5f));
    EXPECT_EQ(Angle::Deg1, Angle::FromDegrees(1.0f));
    EXPECT_EQ(Angle::Deg5, Angle::FromDegrees(5.0f));
    EXPECT_EQ(Angle::Deg10, Angle::FromDegrees(10.0f));
    EXPECT_EQ(Angle::Deg15, Angle::FromDegrees(15.0f));
    EXPECT_EQ(Angle::Deg30, Angle::FromDegrees(30.0f));
    EXPECT_EQ(Angle::Deg45, Angle::FromDegrees(45.0f));
    EXPECT_EQ(Angle::Deg60, Angle::FromDegrees(60.0f));
    EXPECT_EQ(Angle::Deg90, Angle::FromDegrees(90.0f));
    EXPECT_EQ(Angle::Deg120, Angle::FromDegrees(120.0f));
    EXPECT_EQ(Angle::Deg135, Angle::FromDegrees(135.0f));
    EXPECT_EQ(Angle::Deg180, Angle::FromDegrees(180.0f));
}

// ==============================================================================
// Angle Construction Tests
// ==============================================================================

TEST(Angle, DefaultConstruction_ZeroDegrees)
{
    Angle angle;

    EXPECT_EQ(Angle::Deg0, angle);
}

TEST(Angle, ConstructWithPI_Is180Degrees)
{
    Angle angle(PI);

    EXPECT_EQ(Angle::Deg180, angle);
}

TEST(Angle, CopyConstructor_CopiesValue)
{
    Angle angle1(PI);
    Angle angle2(angle1);

    EXPECT_EQ(Angle::Deg180, angle2);
}

TEST(Angle, AssignmentOperator_CopiesValue)
{
    Angle angle1(PI);
    Angle angle2;

    angle2 = angle1;

    EXPECT_EQ(Angle::Deg180, angle2);
}

// ==============================================================================
// Unary Minus Tests
// ==============================================================================

TEST(Angle, UnaryMinus_NegativeDeg90)
{
    Angle angle = -Angle::Deg90;

    EXPECT_FLOAT_EQ(angle.AsDegrees(), -90.0f);
}

TEST(Angle, UnaryMinus_NegativeDeg180)
{
    Angle angle = -Angle::Deg180;

    EXPECT_FLOAT_EQ(angle.AsDegrees(), -180.0f);
}

// ==============================================================================
// Subtraction Tests
// ==============================================================================

TEST(Angle, Subtraction_180Minus90_Equals90)
{
    Angle angle = Angle::Deg180 - Angle::Deg90;

    EXPECT_EQ(Angle::Deg90, angle);
}

TEST(Angle, Subtraction_0Minus90_EqualsNegative90)
{
    Angle angle = Angle::Deg0 - Angle::Deg90;

    EXPECT_EQ(-Angle::Deg90, angle);
}

TEST(Angle, Subtraction_Negative180Minus90_Wraps)
{
    Angle angle = -Angle::Deg180 - Angle::Deg90;

    // -180 - 90 = -270, which wraps to +90
    EXPECT_EQ(Angle::Deg90, angle);
}

TEST(Angle, Subtraction_360Minus90_WrapsToNegative90)
{
    Angle angle = Angle::Deg360 - Angle::Deg90;

    // 360 - 90 = 270, which wraps to -90
    EXPECT_EQ(-Angle::Deg90, angle);
}

// ==============================================================================
// Addition Tests
// ==============================================================================

TEST(Angle, Addition_90Plus90_Equals180)
{
    Angle angle = Angle::Deg90 + Angle::Deg90;

    EXPECT_EQ(Angle::Deg180, angle);
}

TEST(Angle, Addition_Negative180Plus90_EqualsNegative90)
{
    Angle angle = -Angle::Deg180 + Angle::Deg90;

    EXPECT_EQ(-Angle::Deg90, angle);
}

TEST(Angle, Addition_180Plus90_WrapsToNegative90)
{
    Angle angle = Angle::Deg180 + Angle::Deg90;

    // 180 + 90 = 270, which wraps to -90
    EXPECT_EQ(-Angle::Deg90, angle);
}

TEST(Angle, Addition_Negative360Plus90_Wraps)
{
    Angle angle = -Angle::Deg360 + Angle::Deg90;

    // -360 + 90 = -270, which wraps to +90
    EXPECT_EQ(Angle::Deg90, angle);
}

// ==============================================================================
// Multiplication Tests
// ==============================================================================

TEST(Angle, Multiplication_90Times2_Equals180)
{
    Angle angle = Angle::Deg90 * 2.0f;

    EXPECT_EQ(Angle::Deg180, angle);
}

TEST(Angle, Multiplication_90TimesHalf_Equals45)
{
    Angle angle = Angle::Deg90 * 0.5f;

    EXPECT_EQ(Angle::Deg45, angle);
}

TEST(Angle, Multiplication_90TimesNegative2_EqualsNegative180)
{
    Angle angle = Angle::Deg90 * -2.0f;

    EXPECT_EQ(-Angle::Deg180, angle);
}

TEST(Angle, Multiplication_90Times10_WrapsTo180)
{
    Angle angle = Angle::Deg90 * 10.0f;

    // 90 * 10 = 900, which wraps to 180
    EXPECT_EQ(Angle::Deg180, angle);
}

TEST(Angle, Multiplication_90TimesNegative10_WrapsToNegative180)
{
    Angle angle = Angle::Deg90 * -10.0f;

    // 90 * -10 = -900, which wraps to -180
    EXPECT_EQ(-Angle::Deg180, angle);
}

// ==============================================================================
// Division Tests
// ==============================================================================

TEST(Angle, Division_90Divide2_Equals45)
{
    Angle angle = Angle::Deg90 / 2.0f;

    EXPECT_EQ(Angle::Deg45, angle);
}

TEST(Angle, Division_90DivideHalf_Equals180)
{
    Angle angle = Angle::Deg90 / 0.5f;

    EXPECT_EQ(Angle::Deg180, angle);
}

TEST(Angle, Division_90DivideNegative2_EqualsNegative45)
{
    Angle angle = Angle::Deg90 / -2.0f;

    EXPECT_EQ(-Angle::Deg45, angle);
}

TEST(Angle, Division_90Divide10_Equals9)
{
    Angle angle = Angle::Deg90 / 10.0f;

    EXPECT_EQ(Angle::FromDegrees(9.0f), angle);
}

TEST(Angle, Division_90DivideNegative10_EqualsNegative9)
{
    Angle angle = Angle::Deg90 / -10.0f;

    EXPECT_EQ(-Angle::FromDegrees(9.0f), angle);
}

TEST(Angle, Division_90DividePointOne_WrapsTo180)
{
    Angle angle = Angle::Deg90 / 0.1f;

    // 90 / 0.1 = 900, which wraps to 180
    EXPECT_EQ(Angle::Deg180, angle);
}

TEST(Angle, Division_90DivideNegativePointOne_WrapsToNegative180)
{
    Angle angle = Angle::Deg90 / -0.1f;

    // 90 / -0.1 = -900, which wraps to -180
    EXPECT_EQ(-Angle::Deg180, angle);
}

// ==============================================================================
// Compound Assignment Tests - Addition
// ==============================================================================

TEST(Angle, PlusEquals_90PlusEquals90_Equals180)
{
    Angle angle = Angle::Deg90;
    angle += Angle::Deg90;

    EXPECT_EQ(Angle::Deg180, angle);
}

TEST(Angle, PlusEquals_Negative180PlusEquals90_EqualsNegative90)
{
    Angle angle = -Angle::Deg180;
    angle += Angle::Deg90;

    EXPECT_EQ(-Angle::Deg90, angle);
}

TEST(Angle, PlusEquals_180PlusEquals90_WrapsToNegative90)
{
    Angle angle = Angle::Deg180;
    angle += Angle::Deg90;

    EXPECT_EQ(-Angle::Deg90, angle);
}

TEST(Angle, PlusEquals_Negative360PlusEquals90_Wraps)
{
    Angle angle = -Angle::Deg360;
    angle += Angle::Deg90;

    EXPECT_EQ(Angle::Deg90, angle);
}

// ==============================================================================
// Compound Assignment Tests - Subtraction
// ==============================================================================

TEST(Angle, MinusEquals_180MinusEquals90_Equals90)
{
    Angle angle = Angle::Deg180;
    angle -= Angle::Deg90;

    EXPECT_EQ(Angle::Deg90, angle);
}

TEST(Angle, MinusEquals_0MinusEquals90_EqualsNegative90)
{
    Angle angle = Angle::Deg0;
    angle -= Angle::Deg90;

    EXPECT_EQ(-Angle::Deg90, angle);
}

TEST(Angle, MinusEquals_Negative180MinusEquals90_Wraps)
{
    Angle angle = -Angle::Deg180;
    angle -= Angle::Deg90;

    EXPECT_EQ(Angle::Deg90, angle);
}

TEST(Angle, MinusEquals_360MinusEquals90_WrapsToNegative90)
{
    Angle angle = Angle::Deg360;
    angle -= Angle::Deg90;

    EXPECT_EQ(-Angle::Deg90, angle);
}

// ==============================================================================
// Compound Assignment Tests - Multiplication
// ==============================================================================

TEST(Angle, MultiplyEquals_90TimesEquals2_Equals180)
{
    Angle angle = Angle::Deg90;
    angle *= 2.0f;

    EXPECT_EQ(Angle::Deg180, angle);
}

TEST(Angle, MultiplyEquals_90TimesEqualsHalf_Equals45)
{
    Angle angle = Angle::Deg90;
    angle *= 0.5f;

    EXPECT_EQ(Angle::Deg45, angle);
}

// ==============================================================================
// Compound Assignment Tests - Division
// ==============================================================================

TEST(Angle, DivideEquals_90DivideEquals2_Equals45)
{
    Angle angle = Angle::Deg90;
    angle /= 2.0f;

    EXPECT_EQ(Angle::Deg45, angle);
}

TEST(Angle, DivideEquals_90DivideEqualsHalf_Equals180)
{
    Angle angle = Angle::Deg90;
    angle /= 0.5f;

    EXPECT_EQ(Angle::Deg180, angle);
}

// ==============================================================================
// Comparison Tests
// ==============================================================================

TEST(Angle, Equality_SameAngle_ReturnsTrue)
{
    Angle angle1 = Angle::Deg90;
    Angle angle2 = Angle::Deg90;

    EXPECT_EQ(angle1, angle2);
}

TEST(Angle, Inequality_DifferentAngle_ReturnsTrue)
{
    Angle angle1 = Angle::Deg90;
    Angle angle2 = Angle::Deg180;

    EXPECT_NE(angle1, angle2);
}
