#include <gtest/gtest.h>
#include <DiaObservation/Profile/UnitTimingTable.h>

using Dia::Observation::Profile::UnitTimingTable;

TEST(UnitTimingTable, EmptyTableReturnsZeros)
{
	UnitTimingTable t;
	EXPECT_EQ(0u, t.GetEntryCount());
	EXPECT_EQ(0u, t.GetSampleCount(0));
	EXPECT_FLOAT_EQ(0.0f, t.GetLast(0));
	EXPECT_FLOAT_EQ(0.0f, t.GetEwma(0));
	EXPECT_FLOAT_EQ(0.0f, t.GetP50(0));
	EXPECT_FLOAT_EQ(0.0f, t.GetP95(0));
	EXPECT_FLOAT_EQ(0.0f, t.GetMax(0));
	EXPECT_FLOAT_EQ(0.0f, t.GetUnitLast());
	float buf[4];
	EXPECT_EQ(0u, t.CopyRecent(0, buf, 4));
}

TEST(UnitTimingTable, LastEwmaMaxOnKnownSequence)
{
	UnitTimingTable t;
	t.RecordModule(3, 0xABCDu, 10.0f);
	EXPECT_FLOAT_EQ(10.0f, t.GetEwma(3));   // first sample seeds the EWMA
	t.RecordModule(3, 0xABCDu, 20.0f);
	EXPECT_FLOAT_EQ(20.0f, t.GetLast(3));
	EXPECT_NEAR(10.0f + 0.2f * 10.0f, t.GetEwma(3), 1e-4f);
	EXPECT_FLOAT_EQ(20.0f, t.GetMax(3));
	EXPECT_EQ(2u, t.GetSampleCount(3));
	EXPECT_EQ(0xABCDu, t.GetModuleCrc(3));
	EXPECT_EQ(0u, t.GetSampleCount(2));
}

TEST(UnitTimingTable, PercentilesNearestRank)
{
	UnitTimingTable t;
	for (int i = 1; i <= 100; ++i) t.RecordModule(0, 1u, static_cast<float>(i));
	EXPECT_FLOAT_EQ(50.0f, t.GetP50(0));
	EXPECT_FLOAT_EQ(95.0f, t.GetP95(0));
	EXPECT_FLOAT_EQ(100.0f, t.GetMax(0));
}

TEST(UnitTimingTable, PercentilesIgnoreInsertionOrder)
{
	UnitTimingTable t;
	const float v[5] = { 5, 1, 4, 2, 3 };
	for (float f : v) t.RecordModule(0, 1u, f);
	EXPECT_FLOAT_EQ(3.0f, t.GetP50(0));
	EXPECT_FLOAT_EQ(5.0f, t.GetP95(0));
}

TEST(UnitTimingTable, RingWrapsPast128)
{
	UnitTimingTable t;
	t.RecordModule(0, 1u, 1000.0f);               // will be evicted
	for (int i = 0; i < 128; ++i) t.RecordModule(0, 1u, 1.0f);
	EXPECT_EQ(129u, t.GetSampleCount(0));
	EXPECT_FLOAT_EQ(1.0f, t.GetMax(0));            // spike left the window
	float buf[200];
	EXPECT_EQ(128u, t.CopyRecent(0, buf, 200));
}

TEST(UnitTimingTable, OutOfRangeSlot)
{
	UnitTimingTable t;
	EXPECT_FALSE(t.RecordModule(UnitTimingTable::kMaxModules, 1u, 5.0f));
	EXPECT_FALSE(t.RecordModule(0xFFFFFFFFu, 1u, 5.0f));
	EXPECT_FLOAT_EQ(0.0f, t.GetLast(UnitTimingTable::kMaxModules + 1));
	EXPECT_EQ(0u, t.GetSampleCount(1000));
	EXPECT_EQ(0u, t.GetModuleCrc(1000));
	float buf[2];
	EXPECT_EQ(0u, t.CopyRecent(1000, buf, 2));
	EXPECT_EQ(0u, t.GetUnitSampleCount());   // out-of-range writes did not leak into the unit series
}

TEST(UnitTimingTable, CapacityLimitLastSlotValid)
{
	UnitTimingTable t;
	EXPECT_TRUE(t.RecordModule(UnitTimingTable::kMaxModules - 1, 7u, 2.5f));
	EXPECT_FLOAT_EQ(2.5f, t.GetLast(UnitTimingTable::kMaxModules - 1));
	EXPECT_EQ(64u, UnitTimingTable::kMaxModules);
}

TEST(UnitTimingTable, UnitTickIsIndependentSeries)
{
	UnitTimingTable t;
	t.RecordUnitTick(4.0f);
	t.RecordUnitTick(8.0f);
	t.CommitEntry();
	t.CommitEntry();
	EXPECT_EQ(2u, t.GetEntryCount());
	EXPECT_FLOAT_EQ(8.0f, t.GetUnitLast());
	EXPECT_FLOAT_EQ(8.0f, t.GetUnitMax());
	EXPECT_FLOAT_EQ(4.0f, t.GetUnitP50());
	EXPECT_NEAR(4.8f, t.GetUnitEwma(), 1e-4f);
	EXPECT_EQ(2u, t.GetUnitSampleCount());
	EXPECT_EQ(0u, t.GetSampleCount(0));
}

TEST(UnitTimingTable, CopyRecentNewestLastAfterWrap)
{
	UnitTimingTable t;
	for (int i = 0; i < 200; ++i) t.RecordModule(5, 1u, static_cast<float>(i));
	float out[60];
	ASSERT_EQ(60u, t.CopyRecent(5, out, 60));
	for (uint32_t i = 0; i < 60; ++i) EXPECT_FLOAT_EQ(static_cast<float>(140 + i), out[i]);

	UnitTimingTable u;
	u.RecordUnitTick(1.0f); u.RecordUnitTick(2.0f); u.RecordUnitTick(3.0f);
	float o2[60];
	ASSERT_EQ(3u, u.CopyRecentUnit(o2, 60));
	EXPECT_FLOAT_EQ(1.0f, o2[0]);
	EXPECT_FLOAT_EQ(3.0f, o2[2]);
	ASSERT_EQ(2u, u.CopyRecentUnit(o2, 2));
	EXPECT_FLOAT_EQ(2.0f, o2[0]);
	EXPECT_FLOAT_EQ(3.0f, o2[1]);
}

TEST(UnitTimingTable, ResetClears)
{
	UnitTimingTable t;
	t.RecordModule(0, 9u, 3.0f); t.RecordUnitTick(1.0f); t.CommitEntry();
	t.Reset();
	EXPECT_EQ(0u, t.GetEntryCount());
	EXPECT_EQ(0u, t.GetSampleCount(0));
	EXPECT_EQ(0u, t.GetUnitSampleCount());
	EXPECT_EQ(0u, t.GetModuleCrc(0));
}

// Audit (task 11): a unit with no modules still records its own tick series.
TEST(UnitTimingTable, ZeroModulesUnitTickOnly)
{
	UnitTimingTable t;
	for (int i = 0; i < 5; ++i)
	{
		t.RecordUnitTick(2.0f + static_cast<float>(i));
		t.CommitEntry();
	}
	EXPECT_EQ(5u, t.GetEntryCount());
	EXPECT_EQ(5u, t.GetUnitSampleCount());
	EXPECT_FLOAT_EQ(6.0f, t.GetUnitLast());
	EXPECT_FLOAT_EQ(6.0f, t.GetUnitMax());
	for (uint32_t s = 0; s < UnitTimingTable::kMaxModules; ++s)
		EXPECT_EQ(0u, t.GetSampleCount(s));
}

// Audit (task 11): all 64 module slots plus the unit series are independent and none bleeds into another.
TEST(UnitTimingTable, FullSixtyFourModulesIndependent)
{
	UnitTimingTable t;
	for (int pass = 0; pass < 3; ++pass)
	{
		for (uint32_t s = 0; s < UnitTimingTable::kMaxModules; ++s)
			EXPECT_TRUE(t.RecordModule(s, 1000u + s, static_cast<float>(s) + 1.0f));
		t.RecordUnitTick(500.0f);
		t.CommitEntry();
	}
	EXPECT_FALSE(t.RecordModule(UnitTimingTable::kMaxModules, 1u, 1.0f));
	for (uint32_t s = 0; s < UnitTimingTable::kMaxModules; ++s)
	{
		EXPECT_EQ(3u, t.GetSampleCount(s));
		EXPECT_FLOAT_EQ(static_cast<float>(s) + 1.0f, t.GetLast(s));
		EXPECT_FLOAT_EQ(static_cast<float>(s) + 1.0f, t.GetMax(s));
		EXPECT_EQ(1000u + s, t.GetModuleCrc(s));
	}
	EXPECT_EQ(3u, t.GetUnitSampleCount());
	EXPECT_FLOAT_EQ(500.0f, t.GetUnitLast());
}
