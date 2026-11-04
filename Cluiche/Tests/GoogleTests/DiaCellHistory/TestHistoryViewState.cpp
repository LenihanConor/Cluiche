#include "HistoryTestRig.h"
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

	// Tracks `biome` with a cell budget holding exactly 3 samples.
	void StartWithWindow3(Rig& r)
	{
		r.cfg.mTrackCount = 1;
		r.cfg.mTrack[0] = Dia::Core::Containers::String64("%s", "biome");
		RecorderBudgets b;
		b.mAggregateBytes = 1 << 20;
		b.mCellBytes = BudgetBytesForWindow(3, BytesPerSample(0, 1, 12));
		b.mEventBytes = 4096;
		r.Start(&b);
	}
}

TEST(HistoryViewState, StartsLiveAtEpochZero)
{
	Rig r; AddLayers(r); r.Start();
	HistoryViewState v(*r.rec);
	EXPECT_TRUE(v.IsLive());
	EXPECT_EQ(v.Mode(), ViewMode::Live);
	EXPECT_EQ(v.Epoch(), 0u);
	EXPECT_FALSE(v.IsEvicted());
}

TEST(HistoryViewState, LiveToScrubToLive)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 5);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(2, res)) << res.mError;
	EXPECT_TRUE(res.mOk);
	EXPECT_EQ(v.Mode(), ViewMode::Sample);
	EXPECT_FALSE(v.IsLive());
	EXPECT_EQ(v.Sample(), 2u);
	v.GoLive();
	EXPECT_TRUE(v.IsLive());
}

TEST(HistoryViewState, StaysOnTheSampleAsNewSamplesArrive)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 3);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(1, res));
	const uint64_t e = v.Epoch();
	TakeN(r, 10);
	v.Update();
	EXPECT_EQ(v.Mode(), ViewMode::Sample);
	EXPECT_EQ(v.Sample(), 1u);
	EXPECT_EQ(v.Epoch(), e);
}

TEST(HistoryViewState, LatestSampleIsScrubNotLive)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 4);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(3, res));
	EXPECT_FALSE(v.IsLive());
	EXPECT_EQ(v.Sample(), 3u);
	TakeN(r, 2);
	EXPECT_FALSE(v.IsLive());
	EXPECT_EQ(v.Sample(), 3u);
}

TEST(HistoryViewState, OutOfRangeIsRejectedAndChangesNothing)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 4);
	HistoryViewState v(*r.rec);
	ViewResult res;
	// from live
	EXPECT_FALSE(v.ViewSample(4, res));
	EXPECT_FALSE(res.mOk);
	EXPECT_NE(res.mError[0], '\0');
	EXPECT_TRUE(v.IsLive());
	EXPECT_EQ(v.Epoch(), 0u);
	EXPECT_FALSE(v.ViewSample(0xFFFFFFFFu, res));
	EXPECT_TRUE(v.IsLive());
	// from scrub
	ASSERT_TRUE(v.ViewSample(2, res));
	EXPECT_TRUE(res.mOk);
	const uint64_t e = v.Epoch();
	EXPECT_FALSE(v.ViewSample(100, res));
	EXPECT_FALSE(res.mOk);
	EXPECT_EQ(v.Mode(), ViewMode::Sample);
	EXPECT_EQ(v.Sample(), 2u);
	EXPECT_EQ(v.Epoch(), e);
}

TEST(HistoryViewState, ViewingBeforeAnySampleIsAnError)
{
	Rig r; AddLayers(r); r.Start();
	HistoryViewState v(*r.rec);
	ViewResult res;
	EXPECT_FALSE(v.ViewSample(0, res));
	EXPECT_TRUE(v.IsLive());
}

TEST(HistoryViewState, EpochBumpsOnEachRealChangeOnly)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 5);
	HistoryViewState v(*r.rec);
	ViewResult res;
	uint64_t e = v.Epoch();
	ASSERT_TRUE(v.ViewSample(1, res));
	EXPECT_NE(v.Epoch(), e); e = v.Epoch();
	ASSERT_TRUE(v.ViewSample(1, res));			// same sample: no change
	EXPECT_EQ(v.Epoch(), e);
	ASSERT_TRUE(v.ViewSample(3, res));
	EXPECT_NE(v.Epoch(), e); e = v.Epoch();
	EXPECT_FALSE(v.ViewSample(9, res));			// rejected: no change
	EXPECT_EQ(v.Epoch(), e);
	v.GoLive();
	EXPECT_NE(v.Epoch(), e); e = v.Epoch();
	v.GoLive();									// already live: no-op
	EXPECT_EQ(v.Epoch(), e);
	ASSERT_TRUE(v.ViewSample(3, res));			// live -> sample 3 is a change
	EXPECT_NE(v.Epoch(), e);
}

TEST(HistoryViewState, SampleBeforeTheWindowStartIsAcceptedAndReportedEvicted)
{
	Rig r; AddLayers(r);
	StartWithWindow3(r);
	TakeN(r, 8);
	ASSERT_EQ(r.rec->FirstRetained(), 5u);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(2, res)) << res.mError;
	EXPECT_EQ(v.Sample(), 2u);
	EXPECT_TRUE(v.IsEvicted());
	ASSERT_TRUE(v.ViewSample(5, res));
	EXPECT_FALSE(v.IsEvicted());
	v.GoLive();
	EXPECT_FALSE(v.IsEvicted());
}

TEST(HistoryViewState, WindowPassingTheViewedSampleKeepsItAndFlipsEvicted)
{
	LogCapture log("HIS");
	Rig r; AddLayers(r);
	StartWithWindow3(r);
	TakeN(r, 4);	// window [1..3]
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(2, res));
	EXPECT_FALSE(v.IsEvicted());
	EXPECT_FALSE(v.Update());
	const uint64_t e = v.Epoch();
	TakeN(r, 3);	// window [4..6]
	EXPECT_TRUE(v.IsEvicted());
	EXPECT_TRUE(v.Update());
	EXPECT_EQ(v.Mode(), ViewMode::Sample);
	EXPECT_EQ(v.Sample(), 2u);
	EXPECT_NE(v.Epoch(), e);
	EXPECT_EQ(log.Count(LogLevel::kWarning, "evicted"), 1u);
	const uint64_t e2 = v.Epoch();
	TakeN(r, 2);
	EXPECT_FALSE(v.Update());				// one notice only
	EXPECT_EQ(v.Epoch(), e2);
	EXPECT_EQ(log.Count(LogLevel::kWarning, "evicted"), 1u);
}

TEST(HistoryViewState, LogsInfoOnEnteringScrubAndReturningToLive)
{
	LogCapture log("HIS");
	Rig r; AddLayers(r); r.Start(); TakeN(r, 3);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(1, res));
	const std::string label = r.rec->Header(1).mLabel.AsCStr();
	EXPECT_EQ(log.Count(LogLevel::kInfo, "scrub"), 1u);
	EXPECT_GE(log.Count(LogLevel::kInfo, "sample 1"), 1u);
	if (!label.empty()) { EXPECT_GE(log.Count(LogLevel::kInfo, label.c_str()), 1u); }
	v.GoLive();
	EXPECT_EQ(log.Count(LogLevel::kInfo, "live"), 1u);
	v.GoLive();
	EXPECT_EQ(log.Count(LogLevel::kInfo, "live"), 1u);	// no-op logs nothing
}

TEST(HistoryViewState, EvictionBumpsEpochExactlyOnce)
{
	Rig r; AddLayers(r);
	StartWithWindow3(r);
	TakeN(r, 4);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(2, res));
	const uint64_t e = v.Epoch();
	TakeN(r, 3);
	EXPECT_TRUE(v.Update());
	EXPECT_EQ(v.Epoch(), e + 1);
}

TEST(HistoryViewState, TruncatingBelowTheViewedSampleReturnsToLiveWithAWarning)
{
	LogCapture log("HIS");
	Rig r; AddLayers(r); r.Start(); TakeN(r, 6);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(4, res));
	const uint64_t e = v.Epoch();
	r.rec->TruncateAfter(2);			// samples 3.. gone, viewed sample 4 no longer exists
	EXPECT_TRUE(v.Update());
	EXPECT_TRUE(v.IsLive());
	EXPECT_EQ(v.Epoch(), e + 1);
	EXPECT_EQ(log.Count(LogLevel::kWarning, "truncat"), 1u);
	EXPECT_FALSE(v.Update());			// settled: nothing more
	EXPECT_EQ(v.Epoch(), e + 1);
}

TEST(HistoryViewState, TruncatingAtOrAboveTheViewedSampleKeepsTheView)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 6);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(2, res));
	const uint64_t e = v.Epoch();
	r.rec->TruncateAfter(2);			// viewed sample 2 is the last one kept
	EXPECT_FALSE(v.Update());
	EXPECT_EQ(v.Mode(), ViewMode::Sample);
	EXPECT_EQ(v.Sample(), 2u);
	EXPECT_EQ(v.Epoch(), e);
}

TEST(HistoryViewState, TruncatingWhileLiveChangesNothing)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 6);
	HistoryViewState v(*r.rec);
	r.rec->TruncateAfter(1);
	EXPECT_FALSE(v.Update());
	EXPECT_TRUE(v.IsLive());
	EXPECT_EQ(v.Epoch(), 0u);
}
