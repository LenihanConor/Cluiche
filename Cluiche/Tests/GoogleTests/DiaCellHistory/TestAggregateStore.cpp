#include <gtest/gtest.h>
#include <DiaCellHistory/AggregateStore.h>
#include <DiaCellHistory/Testing/BudgetSizing.h>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace Dia::CellHistory;
using Dia::Core::StringCRC;
namespace T = Dia::CellHistory::Testing;

namespace
{
	const StringCRC kTemp("temp");
	const StringCRC kBiome("biome");
	const StringCRC kWorld = AggregateStore::WorldGroup();
	const StringCRC kG0("region=0");
	const StringCRC kG1("region=1");
	const float kNaN = std::nanf("");
	constexpr uint64_t kBig = 1u << 20;

	// 6 cells. Group A = cells 0,1,2 (1,2,6). Group B = cells 3,4,5 (10,NaN,4).
	const float kTempVals[6] = { 1.f, 2.f, 6.f, 10.f, kNaN, 4.f };
	const uint8_t kMaskA[6] = { 1, 1, 1, 0, 0, 0 };
	const uint8_t kMaskB[6] = { 0, 0, 0, 1, 1, 1 };

	void Layout(AggregateStore& s)
	{
		ASSERT_TRUE(s.AddLayer(kTemp, AggregateKind::Float));
		ASSERT_TRUE(s.AddLayer(kBiome, AggregateKind::Category, 3));
		ASSERT_TRUE(s.AddGroup(kG0));
		ASSERT_TRUE(s.AddGroup(kG1));
	}
	void Fill(AggregateStore& s, uint32_t sample, const uint8_t* m0, const uint8_t* m1)
	{
		ASSERT_TRUE(s.BeginSample(sample));
		ASSERT_TRUE(s.AccumulateFloat(kTemp, kWorld, kTempVals, nullptr, 6));
		ASSERT_TRUE(s.AccumulateFloat(kTemp, kG0, kTempVals, m0, 6));
		ASSERT_TRUE(s.AccumulateFloat(kTemp, kG1, kTempVals, m1, 6));
		ASSERT_TRUE(s.SetGroupSize(kWorld, 6));
		uint16_t n0 = 0, n1 = 0;
		for (int i = 0; i < 6; ++i) { n0 += m0[i] ? 1 : 0; n1 += m1[i] ? 1 : 0; }
		ASSERT_TRUE(s.SetGroupSize(kG0, n0));
		ASSERT_TRUE(s.SetGroupSize(kG1, n1));
	}
	uint64_t RowBytes() { return T::AggregateRowBytes(1, 3, 3); }
}

TEST(AggregateStore, WorldGroupAlwaysExists)
{
	AggregateStore s(kBig);
	EXPECT_EQ(1u, s.GroupCount());
	EXPECT_FALSE(s.AddGroup(kWorld));
}

TEST(AggregateStore, FloatMinMeanMaxPerGroup)
{
	AggregateStore s(kBig);
	Layout(s);
	Fill(s, 0, kMaskA, kMaskB);
	AggregateStats a;
	ASSERT_TRUE(s.Aggregate(0, kTemp, kG0, a));
	EXPECT_FLOAT_EQ(1.f, a.mMin); EXPECT_FLOAT_EQ(3.f, a.mMean); EXPECT_FLOAT_EQ(6.f, a.mMax);
	ASSERT_TRUE(s.Aggregate(0, kTemp, kG1, a));	// NaN excluded: 10, 4
	EXPECT_FLOAT_EQ(4.f, a.mMin); EXPECT_FLOAT_EQ(7.f, a.mMean); EXPECT_FLOAT_EQ(10.f, a.mMax);
	ASSERT_TRUE(s.Aggregate(0, kTemp, kWorld, a));	// 1,2,6,10,4 -> mean 4.6
	EXPECT_FLOAT_EQ(1.f, a.mMin); EXPECT_NEAR(4.6f, a.mMean, 1e-5f); EXPECT_FLOAT_EQ(10.f, a.mMax);
}

TEST(AggregateStore, AllNaNGroupIsNaNStats)
{
	AggregateStore s(kBig);
	Layout(s);
	ASSERT_TRUE(s.BeginSample(0));
	const float v[3] = { kNaN, kNaN, kNaN };
	ASSERT_TRUE(s.AccumulateFloat(kTemp, kG0, v, nullptr, 3));
	AggregateStats a;
	ASSERT_TRUE(s.Aggregate(0, kTemp, kG0, a));
	EXPECT_TRUE(std::isnan(a.mMin)); EXPECT_TRUE(std::isnan(a.mMean)); EXPECT_TRUE(std::isnan(a.mMax));
}

TEST(AggregateStore, EmptyGroupIsNaNStatsAndSizeZero)
{
	AggregateStore s(kBig);
	Layout(s);
	const uint8_t none[6] = { 0, 0, 0, 0, 0, 0 };
	Fill(s, 0, none, kMaskB);
	AggregateStats a;
	ASSERT_TRUE(s.Aggregate(0, kTemp, kG0, a));
	EXPECT_TRUE(std::isnan(a.mMin)); EXPECT_TRUE(std::isnan(a.mMean)); EXPECT_TRUE(std::isnan(a.mMax));
	uint16_t sz = 99;
	ASSERT_TRUE(s.GroupSize(0, kG0, sz));
	EXPECT_EQ(0u, sz);
}

TEST(AggregateStore, CategoryCountsPerEnumValuePerGroup)
{
	AggregateStore s(kBig);
	Layout(s);
	ASSERT_TRUE(s.BeginSample(0));
	const float ids[7] = { 0.f, 1.f, 1.f, 2.f, 2.f, 2.f, 1.f };
	const uint8_t m[7] = { 1, 1, 1, 1, 0, 0, 0 };
	ASSERT_TRUE(s.AccumulateCategory(kBiome, kG0, ids, m, 7));
	ASSERT_TRUE(s.AccumulateCategory(kBiome, kWorld, ids, nullptr, 7));
	uint16_t c[3] = { 9, 9, 9 };
	ASSERT_TRUE(s.CategoryCounts(0, kBiome, kG0, c, 3));
	EXPECT_EQ(1u, c[0]); EXPECT_EQ(2u, c[1]); EXPECT_EQ(1u, c[2]);
	ASSERT_TRUE(s.CategoryCounts(0, kBiome, kWorld, c, 3));
	EXPECT_EQ(1u, c[0]); EXPECT_EQ(3u, c[1]); EXPECT_EQ(3u, c[2]);
}

TEST(AggregateStore, CategoryIgnoresNaNAndOutOfRange)
{
	AggregateStore s(kBig);
	Layout(s);
	ASSERT_TRUE(s.BeginSample(0));
	const float ids[5] = { 0.f, kNaN, 3.f, -1.f, 2.f };
	ASSERT_TRUE(s.AccumulateCategory(kBiome, kWorld, ids, nullptr, 5));
	uint16_t c[3];
	ASSERT_TRUE(s.CategoryCounts(0, kBiome, kWorld, c, 3));
	EXPECT_EQ(1u, c[0]); EXPECT_EQ(0u, c[1]); EXPECT_EQ(1u, c[2]);
	EXPECT_FALSE(s.CategoryCounts(0, kBiome, kWorld, c, 2));	// n < enumCount
}

TEST(AggregateStore, WrongKindFails)
{
	AggregateStore s(kBig);
	Layout(s);
	Fill(s, 0, kMaskA, kMaskB);
	AggregateStats a;
	uint16_t c[3];
	EXPECT_FALSE(s.Aggregate(0, kBiome, kG0, a));
	EXPECT_FALSE(s.CategoryCounts(0, kTemp, kG0, c, 3));
	EXPECT_FALSE(s.AccumulateCategory(kTemp, kG0, kTempVals, nullptr, 6));
	EXPECT_FALSE(s.AccumulateFloat(kBiome, kG0, kTempVals, nullptr, 6));
}

TEST(AggregateStore, GroupSizesStoredPerSample)
{
	AggregateStore s(kBig);
	Layout(s);
	Fill(s, 0, kMaskA, kMaskB);
	const uint8_t all[6] = { 1, 1, 1, 1, 1, 0 };
	Fill(s, 1, all, kMaskB);
	uint16_t sz = 0;
	ASSERT_TRUE(s.GroupSize(0, kWorld, sz)); EXPECT_EQ(6u, sz);
	ASSERT_TRUE(s.GroupSize(0, kG0, sz)); EXPECT_EQ(3u, sz);
	ASSERT_TRUE(s.GroupSize(1, kG0, sz)); EXPECT_EQ(5u, sz);
	ASSERT_TRUE(s.GroupSize(1, kG1, sz)); EXPECT_EQ(3u, sz);
}

TEST(AggregateStore, ChangingGroupingMatchesMembershipPerSample)
{
	AggregateStore s(kBig);
	Layout(s);
	Fill(s, 0, kMaskA, kMaskB);
	Fill(s, 1, kMaskB, kMaskA);	// groups swap cells
	AggregateStats a;
	ASSERT_TRUE(s.Aggregate(0, kTemp, kG0, a)); EXPECT_FLOAT_EQ(3.f, a.mMean);
	ASSERT_TRUE(s.Aggregate(1, kTemp, kG0, a)); EXPECT_FLOAT_EQ(7.f, a.mMean);
	ASSERT_TRUE(s.Aggregate(0, kTemp, kG1, a)); EXPECT_FLOAT_EQ(7.f, a.mMean);
	ASSERT_TRUE(s.Aggregate(1, kTemp, kG1, a)); EXPECT_FLOAT_EQ(3.f, a.mMean);
}

TEST(AggregateStore, SetFloatLayerRowRoundTrips)
{
	AggregateStore s(kBig);
	Layout(s);
	ASSERT_TRUE(s.BeginSample(5));
	AggregateStats in; in.mMin = -1.f; in.mMean = 0.5f; in.mMax = 9.f;
	ASSERT_TRUE(s.SetFloatLayerRow(kTemp, kG1, in));
	AggregateStats out;
	ASSERT_TRUE(s.Aggregate(5, kTemp, kG1, out));
	EXPECT_FLOAT_EQ(-1.f, out.mMin); EXPECT_FLOAT_EQ(0.5f, out.mMean); EXPECT_FLOAT_EQ(9.f, out.mMax);
}

TEST(AggregateStore, UnwrittenRowsAreNotRecorded)
{
	AggregateStore s(kBig);
	Layout(s);
	ASSERT_TRUE(s.BeginSample(0));
	ASSERT_TRUE(s.AccumulateFloat(kTemp, kG0, kTempVals, nullptr, 6));
	AggregateStats a;
	uint16_t sz, c[3];
	EXPECT_TRUE(s.Aggregate(0, kTemp, kG0, a));
	EXPECT_FALSE(s.Aggregate(0, kTemp, kG1, a));
	EXPECT_FALSE(s.CategoryCounts(0, kBiome, kG0, c, 3));
	EXPECT_FALSE(s.GroupSize(0, kG0, sz));
	EXPECT_FALSE(s.Aggregate(0, StringCRC("nolayer"), kG0, a));
	EXPECT_FALSE(s.Aggregate(0, kTemp, StringCRC("nogroup"), a));
	EXPECT_FALSE(s.Aggregate(7, kTemp, kG0, a));
}

TEST(AggregateStore, WritesWithoutOpenSampleFail)
{
	AggregateStore s(kBig);
	Layout(s);
	EXPECT_FALSE(s.AccumulateFloat(kTemp, kG0, kTempVals, nullptr, 6));
	EXPECT_FALSE(s.SetGroupSize(kG0, 1));
	EXPECT_FALSE(s.SetFloatLayerRow(kTemp, kG0, AggregateStats()));
}

TEST(AggregateStore, CopyRowFromReusesPreviousRow)
{
	AggregateStore s(kBig);
	Layout(s);
	Fill(s, 0, kMaskA, kMaskB);
	ASSERT_TRUE(s.AccumulateCategory(kBiome, kG0, kTempVals, kMaskA, 6));
	ASSERT_TRUE(s.BeginSample(1));
	ASSERT_TRUE(s.CopyRowFrom(0, kTemp));
	AggregateStats a, b;
	ASSERT_TRUE(s.Aggregate(1, kTemp, kG0, a));
	ASSERT_TRUE(s.Aggregate(1, kTemp, kG1, b));
	EXPECT_FLOAT_EQ(3.f, a.mMean);
	EXPECT_FLOAT_EQ(7.f, b.mMean);
	uint16_t c[3];
	EXPECT_FALSE(s.CategoryCounts(1, kBiome, kG0, c, 3));	// other layers not copied
	EXPECT_FALSE(s.CopyRowFrom(0, StringCRC("nolayer")));
	EXPECT_FALSE(s.CopyRowFrom(99, kTemp));
	EXPECT_FALSE(s.CopyRowFrom(0, kBiome));	// source only partly recorded
}

TEST(AggregateStore, CopyRowFromFalseAfterSourceEvicted)
{
	AggregateStore s(2 * RowBytes());
	Layout(s);
	Fill(s, 0, kMaskA, kMaskB);
	Fill(s, 1, kMaskA, kMaskB);
	ASSERT_TRUE(s.BeginSample(2));	// evicts 0
	EXPECT_FALSE(s.CopyRowFrom(0, kTemp));
	EXPECT_TRUE(s.CopyRowFrom(1, kTemp));
}

TEST(AggregateStore, BudgetHoldsExactlyNRows)
{
	AggregateStore s(T::BudgetBytesForWindow(4, RowBytes()));
	Layout(s);
	for (uint32_t i = 0; i < 10; ++i) { Fill(s, i, kMaskA, kMaskB); }
	EXPECT_EQ(4u, s.WindowLength());
	EXPECT_EQ(4u, s.RetainedCount());
	EXPECT_EQ(6u, s.FirstRetained());
	EXPECT_EQ(9u, s.LatestSample());
	EXPECT_EQ(RowBytes(), s.BytesPerSample());
	EXPECT_LE(s.BytesUsed(), s.Budget());
	EXPECT_DOUBLE_EQ(1.0, s.UsedFraction());
}

TEST(AggregateStore, EvictionOldestFirstMergedGapOneEpisode)
{
	AggregateStore s(T::BudgetBytesForWindow(3, RowBytes()));
	Layout(s);
	for (uint32_t i = 0; i < 3; ++i) { Fill(s, i, kMaskA, kMaskB); }
	EXPECT_EQ(0u, s.GapCount());
	for (uint32_t i = 3; i < 7; ++i) { Fill(s, i, kMaskA, kMaskB); }
	EXPECT_EQ(1u, s.EvictionEpisodeCount());
	ASSERT_EQ(1u, s.GapCount());
	EXPECT_EQ(0u, s.Gap(0).mFirstDropped);
	EXPECT_EQ(3u, s.Gap(0).mLastDropped);
	AggregateStats a;
	EXPECT_FALSE(s.Aggregate(0, kTemp, kG0, a));
	EXPECT_FALSE(s.HasSample(3));
	EXPECT_TRUE(s.Aggregate(4, kTemp, kG0, a));
	EXPECT_EQ(0u, s.Gap(5).mFirstDropped);	// out of range is zeroed
}

TEST(AggregateStore, UsedFractionTracksRetainedRows)
{
	AggregateStore s(T::BudgetBytesForWindow(4, RowBytes()));
	Layout(s);
	EXPECT_DOUBLE_EQ(0.0, s.UsedFraction());
	Fill(s, 0, kMaskA, kMaskB);
	Fill(s, 1, kMaskA, kMaskB);
	EXPECT_DOUBLE_EQ(0.5, s.UsedFraction());
}

TEST(AggregateStore, NotEvenOneRowFitsFails)
{
	AggregateStore s(RowBytes() - 1);
	Layout(s);
	EXPECT_FALSE(s.BeginSample(0));
	EXPECT_TRUE(s.Empty());
}

TEST(AggregateStore, NoLayersCannotBegin)
{
	AggregateStore s(kBig);
	EXPECT_FALSE(s.BeginSample(0));
}

TEST(AggregateStore, TruncateAfterDropsLaterAndAllowsReappend)
{
	AggregateStore s(kBig);
	Layout(s);
	for (uint32_t i = 0; i < 5; ++i) { Fill(s, i, kMaskA, kMaskB); }
	s.TruncateAfter(2);
	EXPECT_EQ(3u, s.RetainedCount());
	EXPECT_EQ(2u, s.LatestSample());
	EXPECT_FALSE(s.HasSample(3));
	AggregateStats a;
	EXPECT_FALSE(s.Aggregate(4, kTemp, kG0, a));
	Fill(s, 3, kMaskB, kMaskA);	// re-append a truncated index with new data
	ASSERT_TRUE(s.Aggregate(3, kTemp, kG0, a));
	EXPECT_FLOAT_EQ(7.f, a.mMean);
	s.TruncateAfter(100);	// no-op
	EXPECT_EQ(4u, s.RetainedCount());
	s.TruncateAfter(0);
	EXPECT_EQ(1u, s.RetainedCount());
}

TEST(AggregateStore, TruncateEverythingLeavesEmptyUsableStore)
{
	AggregateStore s(kBig);
	Layout(s);
	Fill(s, 5, kMaskA, kMaskB);
	s.TruncateAfter(2);
	EXPECT_TRUE(s.Empty());
	EXPECT_EQ(0u, s.FirstRetained());
	Fill(s, 1, kMaskA, kMaskB);
	EXPECT_TRUE(s.HasSample(1));
}

TEST(AggregateStore, NonContiguousIndicesAndStrictlyIncreasing)
{
	AggregateStore s(kBig);
	Layout(s);
	Fill(s, 2, kMaskA, kMaskB);
	Fill(s, 10, kMaskB, kMaskA);
	Fill(s, 500, kMaskA, kMaskB);
	EXPECT_FALSE(s.BeginSample(500));
	EXPECT_FALSE(s.BeginSample(7));
	EXPECT_TRUE(s.HasSample(10));
	EXPECT_FALSE(s.HasSample(11));
	AggregateStats a;
	ASSERT_TRUE(s.Aggregate(10, kTemp, kG0, a)); EXPECT_FLOAT_EQ(7.f, a.mMean);
	ASSERT_TRUE(s.Aggregate(500, kTemp, kG0, a)); EXPECT_FLOAT_EQ(3.f, a.mMean);
	EXPECT_EQ(3u, s.RetainedCount());
}

TEST(AggregateStore, LayoutFrozenAfterFirstSample)
{
	AggregateStore s(kBig);
	Layout(s);
	Fill(s, 0, kMaskA, kMaskB);
	EXPECT_FALSE(s.AddLayer(StringCRC("late"), AggregateKind::Float));
	EXPECT_FALSE(s.AddGroup(StringCRC("region=2")));
	s.TruncateAfter(0);
	s.TruncateAfter(0);
	EXPECT_FALSE(s.AddGroup(StringCRC("region=2")));
}

TEST(AggregateStore, LayoutRejectsBadInput)
{
	AggregateStore s(kBig);
	ASSERT_TRUE(s.AddLayer(kTemp, AggregateKind::Float));
	EXPECT_FALSE(s.AddLayer(kTemp, AggregateKind::Float));
	EXPECT_FALSE(s.AddLayer(kBiome, AggregateKind::Category, 0));
	EXPECT_FALSE(s.AddLayer(kBiome, AggregateKind::Category, 257));
	ASSERT_TRUE(s.AddGroup(kG0));
	EXPECT_FALSE(s.AddGroup(kG0));
	EXPECT_EQ(1u, s.LayerCount());
}

TEST(AggregateStore, LayerAndGroupCaps)
{
	AggregateStore s(kBig);
	char name[32];
	for (uint32_t i = 0; i < AggregateStore::kMaxLayers; ++i) { std::snprintf(name, sizeof(name), "L%u", i); ASSERT_TRUE(s.AddLayer(StringCRC(name), AggregateKind::Float)); }
	EXPECT_FALSE(s.AddLayer(StringCRC("overflow"), AggregateKind::Float));
	for (uint32_t i = 1; i < AggregateStore::kMaxGroups; ++i) { std::snprintf(name, sizeof(name), "G%u", i); ASSERT_TRUE(s.AddGroup(StringCRC(name))); }
	EXPECT_FALSE(s.AddGroup(StringCRC("overflow")));
}

TEST(AggregateStore, TwentyLayersTwentySevenGroupsCost)
{
	AggregateStore s(64ull * 1024 * 1024);
	char name[32];
	for (uint32_t i = 0; i < 20; ++i) { std::snprintf(name, sizeof(name), "L%u", i); ASSERT_TRUE(s.AddLayer(StringCRC(name), AggregateKind::Float)); }
	for (uint32_t i = 1; i < 27; ++i) { std::snprintf(name, sizeof(name), "G%u", i); ASSERT_TRUE(s.AddGroup(StringCRC(name))); }
	ASSERT_TRUE(s.BeginSample(0));
	EXPECT_EQ(T::AggregateRowBytes(20, 0, 27), s.BytesPerSample());
	EXPECT_NEAR(6534.0, double(s.BytesPerSample()), 100.0);	// ~6.5 KB
	EXPECT_GT(s.WindowLength(), 9000u);
}

TEST(AggregateStore, ClearDropsEverythingKeepsTheLayoutAndRestartsNumbering)
{
	AggregateStore s(kBig);
	Layout(s);
	for (uint32_t i = 0; i < 4; ++i) { Fill(s, i, kMaskA, kMaskB); }
	s.Clear();
	EXPECT_TRUE(s.Empty());
	EXPECT_EQ(s.GapCount(), 0u);
	EXPECT_EQ(s.LayerCount(), 2u);
	EXPECT_EQ(s.GroupCount(), 3u);
	EXPECT_FALSE(s.HasSample(0));
	Fill(s, 0, kMaskA, kMaskB);
	EXPECT_TRUE(s.HasSample(0));
	EXPECT_EQ(s.RetainedCount(), 1u);
}
