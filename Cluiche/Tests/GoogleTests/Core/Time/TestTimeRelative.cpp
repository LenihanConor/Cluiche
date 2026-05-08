// TestTimeRelative.cpp - Google Test unit tests for TimeRelative
//
// Tests TimeRelative from DiaCore Time subsystem

#include <gtest/gtest.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaMaths/Core/FloatMaths.h>
#include <climits>

using namespace Dia::Core;

// ==============================================================================
// TimeRelative Construction Tests
// ==============================================================================

TEST(TimeRelative, Zero_InitializesToZero)
{
    TimeRelative time = TimeRelative::Zero();

    EXPECT_EQ(time.AsIntInMilliseconds(), 0);
}

TEST(TimeRelative, CopyConstructor_CopiesValue)
{
    TimeRelative time1 = TimeRelative::Zero();
    TimeRelative time2(time1);

    EXPECT_EQ(time1, time2);
}

// ==============================================================================
// Factory Methods - Days (int)
// ==============================================================================

TEST(TimeRelative, CreateFromDays_Int_OneDay)
{
    TimeRelative time = TimeRelative::CreateFromDays(1);

    EXPECT_EQ(time.AsIntInDays(), 1);
    EXPECT_EQ(time.AsIntInHours(), 24);
    EXPECT_EQ(time.AsIntInMinutes(), 24 * 60);
    EXPECT_EQ(time.AsIntInSeconds(), 24 * 60 * 60);
    EXPECT_EQ(time.AsIntInMilliseconds(), 24 * 60 * 60 * 1000);

    EXPECT_FLOAT_EQ(time.AsFloatInDays(), 1.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInHours(), 24.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInMinutes(), 24.0f * 60.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInSeconds(), 24.0f * 60.0f * 60.0f);

    long long expectedMicroseconds = 24LL * 60LL * 60LL * 1000LL * 1000LL;
    EXPECT_EQ(time.AsLongLongInMicroseconds(), expectedMicroseconds);
}

// ==============================================================================
// Factory Methods - Hours (int)
// ==============================================================================

TEST(TimeRelative, CreateFromHours_Int_OneHour)
{
    TimeRelative time = TimeRelative::CreateFromHours(1);

    EXPECT_EQ(time.AsIntInHours(), 1);
    EXPECT_EQ(time.AsIntInMinutes(), 60);
    EXPECT_EQ(time.AsIntInSeconds(), 60 * 60);
    EXPECT_EQ(time.AsIntInMilliseconds(), 60 * 60 * 1000);
    EXPECT_EQ(time.AsIntInDays(), 0);

    EXPECT_FLOAT_EQ(time.AsFloatInHours(), 1.0f);
    EXPECT_TRUE(Dia::Maths::Float::FEqual(time.AsFloatInDays(), 1.0f / 24.0f));

    long long expectedMicroseconds = 60LL * 60LL * 1000LL * 1000LL;
    EXPECT_EQ(time.AsLongLongInMicroseconds(), expectedMicroseconds);
}

// ==============================================================================
// Factory Methods - Minutes (int)
// ==============================================================================

TEST(TimeRelative, CreateFromMinutes_Int_OneMinute)
{
    TimeRelative time = TimeRelative::CreateFromMinutes(1);

    EXPECT_EQ(time.AsIntInMinutes(), 1);
    EXPECT_EQ(time.AsIntInSeconds(), 60);
    EXPECT_EQ(time.AsIntInMilliseconds(), 60 * 1000);
    EXPECT_EQ(time.AsIntInHours(), 0);

    EXPECT_FLOAT_EQ(time.AsFloatInMinutes(), 1.0f);
    EXPECT_TRUE(Dia::Maths::Float::FEqual(time.AsFloatInHours(), 1.0f / 60.0f));

    long long expectedMicroseconds = 60LL * 1000LL * 1000LL;
    EXPECT_EQ(time.AsIntInMicroseconds(), expectedMicroseconds);
}

// ==============================================================================
// Factory Methods - Seconds (int)
// ==============================================================================

TEST(TimeRelative, CreateFromSeconds_Int_OneSecond)
{
    TimeRelative time = TimeRelative::CreateFromSeconds(1);

    EXPECT_EQ(time.AsIntInSeconds(), 1);
    EXPECT_EQ(time.AsIntInMilliseconds(), 1000);
    EXPECT_EQ(time.AsIntInMinutes(), 0);

    EXPECT_FLOAT_EQ(time.AsFloatInSeconds(), 1.0f);
    EXPECT_TRUE(Dia::Maths::Float::FEqual(time.AsFloatInMinutes(), 1.0f / 60.0f));

    long long expectedMicroseconds = 1000LL * 1000LL;
    EXPECT_EQ(time.AsLongLongInMicroseconds(), expectedMicroseconds);
}

// ==============================================================================
// Factory Methods - Milliseconds (int)
// ==============================================================================

TEST(TimeRelative, CreateFromMilliseconds_Int_OneMillisecond)
{
    TimeRelative time = TimeRelative::CreateFromMilliseconds(1);

    EXPECT_EQ(time.AsIntInMilliseconds(), 1);
    EXPECT_EQ(time.AsIntInSeconds(), 0);

    EXPECT_FLOAT_EQ(time.AsFloatInMilliseconds(), 1.0f);
    EXPECT_TRUE(Dia::Maths::Float::FEqual(time.AsFloatInSeconds(), 1.0f / 1000.0f));

    EXPECT_EQ(time.AsIntInMicroseconds(), 1000LL);
}

// ==============================================================================
// Factory Methods - Microseconds (int)
// ==============================================================================

TEST(TimeRelative, CreateFromMicroseconds_Int_OneMicrosecond)
{
    TimeRelative time = TimeRelative::CreateFromMicroseconds(1);

    EXPECT_EQ(time.AsIntInMicroseconds(), 1);
    EXPECT_EQ(time.AsIntInMilliseconds(), 0);

    EXPECT_FLOAT_EQ(time.AsFloatInMicroseconds(), 1.0f);
    EXPECT_TRUE(Dia::Maths::Float::FEqual(time.AsFloatInMilliseconds(), 1.0f / 1000.0f));
}

// ==============================================================================
// Factory Methods - Float Versions
// ==============================================================================

TEST(TimeRelative, CreateFromDays_Float_OneDay)
{
    TimeRelative time = TimeRelative::CreateFromDays(1.0f);

    EXPECT_EQ(time.AsIntInDays(), 1);
    EXPECT_FLOAT_EQ(time.AsFloatInDays(), 1.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInHours(), 24.0f);
}

TEST(TimeRelative, CreateFromHours_Float_OneHour)
{
    TimeRelative time = TimeRelative::CreateFromHours(1.0f);

    EXPECT_EQ(time.AsIntInHours(), 1);
    EXPECT_FLOAT_EQ(time.AsFloatInHours(), 1.0f);
}

TEST(TimeRelative, CreateFromMinutes_Float_OneMinute)
{
    TimeRelative time = TimeRelative::CreateFromMinutes(1.0f);

    EXPECT_EQ(time.AsIntInMinutes(), 1);
    EXPECT_FLOAT_EQ(time.AsFloatInMinutes(), 1.0f);
}

TEST(TimeRelative, CreateFromSeconds_Float_OneSecond)
{
    TimeRelative time = TimeRelative::CreateFromSeconds(1.0f);

    EXPECT_EQ(time.AsIntInSeconds(), 1);
    EXPECT_FLOAT_EQ(time.AsFloatInSeconds(), 1.0f);
}

TEST(TimeRelative, CreateFromMilliseconds_Float_OneMillisecond)
{
    TimeRelative time = TimeRelative::CreateFromMilliseconds(1.0f);

    EXPECT_EQ(time.AsIntInMilliseconds(), 1);
    EXPECT_FLOAT_EQ(time.AsFloatInMilliseconds(), 1.0f);
}

TEST(TimeRelative, CreateFromMicroseconds_Float_OneMicrosecond)
{
    TimeRelative time = TimeRelative::CreateFromMicroseconds(1.0f);

    EXPECT_EQ(time.AsIntInMicroseconds(), 1);
    EXPECT_FLOAT_EQ(time.AsFloatInMicroseconds(), 1.0f);
}

// ==============================================================================
// Special Values
// ==============================================================================

TEST(TimeRelative, Zero_AllConversionsReturnZero)
{
    TimeRelative time = TimeRelative::Zero();

    EXPECT_EQ(time.AsIntInDays(), 0);
    EXPECT_EQ(time.AsIntInHours(), 0);
    EXPECT_EQ(time.AsIntInMinutes(), 0);
    EXPECT_EQ(time.AsIntInSeconds(), 0);
    EXPECT_EQ(time.AsIntInMilliseconds(), 0);
}

TEST(TimeRelative, MaximumTime_ReturnsMaxValue)
{
    TimeRelative time = TimeRelative::MaximumTime();

    EXPECT_EQ(time.AsLongLongInMicroseconds(), LLONG_MAX);
}

TEST(TimeRelative, MinimumTime_ReturnsNegativeValue)
{
    TimeRelative time = TimeRelative::MinimumTime();

    EXPECT_EQ(time.AsIntInDays(), -1);
}

// ==============================================================================
// Negative Time Tests
// ==============================================================================

TEST(TimeRelative, NegativeTime_AsPositiveTime)
{
    TimeRelative time = TimeRelative::CreateFromMinutes(-2.0f);

    EXPECT_FLOAT_EQ(time.AsFloatInMinutes(), -2.0f);
    EXPECT_FLOAT_EQ(time.AsPositiveTime().AsFloatInMinutes(), 2.0f);
}

// ==============================================================================
// Comparison Operators
// ==============================================================================

TEST(TimeRelative, OperatorLessThan_Compares)
{
    TimeRelative time1 = TimeRelative::CreateFromSeconds(30.0f);
    TimeRelative time2 = TimeRelative::CreateFromSeconds(2.0f);
    TimeRelative time3 = TimeRelative::CreateFromSeconds(2.0f);

    EXPECT_FALSE(time1 < time2);
    EXPECT_FALSE(time2 < time3);
    EXPECT_TRUE(time2 < time1);
}

TEST(TimeRelative, OperatorLessThanOrEqual_Compares)
{
    TimeRelative time1 = TimeRelative::CreateFromSeconds(30.0f);
    TimeRelative time2 = TimeRelative::CreateFromSeconds(2.0f);
    TimeRelative time3 = TimeRelative::CreateFromSeconds(2.0f);

    EXPECT_FALSE(time1 <= time2);
    EXPECT_TRUE(time2 <= time3);
    EXPECT_TRUE(time2 <= time1);
}

TEST(TimeRelative, OperatorEquals_Compares)
{
    TimeRelative time1 = TimeRelative::CreateFromSeconds(30.0f);
    TimeRelative time2 = TimeRelative::CreateFromSeconds(2.0f);
    TimeRelative time3 = TimeRelative::CreateFromSeconds(2.0f);

    EXPECT_FALSE(time1 == time2);
    EXPECT_TRUE(time2 == time3);
    EXPECT_FALSE(time2 == time1);
}

TEST(TimeRelative, OperatorNotEquals_Compares)
{
    TimeRelative time1 = TimeRelative::CreateFromSeconds(30.0f);
    TimeRelative time2 = TimeRelative::CreateFromSeconds(2.0f);
    TimeRelative time3 = TimeRelative::CreateFromSeconds(2.0f);

    EXPECT_TRUE(time1 != time2);
    EXPECT_FALSE(time2 != time3);
    EXPECT_TRUE(time2 != time1);
}

TEST(TimeRelative, OperatorGreaterThanOrEqual_Compares)
{
    TimeRelative time1 = TimeRelative::CreateFromSeconds(30.0f);
    TimeRelative time2 = TimeRelative::CreateFromSeconds(2.0f);
    TimeRelative time3 = TimeRelative::CreateFromSeconds(2.0f);

    EXPECT_TRUE(time1 >= time2);
    EXPECT_TRUE(time2 >= time3);
    EXPECT_FALSE(time2 >= time1);
}

TEST(TimeRelative, OperatorGreaterThan_Compares)
{
    TimeRelative time1 = TimeRelative::CreateFromSeconds(30.0f);
    TimeRelative time2 = TimeRelative::CreateFromSeconds(2.0f);
    TimeRelative time3 = TimeRelative::CreateFromSeconds(2.0f);

    EXPECT_TRUE(time1 > time2);
    EXPECT_FALSE(time2 > time3);
    EXPECT_FALSE(time2 > time1);
}

// ==============================================================================
// Arithmetic Operations - TimeRelative + TimeRelative
// ==============================================================================

TEST(TimeRelative, OperatorPlus_AddsTimeRelative)
{
    TimeRelative time1 = TimeRelative::CreateFromSeconds(30.0f);
    TimeRelative time2 = TimeRelative::CreateFromSeconds(2.0f);

    TimeRelative result = time1 + time2;

    EXPECT_FLOAT_EQ(result.AsFloatInSeconds(), 32.0f);
}

TEST(TimeRelative, OperatorMinus_SubtractsTimeRelative)
{
    TimeRelative time1 = TimeRelative::CreateFromSeconds(30.0f);
    TimeRelative time2 = TimeRelative::CreateFromSeconds(2.0f);

    TimeRelative result = time1 - time2;

    EXPECT_FLOAT_EQ(result.AsFloatInSeconds(), 28.0f);
}

// ==============================================================================
// Arithmetic Operations - TimeAbsolute + TimeRelative
// ==============================================================================

TEST(TimeRelative, TimeAbsolutePlus_AddsTimeRelative)
{
    TimeAbsolute time4 = TimeAbsolute::CreateFromSeconds(2.0f);
    TimeRelative time2 = TimeRelative::CreateFromSeconds(2.0f);

    TimeAbsolute result = time4 + time2;

    EXPECT_FLOAT_EQ(result.AsFloatInSeconds(), 4.0f);
}

TEST(TimeRelative, TimeAbsoluteMinus_SubtractsTimeRelative)
{
    TimeAbsolute time4 = TimeAbsolute::CreateFromSeconds(2.0f);
    TimeRelative time2 = TimeRelative::CreateFromSeconds(2.0f);

    TimeAbsolute result = time4 - time2;

    EXPECT_FLOAT_EQ(result.AsFloatInSeconds(), 0.0f);
}

TEST(TimeRelative, TimeAbsolutePlusEquals_AddsTimeRelative)
{
    TimeAbsolute time6 = TimeAbsolute::CreateFromSeconds(3.0f);
    TimeRelative time2 = TimeRelative::CreateFromSeconds(2.0f);

    time6 += time2;

    EXPECT_FLOAT_EQ(time6.AsFloatInSeconds(), 5.0f);
}

TEST(TimeRelative, TimeAbsoluteMinusEquals_SubtractsTimeRelative)
{
    TimeAbsolute time6 = TimeAbsolute::CreateFromSeconds(5.0f);
    TimeRelative time2 = TimeRelative::CreateFromSeconds(2.0f);

    time6 -= time2;

    EXPECT_FLOAT_EQ(time6.AsFloatInSeconds(), 3.0f);
}

// ==============================================================================
// Multiplication and Division
// ==============================================================================

TEST(TimeRelative, OperatorMultiply_ByInt)
{
    TimeRelative time = TimeRelative::CreateFromSeconds(30.0f);

    TimeRelative result = time * 2;

    EXPECT_FLOAT_EQ(result.AsFloatInSeconds(), 60.0f);
}

TEST(TimeRelative, OperatorMultiply_ByFloat)
{
    TimeRelative time = TimeRelative::CreateFromSeconds(30.0f);

    TimeRelative result = time * 2.0f;

    EXPECT_FLOAT_EQ(result.AsFloatInSeconds(), 60.0f);
}

TEST(TimeRelative, OperatorDivide_ByInt)
{
    TimeRelative time = TimeRelative::CreateFromSeconds(30.0f);

    TimeRelative result = time / 2;

    EXPECT_FLOAT_EQ(result.AsFloatInSeconds(), 15.0f);
}

TEST(TimeRelative, OperatorDivide_ByFloat)
{
    TimeRelative time = TimeRelative::CreateFromSeconds(30.0f);

    TimeRelative result = time / 2.0f;

    EXPECT_FLOAT_EQ(result.AsFloatInSeconds(), 15.0f);
}

TEST(TimeRelative, OperatorMultiplyEquals_ByInt)
{
    TimeRelative time = TimeRelative::CreateFromSeconds(30.0f);

    time *= 2;

    EXPECT_FLOAT_EQ(time.AsFloatInSeconds(), 60.0f);
}

TEST(TimeRelative, OperatorMultiplyEquals_ByFloat)
{
    TimeRelative time = TimeRelative::CreateFromSeconds(30.0f);

    time *= 2.0f;

    EXPECT_FLOAT_EQ(time.AsFloatInSeconds(), 60.0f);
}

TEST(TimeRelative, OperatorDivideEquals_ByInt)
{
    TimeRelative time = TimeRelative::CreateFromSeconds(60.0f);

    time /= 2;

    EXPECT_FLOAT_EQ(time.AsFloatInSeconds(), 30.0f);
}

TEST(TimeRelative, OperatorDivideEquals_ByFloat)
{
    TimeRelative time = TimeRelative::CreateFromSeconds(60.0f);

    time /= 2.0f;

    EXPECT_FLOAT_EQ(time.AsFloatInSeconds(), 30.0f);
}
