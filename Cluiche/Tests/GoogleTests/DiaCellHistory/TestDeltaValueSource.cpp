#include "HistoryTestRig.h"
#include <DiaCellHistory/DeltaValueSource.h>
#include <DiaCellHistory/HistoryValueSource.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellLayers/Testing/LogCapture.h>

#include <cmath>
#include <limits>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::CellLayers::Testing::LogCapture;

namespace
{
	const float kQuantBound = 2.0f * 100.0f / 65535.0f;	// two recorded sides, layer range 0..100

	void AddLayers(Rig& r)
	{
		r.grid.AddFloatLayer("temp", 40.0f);
		r.grid.AddFloatLayer("wet", 10.0f);
		r.grid.AddCategoryLayer("biome", 1);
		ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100), FloatEntry("wet", 0, 100), EnumEntry("biome", 3) }));
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
	bool Enter(HistoryViewState& v, const char* layer, const Side& a, const Side& b)
	{
		ViewResult res;
		return v.EnterDelta(Dia::Core::StringCRC(layer), a, b, res);
	}
	// sample 0: temp=40 everywhere except cell 3 = 10; sample 1: cell 3 = 70.3
	void TwoSamples(Rig& r)
	{
		r.grid.Set("temp", 3, 10.0f); r.grid.BumpVersion("temp"); r.Take();
		r.grid.Set("temp", 3, 70.3f); r.grid.BumpVersion("temp"); r.Take();
	}
}

TEST(DeltaValueSource, SampleVsSampleIsBMinusAWithinQuantisationBound)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start(); TwoSamples(r);
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	ASSERT_TRUE(Enter(v, "temp", S(0), S(1)));
	float out[12];
	ASSERT_TRUE(d.GetValues(Desc(r, "temp"), out, 12));
	EXPECT_NEAR(out[3], 60.3f, kQuantBound);
	EXPECT_NEAR(out[0], 0.0f, kQuantBound);
	EXPECT_EQ(out[0], 0.0f);	// identical recorded values dequantise identically
}

TEST(DeltaValueSource, ReversedSidesNegate)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start(); TwoSamples(r);
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	ASSERT_TRUE(Enter(v, "temp", S(1), S(0)));
	float out[12];
	ASSERT_TRUE(d.GetValues(Desc(r, "temp"), out, 12));
	EXPECT_NEAR(out[3], -60.3f, kQuantBound);
}

TEST(DeltaValueSource, NowIsExact)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start(); TwoSamples(r);
	r.grid.SetAll("temp", 90.0f);
	float rec[12];
	ASSERT_TRUE(r.rec->CellValues(0, Dia::Core::StringCRC("temp"), rec, 12));
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	float out[12];

	ASSERT_TRUE(Enter(v, "temp", S(0), Now()));	// now - sample
	ASSERT_TRUE(d.GetValues(Desc(r, "temp"), out, 12));
	for (int i = 0; i < 12; ++i) { EXPECT_EQ(out[i], 90.0f - rec[i]) << i; }

	ASSERT_TRUE(Enter(v, "temp", Now(), S(0)));	// sample - now
	ASSERT_TRUE(d.GetValues(Desc(r, "temp"), out, 12));
	for (int i = 0; i < 12; ++i) { EXPECT_EQ(out[i], rec[i] - 90.0f) << i; }
}

TEST(DeltaValueSource, NaNOnEitherSideGivesNaNCell)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start();
	r.grid.Set("temp", 5, std::numeric_limits<float>::quiet_NaN()); r.grid.BumpVersion("temp"); r.Take();
	r.grid.Set("temp", 5, 5.0f); r.grid.Set("temp", 6, std::numeric_limits<float>::quiet_NaN()); r.grid.BumpVersion("temp"); r.Take();
	float rec[12];
	ASSERT_TRUE(r.rec->CellValues(0, Dia::Core::StringCRC("temp"), rec, 12));
	if (!std::isnan(rec[5])) { GTEST_SKIP() << "recorder does not preserve NaN in the ring"; }
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	ASSERT_TRUE(Enter(v, "temp", S(0), S(1)));
	float out[12];
	ASSERT_TRUE(d.GetValues(Desc(r, "temp"), out, 12));
	EXPECT_TRUE(std::isnan(out[5]));	// NaN in A
	EXPECT_TRUE(std::isnan(out[6]));	// NaN in B
	EXPECT_FALSE(std::isnan(out[4]));
}

TEST(DeltaValueSource, NaNInNowSideGivesNaNCell)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start(); r.Take();
	r.grid.Set("temp", 2, std::numeric_limits<float>::quiet_NaN());
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	ASSERT_TRUE(Enter(v, "temp", S(0), Now()));
	float out[12];
	ASSERT_TRUE(d.GetValues(Desc(r, "temp"), out, 12));
	EXPECT_TRUE(std::isnan(out[2]));
	EXPECT_FALSE(std::isnan(out[1]));
}

TEST(DeltaValueSource, UntrackedLayerOnSampleSideHasNoDeltaAndNoLiveFallback)
{
	Rig r; AddLayers(r); Track(r, "wet"); r.Start(); r.Take(); r.Take();
	r.grid.SetAll("temp", 77.0f);	// live differs from anything recorded
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	ASSERT_TRUE(Enter(v, "temp", S(0), S(1)));
	float out[12]; for (float& f : out) { f = -7.0f; }
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 12));
	for (float f : out) { EXPECT_EQ(f, -7.0f); }
	ASSERT_TRUE(Enter(v, "temp", S(0), Now()));	// untracked sample side against a legitimate now: still no delta
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 12));
	ASSERT_TRUE(Enter(v, "temp", Now(), S(1)));
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 12));
}

TEST(DeltaValueSource, OtherLayerTrackedOnlyDeltasThatLayer)
{
	Rig r; AddLayers(r); Track(r, "wet"); r.Start();
	r.grid.Set("wet", 1, 20.0f); r.grid.BumpVersion("wet"); r.Take();
	r.grid.Set("wet", 1, 50.0f); r.grid.BumpVersion("wet"); r.Take();
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	ASSERT_TRUE(Enter(v, "wet", S(0), S(1)));
	float out[12];
	ASSERT_TRUE(d.GetValues(Desc(r, "wet"), out, 12));
	EXPECT_NEAR(out[1], 30.0f, kQuantBound);
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 12));
}

TEST(DeltaValueSource, EvictedSampleSideHasNoDelta)
{
	Rig r; AddLayers(r); Track(r, "temp");
	RecorderBudgets b;
	b.mAggregateBytes = 1 << 20;
	b.mCellBytes = BudgetBytesForWindow(3, BytesPerSample(1, 0, 12));
	b.mEventBytes = 4096;
	r.Start(&b);
	for (int i = 0; i < 8; ++i) { r.grid.SetAll("temp", float(i)); r.grid.BumpVersion("temp"); r.Take(); }
	ASSERT_GT(r.rec->FirstRetained(), 0u);
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	ASSERT_TRUE(Enter(v, "temp", S(0), S(r.rec->SampleCount() - 1)));
	float out[12];
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 12));
	ASSERT_TRUE(Enter(v, "temp", S(r.rec->FirstRetained()), S(r.rec->SampleCount() - 1)));
	EXPECT_TRUE(d.GetValues(Desc(r, "temp"), out, 12));
}

TEST(DeltaValueSource, SnapshotSideHasNoDelta)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start(); r.Take();
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	ASSERT_TRUE(Enter(v, "temp", Snap(), S(0)));
	float out[12];
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 12));
	ASSERT_TRUE(Enter(v, "temp", Now(), Snap()));
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 12));
}

TEST(DeltaValueSource, CategoryLayerHasNoDelta)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); r.Take(); r.Take();
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	ASSERT_TRUE(Enter(v, "biome", S(0), S(1)));
	float out[12];
	EXPECT_FALSE(d.GetValues(Desc(r, "biome"), out, 12));
	ASSERT_TRUE(Enter(v, "biome", S(0), Now()));
	EXPECT_FALSE(d.GetValues(Desc(r, "biome"), out, 12));
}

TEST(DeltaValueSource, BadArgumentsAndNotInDeltaReturnFalse)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start(); r.Take(); r.Take();
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	float out[24];
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 12));	// live, not in delta
	ASSERT_TRUE(Enter(v, "temp", S(0), S(1)));
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), nullptr, 12));
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 11));
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 13));
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 0));
	EXPECT_TRUE(d.GetValues(Desc(r, "temp"), out, 12));
}

TEST(DeltaValueSource, WarnsOncePerLayerPerDeltaAndAgainAfterNewDelta)
{
	Rig r; AddLayers(r); Track(r, "wet"); r.Start(); r.Take(); r.Take();
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	LogCapture log("HIS");
	float out[12];
	ASSERT_TRUE(Enter(v, "temp", S(0), S(1)));
	const unsigned base = log.Warnings("no data");
	for (int i = 0; i < 5; ++i) { EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 12)); }
	EXPECT_EQ(log.Warnings("no data"), base + 1);
	EXPECT_FALSE(d.GetValues(Desc(r, "biome"), out, 12));	// category: no data warning (not a data gap)
	EXPECT_EQ(log.Warnings("no data"), base + 1);

	ASSERT_TRUE(Enter(v, "temp", S(1), S(0)));	// a new delta
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 12));
	EXPECT_FALSE(d.GetValues(Desc(r, "temp"), out, 12));
	EXPECT_EQ(log.Warnings("no data"), base + 2);
}

TEST(DeltaValueSource, SuccessfulReadsDoNotWarn)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start(); r.Take(); r.Take();
	HistoryViewState v(*r.rec); DeltaValueSource d(*r.rec, v);
	LogCapture log("HIS");
	ASSERT_TRUE(Enter(v, "temp", S(0), S(1)));
	const unsigned base = log.Warnings();
	float out[12];
	EXPECT_TRUE(d.GetValues(Desc(r, "temp"), out, 12));
	EXPECT_EQ(log.Warnings(), base);
}

TEST(HistoryValueSourceDelta, RoutesToDeltaAndIsNotLive)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start(); TwoSamples(r);
	HistoryViewState v(*r.rec); HistoryValueSource s(r.registry, *r.rec, v);
	ASSERT_TRUE(Enter(v, "temp", S(0), S(1)));
	EXPECT_FALSE(s.IsLive());
	EXPECT_EQ(s.ViewEpoch(), v.Epoch());
	float out[12];
	ASSERT_TRUE(s.GetValues(Desc(r, "temp"), out, 12));
	EXPECT_NEAR(out[3], 60.3f, kQuantBound);
	EXPECT_FALSE(s.GetValues(Desc(r, "wet"), out, 12));
}

TEST(HistoryValueSourceDelta, LiveAndSampleModesUnchangedAroundDelta)
{
	Rig r; AddLayers(r); Track(r, "temp"); r.Start(); TwoSamples(r);
	HistoryViewState v(*r.rec); HistoryValueSource s(r.registry, *r.rec, v);
	float out[12], want[12];
	EXPECT_FALSE(s.GetValues(Desc(r, "temp"), out, 12));	// live
	ViewResult res; ASSERT_TRUE(v.ViewSample(1, res));
	ASSERT_TRUE(s.GetValues(Desc(r, "temp"), out, 12));	// scrub: the recorded sample, not a delta
	ASSERT_TRUE(r.rec->CellValues(1, Dia::Core::StringCRC("temp"), want, 12));
	for (int i = 0; i < 12; ++i) { EXPECT_EQ(out[i], want[i]) << i; }
	ASSERT_TRUE(Enter(v, "temp", S(0), S(1)));
	ASSERT_TRUE(s.GetValues(Desc(r, "temp"), out, 12));
	EXPECT_NEAR(out[3], 60.3f, kQuantBound);
	ASSERT_TRUE(v.ExitDelta());
	ASSERT_TRUE(s.GetValues(Desc(r, "temp"), out, 12));
	for (int i = 0; i < 12; ++i) { EXPECT_EQ(out[i], want[i]) << i; }
}
