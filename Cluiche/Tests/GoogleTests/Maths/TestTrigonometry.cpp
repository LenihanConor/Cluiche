// TestTrigonometry.cpp - Google Test unit tests for Trigonometry
//
// Tests trigonometric functions from DiaMaths

#include <gtest/gtest.h>
#include <DiaMaths/Core/Trigonometry.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Core/FloatMaths.h>
#include <cmath>

using namespace Dia::Maths;

// ==============================================================================
// Degree/Radian Conversion Tests
// ==============================================================================

TEST(Trigonometry, DegToRadians_Zero_ReturnsZero)
{
    float result = DegToRadians(0.0f);

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(Trigonometry, DegToRadians_45_ReturnsQuarterPi)
{
    float result = DegToRadians(45.0f);

    EXPECT_FLOAT_EQ(result, 0.785398163f);
}

TEST(Trigonometry, DegToRadians_90_ReturnsHalfPi)
{
    float result = DegToRadians(90.0f);

    EXPECT_FLOAT_EQ(result, 1.57079633f);
}

TEST(Trigonometry, DegToRadians_135_ReturnsThreeQuarterPi)
{
    float result = DegToRadians(135.0f);

    EXPECT_FLOAT_EQ(result, 2.35619449f);
}

TEST(Trigonometry, DegToRadians_180_ReturnsPi)
{
    float result = DegToRadians(180.0f);

    EXPECT_FLOAT_EQ(result, 3.14159265f);
}

TEST(Trigonometry, DegToRadians_Negative45_ReturnsNegativeQuarterPi)
{
    float result = DegToRadians(-45.0f);

    EXPECT_FLOAT_EQ(result, -0.785398163f);
}

// ==============================================================================
// Radian/Degree Conversion Tests
// ==============================================================================

TEST(Trigonometry, RadiansToDeg_Zero_ReturnsZero)
{
    float result = RadiansToDeg(0.0f);

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST(Trigonometry, RadiansToDeg_QuarterPi_Returns45)
{
    float result = RadiansToDeg(0.785398163f);

    EXPECT_FLOAT_EQ(result, 45.0f);
}

TEST(Trigonometry, RadiansToDeg_HalfPi_Returns90)
{
    float result = RadiansToDeg(1.57079633f);

    EXPECT_FLOAT_EQ(result, 90.0f);
}

TEST(Trigonometry, RadiansToDeg_ThreeQuarterPi_Returns135)
{
    float result = RadiansToDeg(2.35619449f);

    EXPECT_FLOAT_EQ(result, 135.0f);
}

TEST(Trigonometry, RadiansToDeg_Pi_Returns180)
{
    float result = RadiansToDeg(3.14159265f);

    EXPECT_FLOAT_EQ(result, 180.0f);
}

TEST(Trigonometry, RadiansToDeg_NegativeQuarterPi_ReturnsNegative45)
{
    float result = RadiansToDeg(-0.785398163f);

    EXPECT_FLOAT_EQ(result, -45.0f);
}

// ==============================================================================
// Sin Tests
// ==============================================================================

TEST(Trigonometry, Sin_MatchesStandardLibrary)
{
    float angle = -2.0f * PI;
    while (angle <= (2.0f * PI))
    {
        SCOPED_TRACE(testing::Message() << "angle = " << angle);

        float result = Sin(angle);
        float expected = sinf(angle);

        EXPECT_TRUE(Float::FEqual(result, expected));

        angle += 0.03f;
    }

    // Test exact 2*PI
    angle = 2.0f * PI;
    EXPECT_TRUE(Float::FEqual(Sin(angle), sinf(angle)));
}

TEST(Trigonometry, Sin_WithAngleClass_MatchesStandardLibrary)
{
    float angle = -2.0f * PI;
    while (angle <= (2.0f * PI))
    {
        SCOPED_TRACE(testing::Message() << "angle = " << angle);

        Angle angleClass(angle);
        float result = Sin(angleClass.AsRadians());
        float expected = sinf(angle);

        EXPECT_TRUE(Float::FEqual(result, expected));

        angle += 0.03f;
    }

    // Test exact 2*PI
    angle = 2.0f * PI;
    Angle angleClass(angle);
    EXPECT_TRUE(Float::FEqual(Sin(angleClass.AsRadians()), sinf(angle)));
}

// ==============================================================================
// Cos Tests
// ==============================================================================

TEST(Trigonometry, Cos_MatchesStandardLibrary)
{
    float angle = -2.0f * PI;
    while (angle <= (2.0f * PI))
    {
        SCOPED_TRACE(testing::Message() << "angle = " << angle);

        float result = Cos(angle);
        float expected = cosf(angle);

        EXPECT_TRUE(Float::FEqual(result, expected));

        angle += 0.03f;
    }

    // Test exact 2*PI
    angle = 2.0f * PI;
    EXPECT_TRUE(Float::FEqual(Cos(angle), cosf(angle)));
}

TEST(Trigonometry, Cos_WithAngleClass_MatchesStandardLibrary)
{
    float angle = -2.0f * PI;
    while (angle <= (2.0f * PI))
    {
        SCOPED_TRACE(testing::Message() << "angle = " << angle);

        Angle angleClass(angle);
        float result = Cos(angleClass.AsRadians());
        float expected = cosf(angle);

        EXPECT_TRUE(Float::FEqual(result, expected));

        angle += 0.03f;
    }

    // Test exact 2*PI
    angle = 2.0f * PI;
    Angle angleClass(angle);
    EXPECT_TRUE(Float::FEqual(Cos(angleClass.AsRadians()), cosf(angle)));
}

// ==============================================================================
// Tan Tests
// ==============================================================================

TEST(Trigonometry, Tan_MatchesStandardLibrary)
{
    float angle = -2.0f * PI;
    while (angle <= (2.0f * PI))
    {
        SCOPED_TRACE(testing::Message() << "angle = " << angle);

        float result = Tan(angle);
        float expected = tanf(angle);

        EXPECT_TRUE(Float::FEqual(result, expected, 0.005f));

        angle += 0.03f;
    }

    // Test exact 2*PI
    angle = 2.0f * PI;
    EXPECT_TRUE(Float::FEqual(Tan(angle), tanf(angle)));
}

TEST(Trigonometry, Tan_WithAngleClass_MatchesStandardLibrary)
{
    float angle = -2.0f * PI;
    while (angle <= (2.0f * PI))
    {
        SCOPED_TRACE(testing::Message() << "angle = " << angle);

        Angle angleClass(angle);
        float result = Tan(angleClass.AsRadians());
        float expected = tanf(angle);

        EXPECT_TRUE(Float::FEqual(result, expected, 0.005f));

        angle += 0.03f;
    }

    // Test exact 2*PI
    angle = 2.0f * PI;
    Angle angleClass(angle);
    EXPECT_TRUE(Float::FEqual(Tan(angleClass.AsRadians()), tanf(angle)));
}

// ==============================================================================
// ACos Tests (Arc Cosine)
// ==============================================================================

TEST(Trigonometry, ACos_MatchesStandardLibrary)
{
    float angle = -1.0f;
    while (angle < 1.0f)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << angle);

        float result = ACos(angle);
        float expected = acosf(angle);

        EXPECT_TRUE(Float::FEqual(result, expected));

        angle += 0.03f;
    }

    // Test exact 1.0
    angle = 1.0f;
    EXPECT_TRUE(Float::FEqual(ACos(angle), acosf(angle)));
}

TEST(Trigonometry, ACos_WithAngleClass_MatchesStandardLibrary)
{
    float angle = -1.0f;
    while (angle < 1.0f)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << angle);

        Angle angleClass(angle);
        float result = ACos(angleClass.AsRadians());
        float expected = acosf(angle);

        EXPECT_TRUE(Float::FEqual(result, expected));

        angle += 0.03f;
    }

    // Test exact 1.0
    angle = 1.0f;
    Angle angleClass(angle);
    EXPECT_TRUE(Float::FEqual(ACos(angleClass.AsRadians()), acosf(angle)));
}

// ==============================================================================
// ASin Tests (Arc Sine)
// ==============================================================================

TEST(Trigonometry, ASin_MatchesStandardLibrary)
{
    float angle = -1.0f;
    while (angle <= 1.0f)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << angle);

        float result = ASin(angle);
        float expected = asinf(angle);

        EXPECT_TRUE(Float::FEqual(result, expected));

        angle += 0.03f;
    }

    // Test exact 1.0
    angle = 1.0f;
    EXPECT_TRUE(Float::FEqual(ASin(angle), asinf(angle)));
}

TEST(Trigonometry, ASin_WithAngleClass_MatchesStandardLibrary)
{
    float angle = -1.0f;
    while (angle <= 1.0f)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << angle);

        Angle angleClass(angle);
        float result = ASin(angleClass.AsRadians());
        float expected = asinf(angle);

        EXPECT_TRUE(Float::FEqual(result, expected));

        angle += 0.03f;
    }

    // Test exact 1.0
    angle = 1.0f;
    Angle angleClass(angle);
    EXPECT_TRUE(Float::FEqual(ASin(angleClass.AsRadians()), asinf(angle)));
}

// ==============================================================================
// ATan Tests (Arc Tangent)
// ==============================================================================

TEST(Trigonometry, ATan_MatchesStandardLibrary)
{
    float angle = -PI_HALF;
    while (angle <= PI_HALF)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << angle);

        float result = ATan(angle);
        float expected = atanf(angle);

        EXPECT_TRUE(Float::FEqual(result, expected));

        angle += 0.03f;
    }

    // Test exact PI_HALF
    angle = PI_HALF;
    EXPECT_TRUE(Float::FEqual(ATan(angle), atanf(angle)));
}

TEST(Trigonometry, ATan_WithAngleClass_MatchesStandardLibrary)
{
    float angle = -PI_HALF;
    while (angle <= PI_HALF)
    {
        SCOPED_TRACE(testing::Message() << "angle = " << angle);

        Angle angleClass(angle);
        float result = ATan(angleClass.AsRadians());
        float expected = atanf(angle);

        EXPECT_TRUE(Float::FEqual(result, expected));

        angle += 0.03f;
    }

    // Test exact PI_HALF
    angle = PI_HALF;
    Angle angleClass(angle);
    EXPECT_TRUE(Float::FEqual(ATan(angleClass.AsRadians()), atanf(angle)));
}
