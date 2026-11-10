#include <gtest/gtest.h>
#include <DiaCellHistory/Testing/BudgetSizing.h>
#include <DiaCellHistory/Testing/FakeClock.h>
#include <DiaCellHistory/Testing/FixtureEventSchemas.h>
#include <DiaCellHistory/Testing/StubGrid.h>

#include <string.h>

using namespace Dia::CellHistory::Testing;
using Dia::Core::StringCRC;

TEST(HistoryTestSupport, StubGridReadReturnsWhatWasSet)
{
	StubGrid g;
	EXPECT_EQ(g.CellCount(), 48u);
	g.AddFloatLayer("grass", 1.5f);
	g.Set("grass", 3, 9.0f);

	Dia::CellLayers::CellLayerDesc d;
	ASSERT_TRUE(g.Describe(0, d));
	std::vector<float> buf(g.CellCount());
	d.read(buf.data(), g.CellCount());
	EXPECT_FLOAT_EQ(buf[0], 1.5f);
	EXPECT_FLOAT_EQ(buf[3], 9.0f);
	g.SetAll("grass", 2.0f);
	d.read(buf.data(), g.CellCount());
	EXPECT_FLOAT_EQ(buf[47], 2.0f);
	EXPECT_EQ(d.grid, &g.Grid());
}

TEST(HistoryTestSupport, StubGridCategoryLayerKind)
{
	StubGrid g(4, 4);
	g.AddCategoryLayer("biome", 2);
	Dia::CellLayers::CellLayerDesc d;
	ASSERT_TRUE(g.Describe(0, d));
	EXPECT_EQ(d.kind, Dia::CellLayers::CellValueKind::Category);
	float v[16];
	d.read(v, 16);
	EXPECT_FLOAT_EQ(v[5], 2.0f);
	EXPECT_FALSE(g.Describe(1, d));
}

TEST(HistoryTestSupport, StubGridVersionBumpAndReadCount)
{
	StubGrid g;
	g.AddFloatLayer("a");
	g.AddFloatLayer("b");
	Dia::CellLayers::CellLayerDesc da, db;
	g.Describe(0, da);
	g.Describe(1, db);
	const uint64_t v0 = da.version();
	g.BumpVersion("a");
	EXPECT_EQ(da.version(), v0 + 1);
	EXPECT_EQ(db.version(), v0);

	std::vector<float> buf(g.CellCount());
	EXPECT_EQ(g.ReadCount("a"), 0u);
	da.read(buf.data(), g.CellCount());
	da.read(buf.data(), g.CellCount());
	db.read(buf.data(), g.CellCount());
	EXPECT_EQ(g.ReadCount("a"), 2u);
	EXPECT_EQ(g.ReadCount("b"), 1u);
	g.ResetReadCounts();
	EXPECT_EQ(g.ReadCount("a"), 0u);
}

TEST(HistoryTestSupport, StubGridRegisterAllInOrder)
{
	StubGrid g;
	g.AddFloatLayer("a");
	g.AddCategoryLayer("b");
	g.AddFloatLayer("c");
	Dia::CellLayers::CellLayerRegistry reg;
	EXPECT_EQ(g.RegisterAll(reg), 3u);
	ASSERT_EQ(reg.LayerCount(), 3u);
	EXPECT_EQ(reg.LayerAt(0)->id, StringCRC("a"));
	EXPECT_EQ(reg.LayerAt(1)->id, StringCRC("b"));
	EXPECT_EQ(reg.LayerAt(2)->id, StringCRC("c"));
	EXPECT_EQ(g.RegisterAll(reg), 0u);	// duplicates rejected
}

TEST(HistoryTestSupport, FakeClockSequenceAndLabels)
{
	FakeClock c(7);
	FakeClockSample s0 = c.Next();
	FakeClockSample s1 = c.Next();
	EXPECT_EQ(s0.tick, 0u);
	EXPECT_EQ(s1.tick, 7u);
	EXPECT_STREQ(s0.label, "Y1 W1");
	EXPECT_STREQ(s1.label, "Y1 W2");	// earlier label pointer still valid
	EXPECT_STREQ(s0.label, "Y1 W1");
	EXPECT_EQ(c.SamplesIssued(), 2u);
}

TEST(HistoryTestSupport, FakeClockBandRule)
{
	FakeClock c;
	uint32_t years = 0;
	FakeClockSample last;
	for (uint32_t i = 0; i < 104; ++i)
	{
		last = c.Next();
		const bool expectYear = (i % 52) == 0;
		EXPECT_EQ(last.band, expectYear ? FakeClock::kBandYear : FakeClock::kBandWeek) << i;
		if (last.band == FakeClock::kBandYear) { ++years; }
	}
	EXPECT_EQ(years, 2u);
	EXPECT_STREQ(last.label, "Y2 W52");
}

TEST(HistoryTestSupport, FakeClockStartWeek)
{
	FakeClock c(1, 29);	// Y1 W30 first
	EXPECT_STREQ(c.Next().label, "Y1 W30");
}

TEST(HistoryTestSupport, BudgetSizingArithmetic)
{
	// Large grid: 6,656 cells, one tracked float layer = 13,312 bytes per sample (spec: about 13 KB).
	EXPECT_EQ(BytesPerSample(1, 0, 6656), 13312u);
	EXPECT_EQ(BytesPerSample(2, 1, 100), 500u);
	EXPECT_EQ(BytesPerSample(0, 0, 100), 0u);

	EXPECT_EQ(WindowLengthFor(13312u * 10, 13312), 10u);
	EXPECT_EQ(WindowLengthFor(13312u * 10 + 5, 13312), 10u);
	EXPECT_EQ(WindowLengthFor(100, 0), 0u);
	EXPECT_EQ(BudgetBytesForWindow(10, 13312), 133120u);
	EXPECT_EQ(WindowLengthFor(BudgetBytesForWindow(7, 500), 500), 7u);
}

TEST(HistoryTestSupport, BudgetSizingAggregateRow)
{
	// 2 float layers (24) + 3 enum values total (6) + group size (2) = 32 per group; 4 groups = 128.
	EXPECT_EQ(AggregateRowBytes(2, 3, 4), 128u);
	EXPECT_EQ(AggregateRowBytes(0, 0, 5), 10u);
	EXPECT_EQ(AggregateRowBytes(1, 0, 0), 0u);
}

TEST(HistoryTestSupport, FixtureSchemasLoadAndFindEvent)
{
	FixtureSchemaSet set;
	ASSERT_TRUE(set.Load());
	const Dia::ValueSchema::ValueInfo* drought = set.Service()->FindEvent(StringCRC("test.drought"));
	ASSERT_NE(drought, nullptr);
	EXPECT_EQ(drought->mPayload.Size(), 2u);
	EXPECT_EQ(drought->mVocabulary.Size(), 2u);
	const Dia::ValueSchema::ValueInfo* simple = set.Service()->FindEvent(StringCRC("test.simple"));
	ASSERT_NE(simple, nullptr);
	EXPECT_EQ(simple->mPayload.Size(), 0u);
	EXPECT_NE(set.Service()->FindEvent(StringCRC("test.flood")), nullptr);
	EXPECT_EQ(set.Service()->FindEvent(StringCRC("test.nope")), nullptr);
}
