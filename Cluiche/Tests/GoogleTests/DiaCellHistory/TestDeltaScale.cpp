#include <DiaCellHistory/DeltaScale.h>
#include <gtest/gtest.h>

#include <limits>

using Dia::CellHistory::DeltaScale;

TEST(DeltaScale, NiceCeilTable)
{
	struct C { float in, out; };
	const C cases[] = {
		{ 0.07f, 0.1f }, { 0.1f, 0.1f }, { 0.11f, 0.2f }, { 0.2f, 0.2f }, { 0.21f, 0.5f }, { 0.5f, 0.5f }, { 0.51f, 1.0f },
		{ 1.0f, 1.0f }, { 1.5f, 2.0f }, { 2.0f, 2.0f }, { 3.0f, 5.0f }, { 5.0f, 5.0f }, { 5.0001f, 10.0f }, { 10.0f, 10.0f },
		{ 130.0f, 200.0f }, { 1000.0f, 1000.0f }, { 1000.5f, 2000.0f }, { 1e-3f, 1e-3f }, { 0.002f, 0.002f }, { 0.005f, 0.005f },
		{ 60.3f, 100.0f }, { 45.0f, 50.0f }, { 20.0f, 20.0f }, { 500.0f, 500.0f }, { 50.0f, 50.0f }, { 100.0f, 100.0f }, { 0.01f, 0.01f },
	};
	for (const C& c : cases) { EXPECT_FLOAT_EQ(DeltaScale::NiceCeil(c.in), c.out) << c.in; }
}

TEST(DeltaScale, NiceCeilExactBoundariesDoNotDrift)
{
	// exactly the same float, not merely near
	const float vals[] = { 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 1000.0f, 1e-3f, 1e-2f, 1e5f };
	for (float v : vals) { EXPECT_EQ(DeltaScale::NiceCeil(v), v) << v; }
}

TEST(DeltaScale, NiceCeilDegenerate)
{
	const float inf = std::numeric_limits<float>::infinity();
	EXPECT_EQ(DeltaScale::NiceCeil(0.0f), DeltaScale::kFloor);
	EXPECT_EQ(DeltaScale::NiceCeil(-3.0f), DeltaScale::kFloor);
	EXPECT_EQ(DeltaScale::NiceCeil(std::numeric_limits<float>::quiet_NaN()), DeltaScale::kDefault);
	EXPECT_EQ(DeltaScale::NiceCeil(inf), DeltaScale::kDefault);
}

TEST(DeltaScale, FreshHasNoValueAndDefaults)
{
	DeltaScale s;
	EXPECT_FALSE(s.HasValue());
	EXPECT_EQ(s.Current(), 1.0f);
}

TEST(DeltaScale, FirstObservationSetsNiceCeil)
{
	DeltaScale s;
	EXPECT_EQ(s.Observe(60.3f, 0.0), 100.0f);
	EXPECT_TRUE(s.HasValue());
	EXPECT_EQ(s.Current(), 100.0f);
}

TEST(DeltaScale, GrowsImmediately)
{
	DeltaScale s; s.Observe(3.0f, 0.0);
	EXPECT_EQ(s.Current(), 5.0f);
	EXPECT_EQ(s.Observe(5.5f, 0.001), 10.0f);
	EXPECT_EQ(s.Observe(130.0f, 0.002), 200.0f);
}

TEST(DeltaScale, HoldsWhenBetween40And100Percent)
{
	DeltaScale s; s.Observe(100.0f, 0.0);
	EXPECT_EQ(s.Observe(50.0f, 10.0), 100.0f);	// NiceCeil 50 < 100 but 50 >= 40
	EXPECT_EQ(s.Observe(40.0f, 20.0), 100.0f);	// exactly 40% holds
	EXPECT_EQ(s.Observe(40.0f, 100.0), 100.0f);
}

TEST(DeltaScale, NoShrinkBeforeTwoSeconds)
{
	DeltaScale s; s.Observe(100.0f, 0.0);
	EXPECT_EQ(s.Observe(10.0f, 1.0), 100.0f);	// timer starts at 1.0
	EXPECT_EQ(s.Observe(10.0f, 2.9), 100.0f);
	EXPECT_EQ(s.Observe(10.0f, 2.999), 100.0f);
}

TEST(DeltaScale, ShrinksAtExactlyTwoSeconds)
{
	DeltaScale s; s.Observe(100.0f, 0.0);
	s.Observe(10.0f, 1.0);
	EXPECT_EQ(s.Observe(10.0f, 3.0), 10.0f);
}

TEST(DeltaScale, HighReadCancelsTimer)
{
	DeltaScale s; s.Observe(100.0f, 0.0);
	s.Observe(10.0f, 1.0);
	EXPECT_EQ(s.Observe(50.0f, 2.0), 100.0f);	// >= 40%: cancel
	EXPECT_EQ(s.Observe(10.0f, 2.5), 100.0f);	// timer restarts at 2.5
	EXPECT_EQ(s.Observe(10.0f, 3.5), 100.0f);	// would have shrunk had the timer not been cancelled
	EXPECT_EQ(s.Observe(10.0f, 4.4), 100.0f);
	EXPECT_EQ(s.Observe(10.0f, 4.5), 10.0f);
}

TEST(DeltaScale, GrowCancelsTimer)
{
	DeltaScale s; s.Observe(100.0f, 0.0);
	s.Observe(10.0f, 1.0);
	EXPECT_EQ(s.Observe(300.0f, 2.0), 500.0f);
	EXPECT_EQ(s.Observe(10.0f, 3.0), 500.0f);	// new timer from 3.0
	EXPECT_EQ(s.Observe(10.0f, 4.9), 500.0f);
	EXPECT_EQ(s.Observe(10.0f, 5.0), 10.0f);
}

TEST(DeltaScale, ShrinkLandsOnNiceCeilOfCurrentMax)
{
	DeltaScale s; s.Observe(100.0f, 0.0);
	s.Observe(3.0f, 1.0);
	s.Observe(1.0f, 2.0);
	EXPECT_EQ(s.Observe(7.0f, 3.0), 10.0f);	// max at that moment (7), not the earliest low one
}

TEST(DeltaScale, ShrinkLandsBelowCurrentMax40PercentStillHolds)
{
	// after shrinking the timer is cleared: a further low read needs a fresh 2 s
	DeltaScale s; s.Observe(100.0f, 0.0);
	s.Observe(1.0f, 1.0);
	EXPECT_EQ(s.Observe(1.0f, 3.0), 1.0f);
	EXPECT_EQ(s.Observe(0.1f, 3.5), 1.0f);
	EXPECT_EQ(s.Observe(0.1f, 5.4), 1.0f);
	EXPECT_EQ(s.Observe(0.1f, 5.5), 0.1f);
}

TEST(DeltaScale, AllZeroFirstReadIsDefaultScale)
{
	DeltaScale s;
	EXPECT_EQ(s.Observe(0.0f, 0.0), DeltaScale::kDefault);
	EXPECT_TRUE(s.HasValue());
	EXPECT_EQ(s.Observe(0.0f, 1.0), 1.0f);	// timer starts
	EXPECT_EQ(s.Observe(0.0f, 2.9), 1.0f);
	EXPECT_EQ(s.Observe(2.0f, 3.0), 2.0f);	// grows immediately
}

TEST(DeltaScale, NeverShrinksBelowFloor)
{
	DeltaScale s; s.Observe(1.0f, 0.0);
	s.Observe(0.0f, 1.0);
	EXPECT_EQ(s.Observe(0.0f, 3.0), DeltaScale::kFloor);
	EXPECT_EQ(s.Observe(0.0f, 100.0), DeltaScale::kFloor);
	EXPECT_EQ(s.Observe(0.0f, 200.0), DeltaScale::kFloor);
	EXPECT_EQ(s.Observe(1e-9f, 300.0), DeltaScale::kFloor);
	EXPECT_EQ(s.Observe(1e-9f, 303.0), DeltaScale::kFloor);	// a tiny nonzero max must not undercut the floor either
}

TEST(DeltaScale, AllZeroShrinksToFloorAfterTwoSeconds)
{
	DeltaScale s; s.Observe(10.0f, 0.0);
	EXPECT_EQ(s.Observe(0.0f, 1.0), 10.0f);
	EXPECT_EQ(s.Observe(0.0f, 2.9), 10.0f);
	EXPECT_EQ(s.Observe(0.0f, 3.0), DeltaScale::kFloor);
}

TEST(DeltaScale, NonFiniteAndNegativeMaxIgnored)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();
	DeltaScale s;
	EXPECT_EQ(s.Observe(nan, 0.0), DeltaScale::kDefault);
	EXPECT_FALSE(s.HasValue());
	EXPECT_EQ(s.Observe(-5.0f, 0.0), DeltaScale::kDefault);
	EXPECT_FALSE(s.HasValue());
	s.Observe(10.0f, 0.0);
	s.Observe(1.0f, 1.0);	// timer running
	EXPECT_EQ(s.Observe(nan, 2.0), 10.0f);
	EXPECT_EQ(s.Observe(inf, 2.5), 10.0f);
	EXPECT_EQ(s.Observe(-1.0f, 3.0), 10.0f);
	EXPECT_EQ(s.Observe(1.0f, 3.0), 1.0f);	// ignored reads did not disturb the timer (low since 1.0)
}

TEST(DeltaScale, ResetForgetsEverything)
{
	DeltaScale s; s.Observe(100.0f, 0.0); s.Observe(1.0f, 1.0);
	s.Reset();
	EXPECT_FALSE(s.HasValue());
	EXPECT_EQ(s.Observe(1.0f, 3.0), 1.0f);	// first read, not a shrink
	EXPECT_EQ(s.Observe(0.1f, 3.1), 1.0f);	// no stale timer
	EXPECT_EQ(s.Observe(0.1f, 5.0), 1.0f);
}

TEST(DeltaScale, SteadyEqualMaxNeverChanges)
{
	DeltaScale s; s.Observe(3.0f, 0.0);
	for (int i = 1; i < 100; ++i) { EXPECT_EQ(s.Observe(3.0f, i), 5.0f); }
}
