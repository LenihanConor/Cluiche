#include "HistoryTestRig.h"
#include <DiaCellHistory/SideReader.h>
#include <cmath>
#include <cstring>
#include <limits>

using namespace Dia::CellHistory;
using namespace HistoryRig;

namespace
{
	void AddLayers(Rig& r)
	{
		r.grid.AddFloatLayer("temp", 40.0f);
		r.grid.AddCategoryLayer("biome", 1);
		ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3) }));
	}
	void Track(Rig& r, const char* layer)
	{
		r.cfg.mTrackCount = 1;
		r.cfg.mTrack[0] = Dia::Core::Containers::String64("%s", layer);
	}
	const Dia::CellLayers::CellLayerDesc& Desc(Rig& r, const char* id)
	{
		const Dia::CellLayers::CellLayerDesc* d = r.registry.Find(Dia::Core::StringCRC(id));
		EXPECT_NE(d, nullptr);
		return *d;
	}
	Side Parse(const char* t)
	{
		Side s; SideParseResult e;
		EXPECT_TRUE(ParseSide(t, s, e)) << t << ": " << e.error;
		return s;
	}
	bool Rejects(const char* t)
	{
		Side s; SideParseResult e;
		const bool ok = ParseSide(t, s, e);
		return !ok && !e.ok && e.error[0] != '\0';
	}
	Side SampleSide(uint32_t i) { Side s; s.kind = SideKind::Sample; s.sample = i; return s; }
}

TEST(SideParse, Now)
{
	EXPECT_EQ(Parse("now").kind, SideKind::Now);
}
TEST(SideParse, Samples)
{
	Side a = Parse("sample:0"), b = Parse("sample:42");
	EXPECT_EQ(a.kind, SideKind::Sample); EXPECT_EQ(a.sample, 0u);
	EXPECT_EQ(b.kind, SideKind::Sample); EXPECT_EQ(b.sample, 42u);
}
TEST(SideParse, Snapshot)
{
	Side s = Parse("snapshot:name");
	EXPECT_EQ(s.kind, SideKind::Snapshot);
	EXPECT_STREQ(s.snapshot.AsCStr(), "name");
}
TEST(SideParse, Rejections)
{
	EXPECT_TRUE(Rejects("garbage"));
	EXPECT_TRUE(Rejects("sample:"));
	EXPECT_TRUE(Rejects("sample:-1"));
	EXPECT_TRUE(Rejects("sample:abc"));
	EXPECT_TRUE(Rejects("sample:1x"));
	EXPECT_TRUE(Rejects("sample:99999999999"));
	EXPECT_TRUE(Rejects("snapshot:"));
	EXPECT_TRUE(Rejects("nowx"));
	EXPECT_TRUE(Rejects(""));
	EXPECT_TRUE(Rejects(nullptr));
	EXPECT_TRUE(Rejects("snapshot:0123456789012345678901234567890123456789012345678901234567890123456789"));
}

TEST(SideEqual, Cases)
{
	EXPECT_TRUE(SidesEqual(Parse("now"), Parse("now")));
	EXPECT_TRUE(SidesEqual(Parse("sample:1"), Parse("sample:1")));
	EXPECT_FALSE(SidesEqual(Parse("sample:1"), Parse("sample:2")));
	EXPECT_FALSE(SidesEqual(Parse("now"), Parse("sample:1")));
	EXPECT_TRUE(SidesEqual(Parse("snapshot:a"), Parse("snapshot:a")));
	EXPECT_FALSE(SidesEqual(Parse("snapshot:a"), Parse("snapshot:b")));
}

TEST(SideReader, NowReturnsLiveValuesExactly)
{
	Rig r; AddLayers(r); r.Start(); r.Take();
	r.grid.Set("temp", 3, 12.345f);
	SideReader sr(*r.rec);
	float out[12] = {};
	ASSERT_EQ(sr.Read(Parse("now"), Desc(r, "temp"), out, 12), SideReadStatus::Ok);
	EXPECT_EQ(out[3], 12.345f);
	EXPECT_EQ(out[0], 40.0f);
}

TEST(SideReader, NowWithNullReadIsNoData)
{
	Rig r; AddLayers(r); r.Start(); r.Take();
	Dia::CellLayers::CellLayerDesc d = Desc(r, "temp");
	d.read.clear();
	SideReader sr(*r.rec);
	float out[12];
	EXPECT_EQ(sr.Read(Parse("now"), d, out, 12), SideReadStatus::NoData);
	EXPECT_EQ(sr.Read(Parse("now"), Desc(r, "temp"), nullptr, 12), SideReadStatus::NoData);
}

TEST(SideReader, SampleMatchesRecorderAndNeverFallsBackToLive)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start();
	r.Take(); r.Take();
	r.grid.SetAll("temp", 90.0f);	// live now differs from the recorded 40
	SideReader sr(*r.rec);
	float got[12], want[12];
	ASSERT_TRUE(r.rec->CellValues(1, Dia::Core::StringCRC("temp"), want, 12));
	ASSERT_EQ(sr.Read(SampleSide(1), Desc(r, "temp"), got, 12), SideReadStatus::Ok);
	for (int i = 0; i < 12; ++i) { EXPECT_EQ(got[i], want[i]) << i; EXPECT_NE(got[i], 90.0f); }
}

TEST(SideReader, SampleOutsideWindowOrRangeIsNoData)
{
	Rig r; AddLayers(r); Track(r, "biome");
	RecorderBudgets b;
	b.mAggregateBytes = 1 << 20;
	b.mCellBytes = Dia::CellHistory::Testing::BudgetBytesForWindow(3, Dia::CellHistory::Testing::BytesPerSample(0, 1, 12));
	b.mEventBytes = 4096;
	r.Start(&b);
	for (int i = 0; i < 8; ++i) { r.Take(); }
	SideReader sr(*r.rec);
	float out[12];
	ASSERT_GT(r.rec->FirstRetained(), 0u);
	EXPECT_EQ(sr.Read(SampleSide(0), Desc(r, "biome"), out, 12), SideReadStatus::NoData);
	EXPECT_EQ(sr.Read(SampleSide(r.rec->FirstRetained()), Desc(r, "biome"), out, 12), SideReadStatus::Ok);
	EXPECT_EQ(sr.Read(SampleSide(r.rec->SampleCount() - 1), Desc(r, "biome"), out, 12), SideReadStatus::Ok);
	EXPECT_EQ(sr.Read(SampleSide(r.rec->SampleCount()), Desc(r, "biome"), out, 12), SideReadStatus::NoData);
}

TEST(SideReader, UntrackedLayerIsNoDataWithoutFallback)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); r.Take(); r.Take();
	SideReader sr(*r.rec);
	float out[12];
	for (float& f : out) { f = -7.0f; }
	EXPECT_EQ(sr.Read(SampleSide(1), Desc(r, "temp"), out, 12), SideReadStatus::NoData);
	for (float f : out) { EXPECT_EQ(f, -7.0f); }
}

TEST(SideReader, CountMismatchIsNoData)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); r.Take(); r.Take();
	SideReader sr(*r.rec);
	float out[24];
	EXPECT_EQ(sr.Read(SampleSide(1), Desc(r, "biome"), out, 11), SideReadStatus::NoData);
	EXPECT_EQ(sr.Read(SampleSide(1), Desc(r, "biome"), out, 13), SideReadStatus::NoData);
	EXPECT_EQ(sr.Read(SampleSide(1), Desc(r, "biome"), out, 0), SideReadStatus::NoData);
	EXPECT_EQ(sr.Read(SampleSide(1), Desc(r, "biome"), nullptr, 12), SideReadStatus::NoData);
	EXPECT_EQ(sr.Read(SampleSide(1), Desc(r, "biome"), out, 12), SideReadStatus::Ok);
}

TEST(SideReader, NaNCellsStayNaN)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start();
	r.grid.Set("temp", 5, std::numeric_limits<float>::quiet_NaN());
	r.grid.BumpVersion("temp");
	r.Take();
	SideReader sr(*r.rec);
	float want[12], got[12];
	ASSERT_TRUE(r.rec->CellValues(0, Dia::Core::StringCRC("temp"), want, 12));
	if (!std::isnan(want[5])) { GTEST_SKIP() << "recorder does not preserve NaN in the ring"; }
	ASSERT_EQ(sr.Read(SampleSide(0), Desc(r, "temp"), got, 12), SideReadStatus::Ok);
	EXPECT_TRUE(std::isnan(got[5]));
	EXPECT_EQ(got[4], want[4]);
}

TEST(SideReader, SnapshotIsUnavailable)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); r.Take();
	SideReader sr(*r.rec);
	float out[12];
	EXPECT_EQ(sr.Read(Parse("snapshot:foo"), Desc(r, "biome"), out, 12), SideReadStatus::Unavailable);
}
