#include <gtest/gtest.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/SelectionStats.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaValueSchema/ValueInfo.h>

#include <limits>
#include <string.h>
#include <vector>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	const float kNaN = std::numeric_limits<float>::quiet_NaN();

	struct SelectionStatsTest : ::testing::Test
	{
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		Dia::Graphics::CellGridDesc grid = MakeGrid(4, 3);	// 12 cells
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		CountingLayerProvider temp;
		CountingLayerProvider terrain;
		CellSelection sel;

		void SetUp() override
		{
			FakePuId() = 1;
			auto info = UploaderSchemaFixture::FloatEntry("temp", -50, 50);
			Dia::ValueSchema::SetText(info.mLabel, "Temperature");
			Dia::ValueSchema::SetText(info.mUnit, "C");
			fx.Add(info);
			auto terrainInfo = Dia::ValueSchema::Testing::MakeValueInfo("cells", "terrain");
			terrainInfo.mType = Dia::ValueSchema::ValueType::Enum;
			terrainInfo.mEnumValues.Reserve(5);
			for (const char* id : { "grass", "water", "rock", "sand", "unused" })
			{
				Dia::ValueSchema::EnumValue v;
				Dia::ValueSchema::SetText(v.mId, id);
				Dia::ValueSchema::SetText(v.mLabel, id);
				terrainInfo.mEnumValues.Add(v);
			}
			Dia::ValueSchema::SetText(terrainInfo.mLabel, "Terrain");
			fx.Add(terrainInfo);
			service = &fx.Publish();
			terrain.values = { 0, 1, 2, 3, 0, 0, 1, 1, 2, 3, 3, 3 };
			CellLayerDesc td = MakeCountingLayer("terrain", &grid, terrain, CellValueKind::Category);
			td.schemaKey = StringCRC("cells/terrain");
			registry.Register(td);
			temp.values = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
			CellLayerDesc d = MakeCountingLayer("temp", &grid, temp);
			d.schemaKey = StringCRC("cells/temp");
			registry.Register(d);
			sel.Reset(&grid);
		}

		void Select(std::initializer_list<uint32_t> cells) { for (uint32_t c : cells) { sel.Set(c, true); } }

		bool Run(LayerStats& out, const char* id = "temp")
		{
			LayerReader reader(registry);
			return SelectionStats::Compute(registry, reader, service, sel, StringCRC(id), out);
		}
	};
}

TEST_F(SelectionStatsTest, MinMeanMaxOverSubset)
{
	Select({ 1, 3, 4, 10 });	// 2, 4, 5, 11
	LayerStats s;
	ASSERT_TRUE(Run(s));
	EXPECT_TRUE(s.hasData);
	EXPECT_EQ(4u, s.count);
	EXPECT_EQ(0u, s.nanCount);
	EXPECT_TRUE(s.hasRange);
	EXPECT_FLOAT_EQ(2.0f, s.min);
	EXPECT_FLOAT_EQ(11.0f, s.max);
	EXPECT_DOUBLE_EQ(5.5, s.mean);
}

TEST_F(SelectionStatsTest, NaNExcludedFromCountAndCounted)
{
	temp.values[3] = kNaN;
	temp.values[4] = kNaN;
	Select({ 1, 3, 4, 10 });	// 2, NaN, NaN, 11
	LayerStats s;
	ASSERT_TRUE(Run(s));
	EXPECT_EQ(2u, s.count);
	EXPECT_EQ(2u, s.nanCount);
	EXPECT_FLOAT_EQ(2.0f, s.min);
	EXPECT_FLOAT_EQ(11.0f, s.max);
	EXPECT_DOUBLE_EQ(6.5, s.mean);
}

TEST_F(SelectionStatsTest, EmptySelectionHasNoRange)
{
	LayerStats s;
	ASSERT_TRUE(Run(s));
	EXPECT_TRUE(s.hasData);
	EXPECT_EQ(0u, s.count);
	EXPECT_EQ(0u, s.nanCount);
	EXPECT_FALSE(s.hasRange);
}

TEST_F(SelectionStatsTest, AllNaNSelectionHasNoRange)
{
	temp.values[0] = kNaN;
	temp.values[2] = kNaN;
	Select({ 0, 2 });
	LayerStats s;
	ASSERT_TRUE(Run(s));
	EXPECT_EQ(0u, s.count);
	EXPECT_EQ(2u, s.nanCount);
	EXPECT_FALSE(s.hasRange);
}

TEST_F(SelectionStatsTest, SingleValue)
{
	Select({ 6 });
	LayerStats s;
	ASSERT_TRUE(Run(s));
	EXPECT_EQ(1u, s.count);
	EXPECT_TRUE(s.hasRange);
	EXPECT_FLOAT_EQ(7.0f, s.min);
	EXPECT_FLOAT_EQ(7.0f, s.max);
	EXPECT_DOUBLE_EQ(7.0, s.mean);
}

TEST_F(SelectionStatsTest, MeanUsesDoubleAccumulator)
{
	// 1e8 + many 1s: a float accumulator would lose the small values.
	temp.values.assign(12, 1.0f);
	temp.values[0] = 1.0e8f;
	for (uint32_t i = 0; i < 12; ++i) { sel.Set(i, true); }
	LayerStats s;
	ASSERT_TRUE(Run(s));
	EXPECT_DOUBLE_EQ((1.0e8 + 11.0) / 12.0, s.mean);
}

TEST_F(SelectionStatsTest, LabelAndUnitFromSchema)
{
	Select({ 0 });
	LayerStats s;
	ASSERT_TRUE(Run(s));
	EXPECT_EQ(StringCRC("temp"), s.layerId);
	EXPECT_STREQ("Temperature", s.label.AsCStr());
	EXPECT_STREQ("C", s.unit.AsCStr());
}

TEST_F(SelectionStatsTest, NoSchemaLeavesLabelAndUnitEmpty)
{
	Select({ 0 });
	LayerStats s;
	LayerReader reader(registry);
	ASSERT_TRUE(SelectionStats::Compute(registry, reader, nullptr, sel, StringCRC("temp"), s));
	EXPECT_EQ(1u, s.count);
	EXPECT_STREQ("", s.label.AsCStr());
	EXPECT_STREQ("", s.unit.AsCStr());
}

TEST_F(SelectionStatsTest, UnregisteredLayerHasNoData)
{
	Select({ 0 });
	LayerStats s;
	EXPECT_FALSE(Run(s, "missing"));
	EXPECT_FALSE(s.hasData);
	EXPECT_EQ(0u, s.count);
	EXPECT_FALSE(s.hasRange);
}

TEST_F(SelectionStatsTest, ComputeFloatLayerWithoutDataYieldsNoData)
{
	Select({ 0, 1 });
	LayerValues v;	// hasData false
	LayerStats s;
	s.count = 5;
	SelectionStats::ComputeFloat(sel, v, s);
	EXPECT_FALSE(s.hasData);
	EXPECT_EQ(0u, s.count);
	EXPECT_FALSE(s.hasRange);
}

TEST_F(SelectionStatsTest, ComputeFloatIgnoresIndicesBeyondValueCount)
{
	Select({ 0, 1, 2, 9 });
	const float vals[] = { 10.0f, 20.0f };
	LayerValues v;
	v.values = vals;
	v.count = 2;
	v.hasData = true;
	LayerStats s;
	SelectionStats::ComputeFloat(sel, v, s);
	EXPECT_TRUE(s.hasData);
	EXPECT_EQ(2u, s.count);
	EXPECT_EQ(0u, s.nanCount);
	EXPECT_FLOAT_EQ(10.0f, s.min);
	EXPECT_FLOAT_EQ(20.0f, s.max);
	EXPECT_DOUBLE_EQ(15.0, s.mean);
}

// ---- Category counts ----

TEST_F(SelectionStatsTest, CategoryCountsInSchemaOrderWithUnusedZero)
{
	for (uint32_t i = 0; i < 12; ++i) { sel.Set(i, true); }	// 0 1 2 3 0 0 1 1 2 3 3 3
	LayerStats s;
	ASSERT_TRUE(Run(s, "terrain"));
	EXPECT_TRUE(s.hasData);
	EXPECT_EQ(5u, s.categoryCount);
	EXPECT_FALSE(s.categoryTruncated);
	EXPECT_EQ(3u, s.categoryCounts[0]);
	EXPECT_EQ(3u, s.categoryCounts[1]);
	EXPECT_EQ(2u, s.categoryCounts[2]);
	EXPECT_EQ(4u, s.categoryCounts[3]);
	EXPECT_EQ(0u, s.categoryCounts[4]);	// unused enum value
	EXPECT_EQ(0u, s.otherCount);
	EXPECT_EQ(0u, s.nanCount);
	EXPECT_EQ(12u, s.count);
	EXPECT_STREQ("grass", s.categoryIds[0].AsCStr());
	EXPECT_STREQ("unused", s.categoryIds[4].AsCStr());
	EXPECT_STREQ("water", s.categoryLabels[1].AsCStr());
	EXPECT_STREQ("Terrain", s.label.AsCStr());
	EXPECT_EQ(StringCRC("terrain"), s.layerId);
}

TEST_F(SelectionStatsTest, CategoryOnSubsetOnly)
{
	Select({ 0, 1, 2 });
	LayerStats s;
	ASSERT_TRUE(Run(s, "terrain"));
	EXPECT_EQ(1u, s.categoryCounts[0]);
	EXPECT_EQ(1u, s.categoryCounts[1]);
	EXPECT_EQ(1u, s.categoryCounts[2]);
	EXPECT_EQ(0u, s.categoryCounts[3]);
	EXPECT_EQ(3u, s.count);
}

TEST_F(SelectionStatsTest, CategoryIdsOutsideEnumCountAsOther)
{
	terrain.values = { 0, 5, 7, -1, 1.5f, std::numeric_limits<float>::infinity(), 4, 2, 2, 2, 2, 2 };
	for (uint32_t i = 0; i < 12; ++i) { sel.Set(i, true); }
	LayerStats s;
	ASSERT_TRUE(Run(s, "terrain"));
	EXPECT_EQ(5u, s.otherCount);	// 5, 7, -1, 1.5, inf
	EXPECT_EQ(1u, s.categoryCounts[0]);
	EXPECT_EQ(1u, s.categoryCounts[4]);
	EXPECT_EQ(5u, s.categoryCounts[2]);
	EXPECT_EQ(12u, s.count);
}

TEST_F(SelectionStatsTest, CategoryNaNCountedSeparately)
{
	terrain.values[0] = kNaN;
	terrain.values[1] = kNaN;
	Select({ 0, 1, 2, 3 });	// NaN, NaN, 2, 3
	LayerStats s;
	ASSERT_TRUE(Run(s, "terrain"));
	EXPECT_EQ(2u, s.nanCount);
	EXPECT_EQ(2u, s.count);
	EXPECT_EQ(0u, s.otherCount);
	EXPECT_EQ(1u, s.categoryCounts[2]);
	EXPECT_EQ(1u, s.categoryCounts[3]);
}

TEST_F(SelectionStatsTest, CategoryTotalsEqualInRangeSelected)
{
	terrain.values[2] = kNaN;
	terrain.values[3] = 9.0f;
	for (uint32_t i = 0; i < 12; ++i) { sel.Set(i, true); }
	LayerStats s;
	ASSERT_TRUE(Run(s, "terrain"));
	uint32_t total = s.otherCount + s.nanCount;
	for (uint32_t i = 0; i < s.categoryCount; ++i) { total += s.categoryCounts[i]; }
	EXPECT_EQ(12u, total);
}

TEST_F(SelectionStatsTest, CategoryEmptySelection)
{
	LayerStats s;
	ASSERT_TRUE(Run(s, "terrain"));
	EXPECT_TRUE(s.hasData);
	EXPECT_EQ(0u, s.count);
	EXPECT_EQ(0u, s.otherCount);
	EXPECT_EQ(0u, s.nanCount);
	EXPECT_EQ(5u, s.categoryCount);
	for (uint32_t i = 0; i < 5; ++i) { EXPECT_EQ(0u, s.categoryCounts[i]); }
}

TEST_F(SelectionStatsTest, CategoryWithoutSchemaEntryEverythingIsOther)
{
	Select({ 0, 1 });
	LayerStats s;
	LayerReader reader(registry);
	ASSERT_TRUE(SelectionStats::Compute(registry, reader, nullptr, sel, StringCRC("terrain"), s));
	EXPECT_EQ(0u, s.categoryCount);
	EXPECT_EQ(2u, s.otherCount);
	EXPECT_EQ(2u, s.count);
}

TEST_F(SelectionStatsTest, ComputeCategoryPureAndIgnoresIndicesBeyondValueCount)
{
	Select({ 0, 1, 2, 9 });
	const float vals[] = { 0.0f, 1.0f };
	LayerValues v;
	v.values = vals;
	v.count = 2;
	v.hasData = true;
	LayerStats s;
	s.categoryCounts[0] = 99;
	SelectionStats::ComputeCategory(sel, v, 2, s);
	EXPECT_TRUE(s.hasData);
	EXPECT_EQ(2u, s.count);
	EXPECT_EQ(1u, s.categoryCounts[0]);
	EXPECT_EQ(1u, s.categoryCounts[1]);
	EXPECT_EQ(0u, s.otherCount);
}

TEST_F(SelectionStatsTest, ComputeCategoryWithoutDataYieldsNoData)
{
	Select({ 0 });
	LayerValues v;
	LayerStats s;
	s.count = 5;
	SelectionStats::ComputeCategory(sel, v, 3, s);
	EXPECT_FALSE(s.hasData);
	EXPECT_EQ(0u, s.count);
}

TEST_F(SelectionStatsTest, ComputeCategoryClampsBeyondCapacity)
{
	const uint32_t cap = LayerStats::kMaxCategories;
	const float vals[] = { 0.0f, static_cast<float>(cap - 1), static_cast<float>(cap), static_cast<float>(cap + 5) };
	LayerValues v;
	v.values = vals;
	v.count = 4;
	v.hasData = true;
	for (uint32_t i = 0; i < 4; ++i) { sel.Set(i, true); }
	LayerStats s;
	SelectionStats::ComputeCategory(sel, v, cap + 10, s);
	EXPECT_TRUE(s.categoryTruncated);
	EXPECT_EQ(cap, s.categoryCount);
	EXPECT_EQ(1u, s.categoryCounts[0]);
	EXPECT_EQ(1u, s.categoryCounts[cap - 1]);
	EXPECT_EQ(2u, s.otherCount);
	EXPECT_EQ(4u, s.count);
}

TEST_F(SelectionStatsTest, ComputeDispatchesFloatForFloatLayer)
{
	Select({ 0, 1 });
	LayerStats s;
	ASSERT_TRUE(Run(s, "temp"));
	EXPECT_TRUE(s.hasRange);
	EXPECT_EQ(0u, s.categoryCount);
}

TEST_F(SelectionStatsTest, ComputeDispatchesCategoryForCategoryLayer)
{
	Select({ 0, 1 });
	LayerStats s;
	ASSERT_TRUE(Run(s, "terrain"));
	EXPECT_FALSE(s.hasRange);
	EXPECT_EQ(5u, s.categoryCount);
}

// ---- Histogram ----

namespace
{
	struct HistRun
	{
		std::vector<float> vals;
		Histogram h;
		void Go(CellSelection& sel, float mn, float mx)
		{
			LayerValues v;
			v.values = vals.data();
			v.count = static_cast<uint32_t>(vals.size());
			v.hasData = true;
			for (uint32_t i = 0; i < vals.size(); ++i) { sel.Set(i, true); }
			SelectionStats::ComputeHistogram(sel, v, mn, mx, h);
		}
	};
}

TEST_F(SelectionStatsTest, HistogramBinEdgesOverZeroToSixteen)
{
	HistRun r;
	r.vals = { 0.0f, 0.5f, 0.999f, 1.0f, 7.5f, 8.0f, 15.0f, 15.999f, 16.0f };
	r.Go(sel, 0.0f, 16.0f);
	EXPECT_TRUE(r.h.hasData);
	EXPECT_TRUE(r.h.valid);
	EXPECT_EQ(3u, r.h.bins[0]);		// 0, 0.5, 0.999
	EXPECT_EQ(1u, r.h.bins[1]);		// 1.0 exactly on the edge
	EXPECT_EQ(1u, r.h.bins[7]);		// 7.5
	EXPECT_EQ(1u, r.h.bins[8]);		// 8.0
	EXPECT_EQ(3u, r.h.bins[15]);	// 15, 15.999, and max (16.0)
	EXPECT_EQ(0u, r.h.outOfRangeLow);
	EXPECT_EQ(0u, r.h.outOfRangeHigh);
	EXPECT_EQ(9u, r.h.count);
	uint32_t total = 0;
	for (uint32_t i = 0; i < Histogram::kBins; ++i) { total += r.h.bins[i]; }
	EXPECT_EQ(9u, total);
}

TEST_F(SelectionStatsTest, HistogramOutOfRangeClampedAndCounted)
{
	HistRun r;
	r.vals = { -5.0f, -0.001f, 0.0f, 16.0f, 16.001f, 99.0f, std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };
	r.Go(sel, 0.0f, 16.0f);
	EXPECT_EQ(3u, r.h.outOfRangeLow);	// -5, -0.001, -inf
	EXPECT_EQ(3u, r.h.outOfRangeHigh);	// 16.001, 99, +inf
	EXPECT_EQ(4u, r.h.bins[0]);			// three low + 0.0
	EXPECT_EQ(4u, r.h.bins[15]);		// three high + 16.0
	EXPECT_EQ(8u, r.h.count);
}

TEST_F(SelectionStatsTest, HistogramNaNIgnored)
{
	HistRun r;
	r.vals = { kNaN, 4.0f, kNaN };
	r.Go(sel, 0.0f, 16.0f);
	EXPECT_EQ(1u, r.h.count);
	EXPECT_EQ(2u, r.h.nanCount);
	EXPECT_EQ(1u, r.h.bins[4]);
	EXPECT_EQ(0u, r.h.outOfRangeLow);
	EXPECT_EQ(0u, r.h.outOfRangeHigh);
}

TEST_F(SelectionStatsTest, HistogramDegenerateRangeIsInvalidAndAllInBinZero)
{
	for (float mx : { 5.0f, 2.0f })	// max == min, max < min
	{
		CellSelection s2;
		s2.Reset(&grid);
		HistRun r;
		r.vals = { 1.0f, 5.0f, 100.0f, kNaN };
		r.Go(s2, 5.0f, mx);
		EXPECT_TRUE(r.h.hasData);
		EXPECT_FALSE(r.h.valid);
		EXPECT_EQ(3u, r.h.bins[0]);
		EXPECT_EQ(3u, r.h.count);
		EXPECT_EQ(1u, r.h.nanCount);
		EXPECT_EQ(0u, r.h.outOfRangeLow);
		EXPECT_EQ(0u, r.h.outOfRangeHigh);
		for (uint32_t i = 1; i < Histogram::kBins; ++i) { EXPECT_EQ(0u, r.h.bins[i]); }
	}
}

TEST_F(SelectionStatsTest, HistogramEmptySelection)
{
	LayerValues v;
	const float vals[] = { 1.0f };
	v.values = vals;
	v.count = 1;
	v.hasData = true;
	Histogram h;
	SelectionStats::ComputeHistogram(sel, v, 0.0f, 16.0f, h);
	EXPECT_TRUE(h.hasData);
	EXPECT_TRUE(h.valid);
	EXPECT_EQ(0u, h.count);
}

TEST_F(SelectionStatsTest, HistogramWithoutDataHasNoData)
{
	Select({ 0 });
	LayerValues v;
	Histogram h;
	h.count = 4;
	SelectionStats::ComputeHistogram(sel, v, 0.0f, 16.0f, h);
	EXPECT_FALSE(h.hasData);
	EXPECT_EQ(0u, h.count);
}

TEST_F(SelectionStatsTest, HistogramUsesSchemaRangeForFloatLayer)
{
	// temp schema range -50..50, bin width 6.25; temp values start as 1..12.
	for (uint32_t i = 0; i < 12; ++i) { sel.Set(i, true); }
	temp.values[0] = -50.0f;	// bin 0
	temp.values[1] = -43.75f;	// bin 1 (edge)
	temp.values[2] = 50.0f;		// bin 15
	temp.values[3] = 60.0f;		// out of range high, bin 15
	LayerReader reader(registry);
	Histogram h;
	ASSERT_TRUE(SelectionStats::ComputeHistogram(registry, reader, service, sel, StringCRC("temp"), h));
	EXPECT_TRUE(h.valid);
	EXPECT_FLOAT_EQ(-50.0f, h.minRange);
	EXPECT_FLOAT_EQ(50.0f, h.maxRange);
	EXPECT_EQ(1u, h.bins[0]);
	EXPECT_EQ(1u, h.bins[1]);
	EXPECT_EQ(2u, h.bins[15]);
	EXPECT_EQ(1u, h.outOfRangeHigh);
	EXPECT_EQ(0u, h.outOfRangeLow);
	EXPECT_EQ(2u, h.bins[8]);	// values 5, 6 -> (v+50)/6.25 = 8.8, 8.96
	EXPECT_EQ(6u, h.bins[9]);	// values 7..12 -> 9.12 .. 9.92
	EXPECT_EQ(12u, h.count);
}

TEST_F(SelectionStatsTest, HistogramSchemaLookupFailures)
{
	Select({ 0 });
	LayerReader reader(registry);
	Histogram h;
	EXPECT_FALSE(SelectionStats::ComputeHistogram(registry, reader, service, sel, StringCRC("missing"), h));
	EXPECT_FALSE(SelectionStats::ComputeHistogram(registry, reader, service, sel, StringCRC("terrain"), h));	// category layer
	EXPECT_FALSE(SelectionStats::ComputeHistogram(registry, reader, nullptr, sel, StringCRC("temp"), h));	// no schema
	EXPECT_FALSE(h.hasData);
}
