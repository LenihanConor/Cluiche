////////////////////////////////////////////////////////////////////////////////
// Filename: TestPerfTimingStats.cpp
// GoogleTest suite - Perf Inspector task 3: EWMA / p95 of a coarse series (perf.timing).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaObservation/Profile/CoarseRing.h>
#include <DiaObservation/Profile/PerfTimingStats.h>

#include <vector>

using namespace Dia::Observation::Profile;

namespace
{
	PerfSeriesStats Stats(const std::vector<uint16_t>& v, const std::vector<uint8_t>* flags = nullptr)
	{
		PerfSeriesStats s;
		std::vector<uint16_t> scratch;
		ComputePerfSeriesStats(v.data(), flags ? flags->data() : nullptr, static_cast<uint32_t>(v.size()), s, scratch);
		return s;
	}
}

TEST(PerfTimingStats, Empty_IsZeroSamples)
{
	const PerfSeriesStats s = Stats({});
	EXPECT_EQ(0u, s.samples);
	EXPECT_FLOAT_EQ(0.0f, s.ewmaMs);
	EXPECT_FLOAT_EQ(0.0f, s.p95Ms);
}

TEST(PerfTimingStats, SingleEntry_EwmaAndP95AreThatValue)
{
	const PerfSeriesStats s = Stats({ 250 });
	EXPECT_EQ(1u, s.samples);
	EXPECT_NEAR(2.5f, s.ewmaMs, 1e-5f);
	EXPECT_NEAR(2.5f, s.p95Ms, 1e-5f);
}

TEST(PerfTimingStats, P95_IsNearestRank)
{
	std::vector<uint16_t> v;
	for (uint16_t i = 1; i <= 20; ++i) v.push_back(static_cast<uint16_t>(i * 100));   // 1..20 ms
	// n=20: rank = ceil(19) = 19 -> 19 ms
	EXPECT_NEAR(19.0f, Stats(v).p95Ms, 1e-4f);
	v.push_back(2100);   // n=21: rank = ceil(19.95) = 20 -> 20 ms
	EXPECT_NEAR(20.0f, Stats(v).p95Ms, 1e-4f);
}

TEST(PerfTimingStats, Ewma_SeedsWithFirstAndFollowsAlpha)
{
	// 1.00 then 2.00: ewma = 1 + 0.2 * (2 - 1) = 1.2
	const PerfSeriesStats s = Stats({ 100, 200 });
	EXPECT_NEAR(1.2f, s.ewmaMs, 1e-5f);
	// Constant input stays constant.
	EXPECT_NEAR(3.0f, Stats({ 300, 300, 300, 300 }).ewmaMs, 1e-5f);
}

TEST(PerfTimingStats, ClampedValueCountsAtItsStoredValue)
{
	const PerfSeriesStats s = Stats({ CoarseRing::kClampedValue });
	EXPECT_EQ(1u, s.samples);
	EXPECT_NEAR(655.35f, s.p95Ms, 1e-2f);
}

TEST(PerfTimingStats, GapFlaggedEntriesAreIgnoredNotCounted)
{
	const std::vector<uint16_t> v     = { 100, 0, 0, 300 };
	const std::vector<uint8_t>  flags = { 0, CoarseRing::kFlagGap, CoarseRing::kFlagGap, 0 };
	const PerfSeriesStats s = Stats(v, &flags);
	EXPECT_EQ(2u, s.samples);
	EXPECT_NEAR(1.4f, s.ewmaMs, 1e-5f);   // 1 + 0.2 * (3 - 1)
	EXPECT_NEAR(3.0f, s.p95Ms, 1e-5f);
}

TEST(PerfTimingStats, AllGap_IsZeroSamples)
{
	const std::vector<uint16_t> v     = { 0, 0 };
	const std::vector<uint8_t>  flags = { CoarseRing::kFlagGap, CoarseRing::kFlagGap };
	EXPECT_EQ(0u, Stats(v, &flags).samples);
}

TEST(PerfTimingStats, ClampedFlagDoesNotExcludeEntry)
{
	const std::vector<uint16_t> v     = { 100 };
	const std::vector<uint8_t>  flags = { CoarseRing::kFlagClamped };
	EXPECT_EQ(1u, Stats(v, &flags).samples);
}

TEST(PerfTimingStats, Ring_UsesOnlyNewestWindow)
{
	CoarseRing ring;
	ring.Init(600, 2);
	// 100 old entries at 10 ms module 0, then 120 recent entries at 1 ms (60 Hz * 2 s).
	for (int i = 0; i < 100; ++i) { ring.RecordModule(0, 0xAu, 10.0f); ring.RecordUnitTick(12.0f); ring.CommitEntry(); }
	for (int i = 0; i < 120; ++i) { ring.RecordModule(0, 0xAu, 1.0f);  ring.RecordUnitTick(2.0f);  ring.CommitEntry(); }

	PerfTimingScratch scratch;
	PerfSeriesStats mod, unit;
	ASSERT_TRUE(ComputePerfRingSeriesStats(ring, 0, 60.0f, 2.0f, mod, scratch));
	ASSERT_TRUE(ComputePerfRingSeriesStats(ring, CoarseRing::kUnitSlot, 60.0f, 2.0f, unit, scratch));
	EXPECT_EQ(120u, mod.samples);
	EXPECT_NEAR(1.0f, mod.ewmaMs, 1e-3f);
	EXPECT_NEAR(1.0f, mod.p95Ms, 1e-3f);
	EXPECT_NEAR(2.0f, unit.p95Ms, 1e-3f);
}

TEST(PerfTimingStats, Ring_FewerEntriesThanWindow_UsesWhatExists)
{
	CoarseRing ring;
	ring.Init(600, 1);
	for (int i = 0; i < 5; ++i) { ring.RecordModule(0, 0xAu, 4.0f); ring.RecordUnitTick(5.0f); ring.CommitEntry(); }
	PerfTimingScratch scratch;
	PerfSeriesStats s;
	ASSERT_TRUE(ComputePerfRingSeriesStats(ring, 0, 60.0f, 2.0f, s, scratch));
	EXPECT_EQ(5u, s.samples);
}

TEST(PerfTimingStats, Ring_EmptyOrInert_ReturnsFalse)
{
	CoarseRing ring;
	PerfTimingScratch scratch;
	PerfSeriesStats s;
	EXPECT_FALSE(ComputePerfRingSeriesStats(ring, 0, 60.0f, 2.0f, s, scratch));   // inert
	ring.Init(60, 1);
	EXPECT_FALSE(ComputePerfRingSeriesStats(ring, 0, 60.0f, 2.0f, s, scratch));   // no entries
	EXPECT_EQ(0u, s.samples);
}
