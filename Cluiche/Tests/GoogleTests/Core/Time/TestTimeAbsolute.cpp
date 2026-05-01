// TestTimeAbsolute.cpp - Google Test unit tests for TimeAbsolute
//
// Tests TimeAbsolute from DiaCore Time subsystem

#include <gtest/gtest.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaMaths/Core/FloatMaths.h>

using namespace Dia::Core;

// ==============================================================================
// TimeAbsolute Construction Tests
// ==============================================================================

TEST(TimeAbsolute, Zero_InitializesToZero)
{
    TimeAbsolute time = TimeAbsolute::Zero();

    EXPECT_EQ(time.AsIntInMilliseconds(), 0);
}

TEST(TimeAbsolute, CopyConstructor_CopiesValue)
{
    TimeAbsolute time1 = TimeAbsolute::Zero();
    TimeAbsolute time2(time1);

    EXPECT_EQ(time1, time2);
}

// ==============================================================================
// Factory Method Tests - Days
// ==============================================================================

TEST(TimeAbsolute, CreateFromDays_OneDay_CorrectConversions)
{
    TimeAbsolute time = TimeAbsolute::CreateFromDays(1);

    EXPECT_EQ(time.AsIntInDays(), 1);
    EXPECT_EQ(time.AsIntInHours(), 24);
    EXPECT_EQ(time.AsIntInMinutes(), 24 * 60);
    EXPECT_EQ(time.AsIntInSeconds(), 24 * 60 * 60);
    EXPECT_EQ(time.AsIntInMilliseconds(), 24 * 60 * 60 * 1000);

    EXPECT_FLOAT_EQ(time.AsFloatInDays(), 1.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInHours(), 24.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInMinutes(), 24.0f * 60.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInSeconds(), 24.0f * 60.0f * 60.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInMilliseconds(), 24.0f * 60.0f * 60.0f * 1000.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInMicroseconds(), 24.0f * 60.0f * 60.0f * 1000.0f * 1000.0f);

    long long expectedMicroseconds = 24LL * 60LL * 60LL * 1000LL * 1000LL;
    EXPECT_EQ(time.AsLongLongInMicroseconds(), expectedMicroseconds);
}

// ==============================================================================
// Factory Method Tests - Hours
// ==============================================================================

TEST(TimeAbsolute, CreateFromHours_OneHour_CorrectConversions)
{
    TimeAbsolute time = TimeAbsolute::CreateFromHours(1);

    EXPECT_EQ(time.AsIntInHours(), 1);
    EXPECT_EQ(time.AsIntInMinutes(), 60);
    EXPECT_EQ(time.AsIntInSeconds(), 60 * 60);
    EXPECT_EQ(time.AsIntInMilliseconds(), 60 * 60 * 1000);
    EXPECT_EQ(time.AsIntInDays(), 0);  // Less than one day

    EXPECT_FLOAT_EQ(time.AsFloatInHours(), 1.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInMinutes(), 60.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInSeconds(), 60.0f * 60.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInMilliseconds(), 60.0f * 60.0f * 1000.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInMicroseconds(), 60.0f * 60.0f * 1000.0f * 1000.0f);
    EXPECT_TRUE(Dia::Maths::Float::FEqual(time.AsFloatInDays(), 1.0f / 24.0f));

    long long expectedMicroseconds = 60LL * 60LL * 1000LL * 1000LL;
    EXPECT_EQ(time.AsLongLongInMicroseconds(), expectedMicroseconds);
}

// ==============================================================================
// Factory Method Tests - Minutes
// ==============================================================================

TEST(TimeAbsolute, CreateFromMinutes_OneMinute_CorrectConversions)
{
    TimeAbsolute time = TimeAbsolute::CreateFromMinutes(1);

    EXPECT_EQ(time.AsIntInMinutes(), 1);
    EXPECT_EQ(time.AsIntInSeconds(), 60);
    EXPECT_EQ(time.AsIntInMilliseconds(), 60 * 1000);
    EXPECT_EQ(time.AsIntInHours(), 0);
    EXPECT_EQ(time.AsIntInDays(), 0);

    EXPECT_FLOAT_EQ(time.AsFloatInMinutes(), 1.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInSeconds(), 60.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInMilliseconds(), 60.0f * 1000.0f);
    EXPECT_TRUE(Dia::Maths::Float::FEqual(time.AsFloatInHours(), 1.0f / 60.0f));
}

// ==============================================================================
// Factory Method Tests - Seconds
// ==============================================================================

TEST(TimeAbsolute, CreateFromSeconds_OneSecond_CorrectConversions)
{
    TimeAbsolute time = TimeAbsolute::CreateFromSeconds(1);

    EXPECT_EQ(time.AsIntInSeconds(), 1);
    EXPECT_EQ(time.AsIntInMilliseconds(), 1000);
    EXPECT_EQ(time.AsIntInMinutes(), 0);

    EXPECT_FLOAT_EQ(time.AsFloatInSeconds(), 1.0f);
    EXPECT_FLOAT_EQ(time.AsFloatInMilliseconds(), 1000.0f);
}

// ==============================================================================
// Factory Method Tests - Milliseconds
// ==============================================================================

TEST(TimeAbsolute, CreateFromMilliseconds_OneMillisecond_CorrectConversions)
{
    TimeAbsolute time = TimeAbsolute::CreateFromMilliseconds(1);

    EXPECT_EQ(time.AsIntInMilliseconds(), 1);
    EXPECT_EQ(time.AsIntInSeconds(), 0);

    EXPECT_FLOAT_EQ(time.AsFloatInMilliseconds(), 1.0f);
    EXPECT_TRUE(Dia::Maths::Float::FEqual(time.AsFloatInSeconds(), 0.001f));
}

// ==============================================================================
// Arithmetic Tests - Addition
// ==============================================================================

TEST(TimeAbsolute, OperatorPlus_AddsTimeRelative)
{
    TimeAbsolute time = TimeAbsolute::CreateFromSeconds(10);
    TimeRelative delta = TimeRelative::CreateFromSeconds(5);

    TimeAbsolute result = time + delta;

    EXPECT_EQ(result.AsIntInSeconds(), 15);
}

TEST(TimeAbsolute, OperatorPlusEquals_AddsTimeRelative)
{
    TimeAbsolute time = TimeAbsolute::CreateFromSeconds(10);
    TimeRelative delta = TimeRelative::CreateFromSeconds(5);

    time += delta;

    EXPECT_EQ(time.AsIntInSeconds(), 15);
}

// ==============================================================================
// Arithmetic Tests - Subtraction
// ==============================================================================

TEST(TimeAbsolute, OperatorMinus_SubtractsTimeRelative)
{
    TimeAbsolute time = TimeAbsolute::CreateFromSeconds(10);
    TimeRelative delta = TimeRelative::CreateFromSeconds(5);

    TimeAbsolute result = time - delta;

    EXPECT_EQ(result.AsIntInSeconds(), 5);
}

TEST(TimeAbsolute, OperatorMinus_TimeAbsoluteDifference_ReturnsTimeRelative)
{
    TimeAbsolute time1 = TimeAbsolute::CreateFromSeconds(10);
    TimeAbsolute time2 = TimeAbsolute::CreateFromSeconds(5);

    TimeRelative diff = time1 - time2;

    EXPECT_EQ(diff.AsIntInSeconds(), 5);
}

TEST(TimeAbsolute, OperatorMinusEquals_SubtractsTimeRelative)
{
    TimeAbsolute time = TimeAbsolute::CreateFromSeconds(10);
    TimeRelative delta = TimeRelative::CreateFromSeconds(5);

    time -= delta;

    EXPECT_EQ(time.AsIntInSeconds(), 5);
}

// ==============================================================================
// Comparison Tests
// ==============================================================================

TEST(TimeAbsolute, OperatorEquals_SameTime_ReturnsTrue)
{
    TimeAbsolute time1 = TimeAbsolute::CreateFromSeconds(10);
    TimeAbsolute time2 = TimeAbsolute::CreateFromSeconds(10);

    EXPECT_EQ(time1, time2);
}

TEST(TimeAbsolute, OperatorNotEquals_DifferentTime_ReturnsTrue)
{
    TimeAbsolute time1 = TimeAbsolute::CreateFromSeconds(10);
    TimeAbsolute time2 = TimeAbsolute::CreateFromSeconds(5);

    EXPECT_NE(time1, time2);
}

TEST(TimeAbsolute, OperatorLessThan_EarlierTime_ReturnsTrue)
{
    TimeAbsolute earlier = TimeAbsolute::CreateFromSeconds(5);
    TimeAbsolute later = TimeAbsolute::CreateFromSeconds(10);

    EXPECT_LT(earlier, later);
    EXPECT_FALSE(later < earlier);
}

TEST(TimeAbsolute, OperatorLessThanOrEqual_SameOrEarlier_ReturnsTrue)
{
    TimeAbsolute time1 = TimeAbsolute::CreateFromSeconds(5);
    TimeAbsolute time2 = TimeAbsolute::CreateFromSeconds(10);
    TimeAbsolute time3 = TimeAbsolute::CreateFromSeconds(5);

    EXPECT_LE(time1, time2);
    EXPECT_LE(time1, time3);
}

TEST(TimeAbsolute, OperatorGreaterThan_LaterTime_ReturnsTrue)
{
    TimeAbsolute earlier = TimeAbsolute::CreateFromSeconds(5);
    TimeAbsolute later = TimeAbsolute::CreateFromSeconds(10);

    EXPECT_GT(later, earlier);
    EXPECT_FALSE(earlier > later);
}

TEST(TimeAbsolute, OperatorGreaterThanOrEqual_SameOrLater_ReturnsTrue)
{
    TimeAbsolute time1 = TimeAbsolute::CreateFromSeconds(10);
    TimeAbsolute time2 = TimeAbsolute::CreateFromSeconds(5);
    TimeAbsolute time3 = TimeAbsolute::CreateFromSeconds(10);

    EXPECT_GE(time1, time2);
    EXPECT_GE(time1, time3);
}

// ==============================================================================
// Edge Cases
// ==============================================================================

TEST(TimeAbsolute, LargeValues_NoOverflow)
{
    TimeAbsolute time = TimeAbsolute::CreateFromDays(365);

    EXPECT_EQ(time.AsIntInDays(), 365);
    EXPECT_GT(time.AsLongLongInMicroseconds(), 0);
}

TEST(TimeAbsolute, ZeroTime_AllConversionsReturnZero)
{
    TimeAbsolute zero = TimeAbsolute::Zero();

    EXPECT_EQ(zero.AsIntInDays(), 0);
    EXPECT_EQ(zero.AsIntInHours(), 0);
    EXPECT_EQ(zero.AsIntInMinutes(), 0);
    EXPECT_EQ(zero.AsIntInSeconds(), 0);
    EXPECT_EQ(zero.AsIntInMilliseconds(), 0);
    EXPECT_EQ(zero.AsLongLongInMicroseconds(), 0);
}
