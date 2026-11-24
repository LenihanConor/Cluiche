#include <gtest/gtest.h>
#include <DiaObservation/Profile/TripDetector.h>

using Dia::Observation::Profile::TripConfig;
using Dia::Observation::Profile::TripDetector;
using Dia::Observation::Profile::TripResult;

namespace
{
	const float kBudget = 10.0f;
	const float kOver   = 12.0f; // over budget, below factor 2
	const float kUnder  = 5.0f;

	// Burns the first (ignored) entry of a unit.
	void Prime(TripDetector& d, double now = 0.0) { d.Observe(kUnder, kBudget, false, now); }

	TripResult Burst(TripDetector& d, double now)
	{
		TripResult r;
		for (int i = 0; i < 10; ++i)
		{
			TripResult x = d.Observe(kOver, kBudget, false, now);
			if (x.tripped) r = x;
		}
		return r;
	}

	void Calm(TripDetector& d, double now)
	{
		for (int i = 0; i < 30; ++i) d.Observe(kUnder, kBudget, false, now);
	}
}

TEST(TripDetector, TenConsecutiveOverBudgetTripsExactlyOnce)
{
	TripDetector d;
	Prime(d);
	int trips = 0;
	TripResult last;
	for (int i = 0; i < 10; ++i)
	{
		TripResult r = d.Observe(kOver, kBudget, false, 100.0 + i);
		if (r.tripped) { ++trips; last = r; EXPECT_EQ(9, i); }
	}
	EXPECT_EQ(1, trips);
	EXPECT_EQ(1u, last.firstEntryIndex);
	EXPECT_EQ(10u, last.lastEntryIndex);
	EXPECT_FLOAT_EQ(kOver, last.peakMs);
	EXPECT_EQ(0u, last.suppressedSinceLastTrip);
	EXPECT_EQ(1u, d.GetTripCount());
	EXPECT_EQ(10u, d.GetOverBudgetFrames());
}

TEST(TripDetector, NineOverDoesNotTrip)
{
	TripDetector d;
	Prime(d);
	for (int i = 0; i < 9; ++i) EXPECT_FALSE(d.Observe(kOver, kBudget, false, i).tripped);
	EXPECT_EQ(0u, d.GetTripCount());
}

TEST(TripDetector, ThousandOverBudgetEntriesYieldOneTrip)
{
	TripDetector d;
	Prime(d);
	uint32_t trips = 0;
	for (int i = 0; i < 1000; ++i)
		if (d.Observe(kOver, kBudget, false, 100.0 + i * 16.0).tripped) ++trips;
	EXPECT_EQ(1u, trips); // disarmed, never re-arms without under-budget entries
}

TEST(TripDetector, RepeatedBurstsAreRateLimitedAndReportSuppressed)
{
	TripDetector d;
	Prime(d);
	double now = 0.0;
	uint32_t emitted = 0;
	TripResult records[8];
	// Each cycle: 10 over + 30 under at 100 ms per entry => 4000 ms per cycle.
	for (int cycle = 0; cycle < 6; ++cycle)
	{
		for (int i = 0; i < 10; ++i)
		{
			now += 100.0;
			TripResult r = d.Observe(kOver, kBudget, false, now);
			if (r.tripped && emitted < 8) records[emitted++] = r;
		}
		for (int i = 0; i < 30; ++i) { now += 100.0; d.Observe(kUnder, kBudget, false, now); }
	}
	// Trip candidates at ~1000, 5000, 9000, ... ms: 1000 emits, 5000 is 4000 later (suppressed),
	// 9000 emits (8000 later) and reports 1 suppressed.
	ASSERT_GE(emitted, 2u);
	EXPECT_EQ(0u, records[0].suppressedSinceLastTrip);
	EXPECT_EQ(1u, records[1].suppressedSinceLastTrip);
	EXPECT_EQ(6u, d.GetSuppressedCount() + emitted);
	EXPECT_EQ(emitted, d.GetTripCount());
}

TEST(TripDetector, SuppressedCountResetsAfterEmit)
{
	TripDetector d;
	Prime(d);
	TripConfig c; c.minIntervalMs = 1000.0; d.SetConfig(c);
	EXPECT_TRUE(Burst(d, 0.0).tripped);
	Calm(d, 0.0);
	EXPECT_FALSE(Burst(d, 500.0).tripped);
	Calm(d, 500.0);
	EXPECT_FALSE(Burst(d, 900.0).tripped);
	Calm(d, 900.0);
	TripResult r = Burst(d, 2000.0);
	EXPECT_TRUE(r.tripped);
	EXPECT_EQ(2u, r.suppressedSinceLastTrip);
	Calm(d, 2000.0);
	TripResult r2 = Burst(d, 4000.0);
	EXPECT_TRUE(r2.tripped);
	EXPECT_EQ(0u, r2.suppressedSinceLastTrip);
	EXPECT_EQ(2u, d.GetSuppressedCount());
}

TEST(TripDetector, SingleFrameFactorTrip)
{
	TripDetector d;
	Prime(d);
	TripResult r = d.Observe(25.0f, kBudget, false, 10.0);
	EXPECT_TRUE(r.tripped);
	EXPECT_EQ(1u, r.firstEntryIndex);
	EXPECT_EQ(1u, r.lastEntryIndex);
	EXPECT_FLOAT_EQ(25.0f, r.peakMs);
}

TEST(TripDetector, FactorBoundaryIsNotATrip)
{
	TripDetector d;
	Prime(d);
	EXPECT_FALSE(d.Observe(20.0f, kBudget, false, 10.0).tripped); // == 2 * budget
	EXPECT_TRUE(d.Observe(20.5f, kBudget, false, 20.0).tripped);
}

TEST(TripDetector, PeakCoversWholeRun)
{
	TripDetector d;
	Prime(d);
	for (int i = 0; i < 8; ++i) d.Observe(11.0f, kBudget, false, 0.0);
	d.Observe(19.0f, kBudget, false, 0.0);
	TripResult r = d.Observe(12.0f, kBudget, false, 0.0);
	ASSERT_TRUE(r.tripped);
	EXPECT_FLOAT_EQ(19.0f, r.peakMs);
	EXPECT_EQ(1u, r.firstEntryIndex);
	EXPECT_EQ(10u, r.lastEntryIndex);
}

TEST(TripDetector, RearmRequiresConsecutiveUnderBudgetEntries)
{
	TripDetector d;
	TripConfig c; c.minIntervalMs = 0.0; d.SetConfig(c);
	Prime(d);
	for (int i = 0; i < 10; ++i) d.Observe(kOver, kBudget, false, 0.0);
	EXPECT_FALSE(d.IsArmed());
	for (int i = 0; i < 29; ++i) d.Observe(kUnder, kBudget, false, 0.0);
	EXPECT_FALSE(d.IsArmed());
	d.Observe(kOver, kBudget, false, 0.0); // interrupts the under streak
	for (int i = 0; i < 29; ++i) d.Observe(kUnder, kBudget, false, 0.0);
	EXPECT_FALSE(d.IsArmed());
	d.Observe(kUnder, kBudget, false, 0.0);
	EXPECT_TRUE(d.IsArmed());
	bool tripped = false;
	for (int i = 0; i < 10; ++i) tripped = d.Observe(kOver, kBudget, false, 0.0).tripped;
	EXPECT_TRUE(tripped);
	EXPECT_EQ(2u, d.GetTripCount());
}

TEST(TripDetector, FirstEntryIsIgnored)
{
	TripDetector d;
	TripResult r = d.Observe(500.0f, kBudget, false, 0.0);
	EXPECT_FALSE(r.tripped);
	EXPECT_EQ(0u, d.GetOverBudgetFrames());
	EXPECT_TRUE(d.Observe(500.0f, kBudget, false, 1.0).tripped); // second entry is normal
}

TEST(TripDetector, StartingModuleEntriesAreSkipped)
{
	TripDetector d;
	Prime(d);
	EXPECT_FALSE(d.Observe(500.0f, kBudget, true, 0.0).tripped);
	EXPECT_EQ(0u, d.GetOverBudgetFrames());
	// Starting entries neither count nor break a run.
	for (int i = 0; i < 5; ++i) d.Observe(kOver, kBudget, false, 0.0);
	for (int i = 0; i < 3; ++i) EXPECT_FALSE(d.Observe(kUnder, kBudget, true, 0.0).tripped);
	for (int i = 0; i < 4; ++i) EXPECT_FALSE(d.Observe(kOver, kBudget, false, 0.0).tripped);
	EXPECT_TRUE(d.Observe(kOver, kBudget, false, 0.0).tripped);
}

TEST(TripDetector, UnderBudgetEntryResetsConsecutiveRun)
{
	TripDetector d;
	Prime(d);
	for (int i = 0; i < 9; ++i) d.Observe(kOver, kBudget, false, 0.0);
	d.Observe(kUnder, kBudget, false, 0.0);
	for (int i = 0; i < 9; ++i) EXPECT_FALSE(d.Observe(kOver, kBudget, false, 0.0).tripped);
	EXPECT_EQ(0u, d.GetTripCount());
}

TEST(TripDetector, BoundaryEqualityIsNotOver)
{
	TripDetector d;
	Prime(d);
	for (int i = 0; i < 50; ++i) EXPECT_FALSE(d.Observe(kBudget, kBudget, false, 0.0).tripped);
	EXPECT_EQ(0u, d.GetOverBudgetFrames());
}

TEST(TripDetector, NoBudgetNeverTrips)
{
	TripDetector d;
	Prime(d);
	for (int i = 0; i < 100; ++i) EXPECT_FALSE(d.Observe(1000.0f, 0.0f, false, 0.0).tripped);
	for (int i = 0; i < 100; ++i) EXPECT_FALSE(d.Observe(1000.0f, -1.0f, false, 0.0).tripped);
	EXPECT_EQ(0u, d.GetOverBudgetFrames());
	EXPECT_EQ(0u, d.GetTripCount());
}

TEST(TripDetector, BudgetScaleMultipliesBudget)
{
	TripDetector d;
	TripConfig c; c.budgetScale = 2.0f; d.SetConfig(c);
	Prime(d);
	for (int i = 0; i < 20; ++i) EXPECT_FALSE(d.Observe(kOver, kBudget, false, 0.0).tripped); // 12 < 20
	EXPECT_EQ(0u, d.GetOverBudgetFrames());
}

TEST(TripDetector, NonPositiveBudgetScaleTreatedAsOne)
{
	const float scales[] = { 0.0f, -3.0f };
	for (float scale : scales)
	{
		TripDetector d;
		TripConfig c; c.budgetScale = scale; d.SetConfig(c);
		Prime(d);
		bool tripped = false;
		for (int i = 0; i < 10; ++i) tripped = d.Observe(kOver, kBudget, false, 0.0).tripped;
		EXPECT_TRUE(tripped) << scale;
	}
}
