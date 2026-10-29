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
		CellSelection sel;

		void SetUp() override
		{
			FakePuId() = 1;
			auto info = UploaderSchemaFixture::FloatEntry("temp", -50, 50);
			Dia::ValueSchema::SetText(info.mLabel, "Temperature");
			Dia::ValueSchema::SetText(info.mUnit, "C");
			fx.Add(info);
			service = &fx.Publish();
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
