#include "HistoryTestRig.h"
#include <DiaCellHistory/GroupDeltaTable.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellLayers/Testing/LogCapture.h>

#include <cmath>
#include <limits>
#include <memory>
#include <vector>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::CellLayers::Testing::LogCapture;

namespace
{
	const float kNaN = std::numeric_limits<float>::quiet_NaN();

	std::unique_ptr<GroupDeltaTable> NewTable() { return std::make_unique<GroupDeltaTable>(); }

	// ---- pure core: 6 cells; group 0 = cells 0..2, group 1 = cells 3..5 ----
	const float kG[6] = { 0, 0, 0, 1, 1, 1 };
	const float kA[6] = { 1, 2, 3, 10, 10, 10 };
	const float kB[6] = { 2, 4, 6, 13, 13, 13 };

	// ---- builder helpers ----
	void AddLayers(Rig& r)
	{
		r.grid.AddFloatLayer("temp", 10.0f);
		r.grid.AddFloatLayer("wet", 10.0f);
		r.grid.AddCategoryLayer("biome", 0);
		r.grid.AddCategoryLayer("region", 0);
		r.grid.AddCategoryLayer("zone", 0);
		ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100), FloatEntry("wet", 0, 100), EnumEntry("biome", 3), EnumEntry("region", 3), EnumEntry("zone", 3) }));
	}
	void Track(Rig& r, const char* a, const char* b = nullptr, const char* c = nullptr)
	{
		const char* names[3] = { a, b, c };
		r.cfg.mTrackCount = 0;
		for (const char* nm : names)
		{
			if (nm) { r.cfg.mTrack[r.cfg.mTrackCount++] = Dia::Core::Containers::String64("%s", nm); }
		}
	}
	const Dia::CellLayers::CellLayerDesc& Desc(Rig& r, const char* id)
	{
		const Dia::CellLayers::CellLayerDesc* d = r.registry.Find(Dia::Core::StringCRC(id));
		EXPECT_NE(d, nullptr);
		return *d;
	}
	Side S(uint32_t i) { Side s; s.kind = SideKind::Sample; s.sample = i; return s; }
	Side Now() { return Side(); }
	Side Snap() { Side s; s.kind = SideKind::Snapshot; s.snapshot = Dia::Core::Containers::String64("%s", "x"); return s; }
	DeltaSpec Spec(const char* layer, const Side& a, const Side& b, bool groupSideB = true)
	{
		DeltaSpec d; d.layer = Dia::Core::StringCRC(layer); d.a = a; d.b = b; d.groupSideB = groupSideB; return d;
	}
	const GroupDeltaRow* FindRow(const GroupDeltaTable& t, uint32_t layerIdx, uint32_t value)
	{
		for (uint32_t i = 0; i < t.rowCount; ++i)
		{
			if (t.rows[i].groupLayerIndex == layerIdx && t.rows[i].groupValue == value) { return &t.rows[i]; }
		}
		return nullptr;
	}
	// sample 0: temp 10 everywhere; biome 1 on cells 0..5, 0 elsewhere; region 1 on cells 0..5 (never changes).
	// sample 1: temp 30 on cells 0..5 and 12 elsewhere; biome 1 on cells 0..2 only.
	void TwoSamples(Rig& r)
	{
		for (uint32_t c = 0; c < 6; ++c) { r.grid.Set("biome", c, 1.0f); r.grid.Set("region", c, 1.0f); }
		r.grid.BumpVersion("biome"); r.grid.BumpVersion("region");
		r.Take();
		for (uint32_t c = 0; c < 12; ++c) { r.grid.Set("temp", c, c < 6 ? 30.0f : 12.0f); }
		for (uint32_t c = 3; c < 6; ++c) { r.grid.Set("biome", c, 0.0f); }
		r.grid.BumpVersion("temp"); r.grid.BumpVersion("biome");
		r.Take();
	}
}

TEST(GroupDeltaPure, TwoGroupsMeansAndChange)
{
	auto t = NewTable();
	AccumulateGroups(*t, 0, kA, kB, kG, 6, nullptr);
	SortByAbsChange(*t);
	ASSERT_EQ(t->rowCount, 2u);
	EXPECT_EQ(t->rows[0].groupValue, 1u);
	EXPECT_EQ(t->rows[0].count, 3u);
	EXPECT_DOUBLE_EQ(t->rows[0].meanA, 10.0);
	EXPECT_DOUBLE_EQ(t->rows[0].meanB, 13.0);
	EXPECT_DOUBLE_EQ(t->rows[0].change, 3.0);
	EXPECT_EQ(t->rows[1].groupValue, 0u);
	EXPECT_DOUBLE_EQ(t->rows[1].meanA, 2.0);
	EXPECT_DOUBLE_EQ(t->rows[1].meanB, 4.0);
	EXPECT_DOUBLE_EQ(t->rows[1].change, 2.0);
	EXPECT_FALSE(t->truncated);
}

TEST(GroupDeltaPure, ChangeIsBMinusAAndCanBeNegative)
{
	auto t = NewTable();
	AccumulateGroups(*t, 0, kB, kA, kG, 6, nullptr);	// swapped inputs
	SortByAbsChange(*t);
	ASSERT_EQ(t->rowCount, 2u);
	EXPECT_DOUBLE_EQ(t->rows[0].change, -3.0);
	EXPECT_DOUBLE_EQ(t->rows[0].meanA, 13.0);
	EXPECT_DOUBLE_EQ(t->rows[1].change, -2.0);
}

TEST(GroupDeltaPure, SortedByAbsoluteChangeDescendingMixedSigns)
{
	const float g[3] = { 0, 1, 2 };
	const float a[3] = { 0, 0, 0 };
	const float b[3] = { 1, -5, 3 };
	auto t = NewTable();
	AccumulateGroups(*t, 0, a, b, g, 3, nullptr);
	SortByAbsChange(*t);
	ASSERT_EQ(t->rowCount, 3u);
	EXPECT_EQ(t->rows[0].groupValue, 1u);	// |-5|
	EXPECT_EQ(t->rows[1].groupValue, 2u);	// 3
	EXPECT_EQ(t->rows[2].groupValue, 0u);	// 1
}

TEST(GroupDeltaPure, TiesBreakByGroupValueThenLayerIndex)
{
	const float g[2] = { 5, 2 };
	const float a[2] = { 0, 0 };
	const float b[2] = { 2, -2 };
	auto t = NewTable();
	AccumulateGroups(*t, 1, a, b, g, 2, nullptr);	// layer index 1 first
	AccumulateGroups(*t, 0, a, b, g, 2, nullptr);
	SortByAbsChange(*t);
	ASSERT_EQ(t->rowCount, 4u);
	EXPECT_EQ(t->rows[0].groupValue, 2u); EXPECT_EQ(t->rows[0].groupLayerIndex, 0u);
	EXPECT_EQ(t->rows[1].groupValue, 2u); EXPECT_EQ(t->rows[1].groupLayerIndex, 1u);
	EXPECT_EQ(t->rows[2].groupValue, 5u); EXPECT_EQ(t->rows[2].groupLayerIndex, 0u);
	EXPECT_EQ(t->rows[3].groupValue, 5u); EXPECT_EQ(t->rows[3].groupLayerIndex, 1u);
}

TEST(GroupDeltaPure, InvalidGroupValuesExcluded)
{
	EXPECT_TRUE(IsGroupValue(0.0f));
	EXPECT_TRUE(IsGroupValue(255.0f));
	EXPECT_FALSE(IsGroupValue(256.0f));
	EXPECT_FALSE(IsGroupValue(-1.0f));
	EXPECT_FALSE(IsGroupValue(1.5f));
	EXPECT_FALSE(IsGroupValue(kNaN));
	EXPECT_FALSE(IsGroupValue(std::numeric_limits<float>::infinity()));

	const float g[7] = { kNaN, -1.0f, 1.5f, 256.0f, 1000.0f, 255.0f, 3.0f };
	const float a[7] = { 0, 0, 0, 0, 0, 0, 0 };
	const float b[7] = { 1, 1, 1, 1, 1, 7, 9 };
	auto t = NewTable();
	AccumulateGroups(*t, 0, a, b, g, 7, nullptr);
	SortByAbsChange(*t);
	ASSERT_EQ(t->rowCount, 2u);
	EXPECT_EQ(t->rows[0].groupValue, 3u);
	EXPECT_EQ(t->rows[1].groupValue, 255u);
	EXPECT_DOUBLE_EQ(t->rows[1].change, 7.0);
}

TEST(GroupDeltaPure, NaNInEitherSideExcludesCellFromBothMeans)
{
	// cell 1 has NaN in A (b = 100 would skew meanB), cell 2 has NaN in B (a = 100 would skew meanA).
	const float g[4] = { 0, 0, 0, 0 };
	const float a[4] = { 2, kNaN, 100, 4 };
	const float b[4] = { 4, 100, kNaN, 8 };
	auto t = NewTable();
	AccumulateGroups(*t, 0, a, b, g, 4, nullptr);
	ASSERT_EQ(t->rowCount, 1u);
	EXPECT_EQ(t->rows[0].count, 2u);
	EXPECT_DOUBLE_EQ(t->rows[0].meanA, 3.0);
	EXPECT_DOUBLE_EQ(t->rows[0].meanB, 6.0);
	EXPECT_DOUBLE_EQ(t->rows[0].change, 3.0);
}

TEST(GroupDeltaPure, GroupWithOnlyNaNCellsIsOmitted)
{
	const float g[2] = { 0, 1 };
	const float a[2] = { kNaN, 1 };
	const float b[2] = { 5, 2 };
	auto t = NewTable();
	AccumulateGroups(*t, 0, a, b, g, 2, nullptr);
	ASSERT_EQ(t->rowCount, 1u);
	EXPECT_EQ(t->rows[0].groupValue, 1u);
}

TEST(GroupDeltaPure, SelectionRestrictsCells)
{
	const uint8_t mask[6] = { 1, 0, 1, 0, 1, 0 };	// cells 0,2 (group 0), cell 4 (group 1)
	auto t = NewTable();
	AccumulateGroups(*t, 0, kA, kB, kG, 6, mask);
	SortByAbsChange(*t);
	ASSERT_EQ(t->rowCount, 2u);
	const GroupDeltaRow* g0 = FindRow(*t, 0, 0);
	const GroupDeltaRow* g1 = FindRow(*t, 0, 1);
	ASSERT_NE(g0, nullptr); ASSERT_NE(g1, nullptr);
	EXPECT_EQ(g0->count, 2u);
	EXPECT_DOUBLE_EQ(g0->meanA, 2.0);	// (1 + 3) / 2
	EXPECT_DOUBLE_EQ(g0->meanB, 4.0);	// (2 + 6) / 2
	EXPECT_EQ(g1->count, 1u);
	EXPECT_DOUBLE_EQ(g1->change, 3.0);
}

TEST(GroupDeltaPure, GroupWithNoSelectedCellsOmittedAndEmptyMaskGivesNoRows)
{
	const uint8_t onlyG0[6] = { 1, 1, 1, 0, 0, 0 };
	auto t = NewTable();
	AccumulateGroups(*t, 0, kA, kB, kG, 6, onlyG0);
	ASSERT_EQ(t->rowCount, 1u);
	EXPECT_EQ(t->rows[0].groupValue, 0u);

	const uint8_t none[6] = { 0, 0, 0, 0, 0, 0 };
	auto t2 = NewTable();
	AccumulateGroups(*t2, 0, kA, kB, kG, 6, none);
	EXPECT_EQ(t2->rowCount, 0u);
}

TEST(GroupDeltaPure, EmptyGroupsOmittedAndZeroCells)
{
	const float g[2] = { 7, 7 };
	const float a[2] = { 1, 1 };
	const float b[2] = { 2, 2 };
	auto t = NewTable();
	AccumulateGroups(*t, 0, a, b, g, 2, nullptr);
	EXPECT_EQ(t->rowCount, 1u);	// only group 7; the other 255 groups have no cells

	auto t0 = NewTable();
	AccumulateGroups(*t0, 0, a, b, g, 0, nullptr);
	SortByAbsChange(*t0);
	EXPECT_EQ(t0->rowCount, 0u);
	EXPECT_FALSE(t0->truncated);
	AccumulateGroups(*t0, 0, nullptr, nullptr, nullptr, 5, nullptr);	// null input is a no-op
	EXPECT_EQ(t0->rowCount, 0u);
}

TEST(GroupDeltaPure, TruncationKeepsLargestChanges)
{
	// 4 grouping layers x 256 groups = 1024 candidate rows; row change = layer * 1000 + group + 1 (all distinct).
	std::vector<float> g(256), a(256, 0.0f), b(256);
	auto t = NewTable();
	for (uint32_t layer = 0; layer < 4; ++layer)
	{
		for (uint32_t i = 0; i < 256; ++i) { g[i] = float(i); b[i] = float(layer * 1000 + i + 1); }
		AccumulateGroups(*t, layer, a.data(), b.data(), g.data(), 256, nullptr);
	}
	SortByAbsChange(*t);
	EXPECT_TRUE(t->truncated);
	ASSERT_EQ(t->rowCount, GroupDeltaTable::kMaxRows);
	for (uint32_t i = 0; i < t->rowCount; ++i) { EXPECT_GE(t->rows[i].groupLayerIndex, 2u) << i; }	// layers 2 and 3 hold the largest
	EXPECT_EQ(t->rows[0].groupLayerIndex, 3u);
	EXPECT_EQ(t->rows[0].groupValue, 255u);
	for (uint32_t i = 1; i < t->rowCount; ++i) { EXPECT_GE(std::fabs(t->rows[i - 1].change), std::fabs(t->rows[i].change)); }
}

TEST(GroupDeltaPure, ExactlyCapRowsIsNotTruncated)
{
	std::vector<float> g(256), a(256, 0.0f), b(256);
	auto t = NewTable();
	for (uint32_t layer = 0; layer < 2; ++layer)
	{
		for (uint32_t i = 0; i < 256; ++i) { g[i] = float(i); b[i] = float(layer * 1000 + i + 1); }
		AccumulateGroups(*t, layer, a.data(), b.data(), g.data(), 256, nullptr);
	}
	EXPECT_EQ(t->rowCount, GroupDeltaTable::kMaxRows);
	EXPECT_FALSE(t->truncated);
}

TEST(GroupDeltaPure, ClearResetsEverything)
{
	auto t = NewTable();
	AccumulateGroups(*t, 0, kA, kB, kG, 6, nullptr);
	t->truncated = true; t->baseNoData = true; t->usedLive[2] = true; t->skipped[3] = true;
	t->Clear();
	EXPECT_EQ(t->rowCount, 0u);
	EXPECT_FALSE(t->truncated); EXPECT_FALSE(t->baseNoData); EXPECT_FALSE(t->usedLive[2]); EXPECT_FALSE(t->skipped[3]);
}

// ---- builder ----

TEST(GroupDeltaBuilder, SampleVsSampleGroupsByChosenSide)
{
	Rig r; AddLayers(r); Track(r, "temp", "biome"); r.Start(); TwoSamples(r);
	GroupDeltaBuilder b(*r.rec);
	auto t = NewTable();
	const Dia::CellLayers::CellLayerDesc groups[1] = { Desc(r, "biome") };

	// side B: biome at sample 1: group 1 = cells 0..2 (10 -> 30), group 0 = cells 3..11 (10 -> mean 18)
	b.Build(Desc(r, "temp"), Spec("temp", S(0), S(1), true), groups, 1, nullptr, *t);
	ASSERT_EQ(t->rowCount, 2u);
	EXPECT_FALSE(t->baseNoData);
	EXPECT_FALSE(t->usedLive[0]);
	EXPECT_EQ(t->rows[0].groupValue, 1u);
	EXPECT_EQ(t->rows[0].count, 3u);
	EXPECT_NEAR(t->rows[0].meanA, 10.0, 0.01);
	EXPECT_NEAR(t->rows[0].meanB, 30.0, 0.01);
	EXPECT_NEAR(t->rows[0].change, 20.0, 0.01);
	EXPECT_EQ(t->rows[1].groupValue, 0u);
	EXPECT_EQ(t->rows[1].count, 9u);
	EXPECT_NEAR(t->rows[1].meanB, 18.0, 0.01);
	EXPECT_NEAR(t->rows[1].change, 8.0, 0.01);

	// side A: biome at sample 0: group 1 = cells 0..5 (10 -> 30), group 0 = cells 6..11 (10 -> 12)
	b.Build(Desc(r, "temp"), Spec("temp", S(0), S(1), false), groups, 1, nullptr, *t);
	ASSERT_EQ(t->rowCount, 2u);
	EXPECT_EQ(t->rows[0].groupValue, 1u);
	EXPECT_EQ(t->rows[0].count, 6u);
	EXPECT_NEAR(t->rows[0].change, 20.0, 0.01);
	EXPECT_EQ(t->rows[1].groupValue, 0u);
	EXPECT_EQ(t->rows[1].count, 6u);
	EXPECT_NEAR(t->rows[1].change, 2.0, 0.01);
}

TEST(GroupDeltaBuilder, StaticGroupingSameEitherSide)
{
	Rig r; AddLayers(r); Track(r, "temp", "region"); r.Start(); TwoSamples(r);
	GroupDeltaBuilder b(*r.rec);
	auto tb = NewTable(); auto ta = NewTable();
	const Dia::CellLayers::CellLayerDesc groups[1] = { Desc(r, "region") };
	b.Build(Desc(r, "temp"), Spec("temp", S(0), S(1), true), groups, 1, nullptr, *tb);
	b.Build(Desc(r, "temp"), Spec("temp", S(0), S(1), false), groups, 1, nullptr, *ta);
	ASSERT_EQ(tb->rowCount, 2u);
	ASSERT_EQ(ta->rowCount, 2u);
	for (uint32_t i = 0; i < 2; ++i)
	{
		EXPECT_EQ(tb->rows[i].groupValue, ta->rows[i].groupValue);
		EXPECT_EQ(tb->rows[i].count, ta->rows[i].count);
		EXPECT_DOUBLE_EQ(tb->rows[i].change, ta->rows[i].change);
	}
	EXPECT_NEAR(FindRow(*tb, 0, 1)->change, 20.0, 0.01);
	EXPECT_NEAR(FindRow(*tb, 0, 0)->change, 2.0, 0.01);
}

TEST(GroupDeltaBuilder, UntrackedGroupingFallsBackToLiveAndWarnsOnce)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start(); TwoSamples(r);
	for (uint32_t c = 0; c < 4; ++c) { r.grid.Set("zone", c, 2.0f); }	// live only
	GroupDeltaBuilder b(*r.rec);
	auto t = NewTable();
	const Dia::CellLayers::CellLayerDesc groups[1] = { Desc(r, "zone") };
	LogCapture log("HIS");
	const unsigned base = log.Warnings();
	b.Build(Desc(r, "temp"), Spec("temp", S(0), S(1), true), groups, 1, nullptr, *t);
	EXPECT_TRUE(t->usedLive[0]);
	EXPECT_FALSE(t->skipped[0]);
	ASSERT_EQ(t->rowCount, 2u);
	const GroupDeltaRow* z2 = FindRow(*t, 0, 2);
	ASSERT_NE(z2, nullptr);
	EXPECT_EQ(z2->count, 4u);
	EXPECT_NEAR(z2->change, 20.0, 0.01);
	EXPECT_EQ(log.Warnings(), base + 1);
	b.Build(Desc(r, "temp"), Spec("temp", S(0), S(1), true), groups, 1, nullptr, *t);
	EXPECT_EQ(log.Warnings(), base + 1);	// once per builder
	b.ResetWarnings();
	b.Build(Desc(r, "temp"), Spec("temp", S(0), S(1), true), groups, 1, nullptr, *t);
	EXPECT_EQ(log.Warnings(), base + 2);
}

TEST(GroupDeltaBuilder, GroupingWithNoLiveReadIsSkipped)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start(); TwoSamples(r);
	GroupDeltaBuilder b(*r.rec);
	auto t = NewTable();
	Dia::CellLayers::CellLayerDesc groups[2] = { Desc(r, "zone"), Desc(r, "biome") };	// both untracked here
	groups[0].read.clear();
	b.Build(Desc(r, "temp"), Spec("temp", S(0), S(1), true), groups, 2, nullptr, *t);
	EXPECT_TRUE(t->skipped[0]);
	EXPECT_FALSE(t->usedLive[0]);
	EXPECT_FALSE(t->skipped[1]);
	EXPECT_TRUE(t->usedLive[1]);
}

TEST(GroupDeltaBuilder, BaseUntrackedOnSampleSideIsBaseNoData)
{
	Rig r; AddLayers(r); Track(r, "temp", "biome"); r.Start(); TwoSamples(r);
	GroupDeltaBuilder b(*r.rec);
	auto t = NewTable();
	const Dia::CellLayers::CellLayerDesc groups[1] = { Desc(r, "biome") };
	b.Build(Desc(r, "wet"), Spec("wet", S(0), S(1), true), groups, 1, nullptr, *t);
	EXPECT_TRUE(t->baseNoData);
	EXPECT_EQ(t->rowCount, 0u);
	// Stale rows from a previous build are cleared.
	b.Build(Desc(r, "temp"), Spec("temp", S(0), S(1), true), groups, 1, nullptr, *t);
	EXPECT_GT(t->rowCount, 0u);
	b.Build(Desc(r, "wet"), Spec("wet", S(0), S(1), true), groups, 1, nullptr, *t);
	EXPECT_EQ(t->rowCount, 0u);
	EXPECT_TRUE(t->baseNoData);
}

TEST(GroupDeltaBuilder, SampleOutOfRangeIsBaseNoData)
{
	Rig r; AddLayers(r); Track(r, "temp", "biome"); r.Start(); TwoSamples(r);
	GroupDeltaBuilder b(*r.rec);
	auto t = NewTable();
	const Dia::CellLayers::CellLayerDesc groups[1] = { Desc(r, "biome") };
	b.Build(Desc(r, "temp"), Spec("temp", S(0), S(7), true), groups, 1, nullptr, *t);
	EXPECT_TRUE(t->baseNoData);
	EXPECT_EQ(t->rowCount, 0u);
}

TEST(GroupDeltaBuilder, SnapshotSideIsBaseNoData)
{
	Rig r; AddLayers(r); Track(r, "temp", "biome"); r.Start(); TwoSamples(r);
	GroupDeltaBuilder b(*r.rec);
	auto t = NewTable();
	const Dia::CellLayers::CellLayerDesc groups[1] = { Desc(r, "biome") };
	b.Build(Desc(r, "temp"), Spec("temp", Snap(), S(1), true), groups, 1, nullptr, *t);
	EXPECT_TRUE(t->baseNoData);
	EXPECT_EQ(t->rowCount, 0u);
	b.Build(Desc(r, "temp"), Spec("temp", S(0), Snap(), false), groups, 1, nullptr, *t);
	EXPECT_TRUE(t->baseNoData);
	EXPECT_EQ(t->rowCount, 0u);
}

TEST(GroupDeltaBuilder, NowVersusSampleUsesLiveWithoutFallbackFlag)
{
	Rig r; AddLayers(r); Track(r, "temp", "biome"); r.Start(); TwoSamples(r);
	r.grid.SetAll("temp", 50.0f);
	r.grid.SetAll("biome", 2.0f);	// live biome: everything is group 2
	GroupDeltaBuilder b(*r.rec);
	auto t = NewTable();
	const Dia::CellLayers::CellLayerDesc groups[1] = { Desc(r, "biome") };
	b.Build(Desc(r, "temp"), Spec("temp", S(0), Now(), true), groups, 1, nullptr, *t);
	EXPECT_FALSE(t->baseNoData);
	EXPECT_FALSE(t->usedLive[0]);	// `now` is a legitimate side, not a fallback
	ASSERT_EQ(t->rowCount, 1u);
	EXPECT_EQ(t->rows[0].groupValue, 2u);
	EXPECT_EQ(t->rows[0].count, 12u);
	EXPECT_NEAR(t->rows[0].meanA, 10.0, 0.01);
	EXPECT_DOUBLE_EQ(t->rows[0].meanB, 50.0);
	EXPECT_NEAR(t->rows[0].change, 40.0, 0.01);

	// group side A = sample 0's biome (1 on cells 0..5, 0 elsewhere)
	b.Build(Desc(r, "temp"), Spec("temp", S(0), Now(), false), groups, 1, nullptr, *t);
	ASSERT_EQ(t->rowCount, 2u);
	EXPECT_EQ(t->rows[0].count, 6u);
}

TEST(GroupDeltaBuilder, SelectionRestrictsBuilderRows)
{
	Rig r; AddLayers(r); Track(r, "temp", "biome"); r.Start(); TwoSamples(r);
	GroupDeltaBuilder b(*r.rec);
	auto t = NewTable();
	const Dia::CellLayers::CellLayerDesc groups[1] = { Desc(r, "biome") };
	uint8_t mask[12] = {};
	mask[0] = 1; mask[1] = 1;	// both biome 1 at sample 1
	b.Build(Desc(r, "temp"), Spec("temp", S(0), S(1), true), groups, 1, mask, *t);
	ASSERT_EQ(t->rowCount, 1u);
	EXPECT_EQ(t->rows[0].groupValue, 1u);
	EXPECT_EQ(t->rows[0].count, 2u);
}

TEST(GroupDeltaBuilder, MultipleGroupingLayersKeepLayerIndex)
{
	Rig r; AddLayers(r); Track(r, "temp", "biome", "region"); r.Start(); TwoSamples(r);
	GroupDeltaBuilder b(*r.rec);
	auto t = NewTable();
	const Dia::CellLayers::CellLayerDesc groups[2] = { Desc(r, "region"), Desc(r, "biome") };
	b.Build(Desc(r, "temp"), Spec("temp", S(0), S(1), true), groups, 2, nullptr, *t);
	ASSERT_EQ(t->rowCount, 4u);
	for (uint32_t i = 1; i < t->rowCount; ++i) { EXPECT_GE(std::fabs(t->rows[i - 1].change), std::fabs(t->rows[i].change)); }
	ASSERT_NE(FindRow(*t, 0, 1), nullptr);	// region group 1
	ASSERT_NE(FindRow(*t, 1, 1), nullptr);	// biome group 1
	EXPECT_EQ(FindRow(*t, 0, 1)->count, 6u);
	EXPECT_EQ(FindRow(*t, 1, 1)->count, 3u);
}

TEST(GroupDeltaBuilder, ZeroGroupingLayersGivesEmptyTable)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start(); TwoSamples(r);
	GroupDeltaBuilder b(*r.rec);
	auto t = NewTable();
	b.Build(Desc(r, "temp"), Spec("temp", S(0), S(1), true), nullptr, 0, nullptr, *t);
	EXPECT_EQ(t->rowCount, 0u);
	EXPECT_FALSE(t->baseNoData);
}
