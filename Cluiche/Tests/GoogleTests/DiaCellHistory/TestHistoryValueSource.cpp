#include "HistoryTestRig.h"
#include <DiaCellHistory/HistoryValueSource.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellLayers/Testing/LogCapture.h>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::CellLayers::Testing::LogCapture;
using Dia::Observation::Log::LogLevel;

namespace
{
	void AddLayers(Rig& r)
	{
		r.grid.AddFloatLayer("temp", 40.0f);
		r.grid.AddCategoryLayer("biome", 1);
		ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3) }));
	}
	void TakeN(Rig& r, int n) { for (int i = 0; i < n; ++i) { r.Take(); } }

	void Track(Rig& r, const char* layer)
	{
		r.cfg.mTrackCount = 1;
		r.cfg.mTrack[0] = Dia::Core::Containers::String64("%s", layer);
	}
	void StartWithWindow3(Rig& r)
	{
		Track(r, "biome");
		RecorderBudgets b;
		b.mAggregateBytes = 1 << 20;
		b.mCellBytes = BudgetBytesForWindow(3, BytesPerSample(0, 1, 12));
		b.mEventBytes = 4096;
		r.Start(&b);
	}
	const Dia::CellLayers::CellLayerDesc& Desc(Rig& r, const char* id)
	{
		const Dia::CellLayers::CellLayerDesc* d = r.registry.Find(Dia::Core::StringCRC(id));
		EXPECT_NE(d, nullptr);
		return *d;
	}
}

TEST(HistoryValueSource, LiveReportsLiveAndNoValues)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); TakeN(r, 3);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	float out[12] = {};
	EXPECT_TRUE(s.IsLive());
	EXPECT_FALSE(s.GetValues(Desc(r, "biome"), out, 12));
}

TEST(HistoryValueSource, ScrubReturnsExactRecorderValues)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); TakeN(r, 4);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(2, res));
	EXPECT_FALSE(s.IsLive());
	float got[12], want[12];
	ASSERT_TRUE(r.rec->CellValues(2, Dia::Core::StringCRC("biome"), want, 12));
	ASSERT_TRUE(s.GetValues(Desc(r, "biome"), got, 12));
	for (int i = 0; i < 12; ++i) { EXPECT_EQ(got[i], want[i]) << i; }
}

TEST(HistoryValueSource, UntrackedLayerHasNoData)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); TakeN(r, 3);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(1, res));
	float out[12];
	EXPECT_FALSE(s.GetValues(Desc(r, "temp"), out, 12));
}

TEST(HistoryValueSource, EvictedSampleHasNoData)
{
	Rig r; AddLayers(r); StartWithWindow3(r); TakeN(r, 8);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(2, res));
	float out[12];
	EXPECT_FALSE(s.GetValues(Desc(r, "biome"), out, 12));
	ASSERT_TRUE(v.ViewSample(6, res));
	EXPECT_TRUE(s.GetValues(Desc(r, "biome"), out, 12));
}

TEST(HistoryValueSource, CountMismatchHasNoData)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); TakeN(r, 3);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(1, res));
	float out[24];
	EXPECT_FALSE(s.GetValues(Desc(r, "biome"), out, 11));
	EXPECT_FALSE(s.GetValues(Desc(r, "biome"), out, 13));
	EXPECT_FALSE(s.GetValues(Desc(r, "biome"), out, 0));
	EXPECT_TRUE(s.GetValues(Desc(r, "biome"), out, 12));
}

TEST(HistoryValueSource, EpochFollowsViewChangesAndEvictionAfterTick)
{
	Rig r; AddLayers(r); StartWithWindow3(r); TakeN(r, 4);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	ViewResult res;
	EXPECT_EQ(s.ViewEpoch(), v.Epoch());
	uint64_t e = s.ViewEpoch();
	ASSERT_TRUE(v.ViewSample(2, res));
	EXPECT_NE(s.ViewEpoch(), e); EXPECT_EQ(s.ViewEpoch(), v.Epoch()); e = s.ViewEpoch();
	ASSERT_TRUE(v.ViewSample(3, res));
	EXPECT_NE(s.ViewEpoch(), e); e = s.ViewEpoch();
	ASSERT_TRUE(v.ViewSample(2, res)); e = s.ViewEpoch();
	TakeN(r, 3);	// window passes sample 2
	EXPECT_EQ(s.ViewEpoch(), e);	// not until Tick
	EXPECT_TRUE(s.Tick());
	EXPECT_NE(s.ViewEpoch(), e); e = s.ViewEpoch();
	EXPECT_FALSE(s.Tick());
	EXPECT_EQ(s.ViewEpoch(), e);
	v.GoLive();
	EXPECT_NE(s.ViewEpoch(), e);
}

TEST(HistoryValueSource, InstallFollowsRegistryLiveness)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); TakeN(r, 3);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	EXPECT_TRUE(r.registry.IsLive());
	ASSERT_TRUE(s.Install());
	EXPECT_TRUE(s.IsInstalled());
	EXPECT_EQ(r.registry.ViewValueSource(), &s);
	EXPECT_TRUE(r.registry.IsLive());
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(1, res));
	EXPECT_FALSE(r.registry.IsLive());
	v.GoLive();
	EXPECT_TRUE(r.registry.IsLive());
}

TEST(HistoryValueSource, SecondInstallIsRejectedFirstStaysInstalled)
{
	LogCapture log("CEL");
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); TakeN(r, 3);
	HistoryViewState v1(*r.rec), v2(*r.rec);
	HistoryValueSource a(r.registry, *r.rec, v1), b(r.registry, *r.rec, v2);
	ASSERT_TRUE(a.Install());
	EXPECT_FALSE(b.Install());
	EXPECT_FALSE(b.IsInstalled());
	EXPECT_TRUE(a.IsInstalled());
	EXPECT_EQ(r.registry.ViewValueSource(), &a);
	EXPECT_GE(log.Count(LogLevel::kError, "already installed"), 1u);
	b.Uninstall();	// not the installed one: leaves a alone
	EXPECT_EQ(r.registry.ViewValueSource(), &a);
}

TEST(HistoryValueSource, DestructorUninstalls)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); TakeN(r, 3);
	HistoryViewState v(*r.rec);
	{
		HistoryValueSource a(r.registry, *r.rec, v);
		ASSERT_TRUE(a.Install());
	}
	EXPECT_EQ(r.registry.ViewValueSource(), nullptr);
	HistoryValueSource b(r.registry, *r.rec, v);
	EXPECT_TRUE(b.Install());
	b.Uninstall();
	EXPECT_EQ(r.registry.ViewValueSource(), nullptr);
	EXPECT_FALSE(b.IsInstalled());
	b.Uninstall();	// idempotent
	EXPECT_TRUE(b.Install());
}

TEST(HistoryValueSource, UninstallDoesNotRemoveAForeignSource)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); TakeN(r, 3);
	HistoryViewState v(*r.rec);
	HistoryValueSource a(r.registry, *r.rec, v);
	a.Uninstall();	// never installed
	EXPECT_EQ(r.registry.ViewValueSource(), nullptr);
}

TEST(HistoryValueSource, NullOrZeroSizedOutputBufferHasNoData)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); TakeN(r, 3);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(1, res));
	float out[12];
	EXPECT_FALSE(s.GetValues(Desc(r, "biome"), nullptr, 12));
	EXPECT_FALSE(s.GetValues(Desc(r, "biome"), out, 0));
	EXPECT_TRUE(s.GetValues(Desc(r, "biome"), out, 12));
}

TEST(HistoryValueSource, ViewedSampleTruncatedAwayHasNoDataAndTickReturnsToLive)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); TakeN(r, 5);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(4, res));
	r.rec->TruncateAfter(1);
	float out[12];
	EXPECT_FALSE(s.GetValues(Desc(r, "biome"), out, 12)) << "a sample that no longer exists has no values";
	const uint64_t e = s.ViewEpoch();
	EXPECT_TRUE(s.Tick());
	EXPECT_TRUE(s.IsLive());
	EXPECT_NE(s.ViewEpoch(), e);
}

TEST(HistoryValueSource, DeltaModeGetValuesReturnsFalse)
{
	Rig r; AddLayers(r); Track(r, "biome"); r.Start(); TakeN(r, 4);
	HistoryViewState v(*r.rec);
	HistoryValueSource s(r.registry, *r.rec, v);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(2, res));
	Side now; now.kind = SideKind::Now;
	Side smp; smp.kind = SideKind::Sample; smp.sample = 1;
	ASSERT_TRUE(v.EnterDelta(Dia::Core::StringCRC("biome"), now, smp, res));
	float out[12] = {};
	EXPECT_FALSE(s.IsLive());
	EXPECT_FALSE(s.GetValues(Desc(r, "biome"), out, 12));
}
