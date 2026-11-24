#include "HistoryTestRig.h"
#include <cstring>
#include <limits>
#include <string>
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

// ---- Delta mode (Feature 4 task 4.2) ----

namespace
{
	Side MakeSide(SideKind k, uint32_t sample = 0)
	{
		Side s; s.kind = k; s.sample = sample; return s;
	}
	const Dia::Core::StringCRC kTemp("temp");
}

TEST(HistoryViewState, DeltaFromLiveAndOffReturnsLive)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 5);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 2), res)) << res.mError;
	EXPECT_EQ(v.Mode(), ViewMode::Delta);
	EXPECT_TRUE(v.InDelta());
	EXPECT_FALSE(v.IsLive());
	EXPECT_FALSE(v.IsEvicted());
	EXPECT_EQ(v.HeldMode(), ViewMode::Live);
	EXPECT_TRUE(v.Delta().layer == kTemp);
	EXPECT_TRUE(v.Delta().autoScale);
	EXPECT_TRUE(v.ExitDelta());
	EXPECT_TRUE(v.IsLive());
	EXPECT_FALSE(v.InDelta());
}

TEST(HistoryViewState, DeltaFromSampleAndOffReturnsSample)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 6);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(3, res));
	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 1), res));
	EXPECT_EQ(v.HeldMode(), ViewMode::Sample);
	EXPECT_EQ(v.HeldSample(), 3u);
	EXPECT_EQ(v.Sample(), 3u);
	EXPECT_TRUE(v.ExitDelta());
	EXPECT_EQ(v.Mode(), ViewMode::Sample);
	EXPECT_EQ(v.Sample(), 3u);
}

TEST(HistoryViewState, ReplacingDeltaKeepsOriginalHeldView)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 6);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(3, res));
	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 1), res));
	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Sample, 4), MakeSide(SideKind::Sample, 0), res));
	EXPECT_EQ(v.Delta().a.sample, 4u);
	EXPECT_EQ(v.HeldMode(), ViewMode::Sample);
	EXPECT_EQ(v.HeldSample(), 3u);
	EXPECT_TRUE(v.ExitDelta());
	EXPECT_EQ(v.Sample(), 3u);
	EXPECT_EQ(v.Mode(), ViewMode::Sample);
}

TEST(HistoryViewState, ViewAndLiveLeaveDeltaAndDropHeld)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 6);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 1), res));
	ASSERT_TRUE(v.ViewSample(2, res));
	EXPECT_EQ(v.Mode(), ViewMode::Sample);
	EXPECT_EQ(v.Sample(), 2u);
	EXPECT_FALSE(v.ExitDelta());
	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 1), res));
	v.GoLive();
	EXPECT_TRUE(v.IsLive());
	EXPECT_FALSE(v.ExitDelta());
	EXPECT_TRUE(v.IsLive());
}

TEST(HistoryViewState, DeltaEpochBumpsOnEnterExitScaleAndNotOnNoOp)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 5);
	HistoryViewState v(*r.rec);
	ViewResult res;
	EXPECT_FALSE(v.ExitDelta());
	EXPECT_EQ(v.Epoch(), 0u);
	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 1), res));
	uint64_t e = v.Epoch();
	EXPECT_EQ(e, 1u);
	ASSERT_TRUE(v.SetDeltaScaleAuto(res));
	EXPECT_EQ(v.Epoch(), e) << "auto -> auto is a no-op";
	ASSERT_TRUE(v.SetDeltaScaleFixed(5.0f, res));
	EXPECT_EQ(v.Epoch(), e + 1); e = v.Epoch();
	EXPECT_FALSE(v.Delta().autoScale);
	EXPECT_EQ(v.Delta().fixedScale, 5.0f);
	ASSERT_TRUE(v.SetDeltaScaleFixed(5.0f, res));
	EXPECT_EQ(v.Epoch(), e);
	ASSERT_TRUE(v.SetDeltaScaleFixed(6.0f, res));
	EXPECT_EQ(v.Epoch(), e + 1); e = v.Epoch();
	ASSERT_TRUE(v.SetDeltaScaleAuto(res));
	EXPECT_EQ(v.Epoch(), e + 1); e = v.Epoch();
	EXPECT_TRUE(v.Delta().autoScale);
	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 1), res));
	EXPECT_EQ(v.Epoch(), e) << "identical re-enter is a no-op";
	ASSERT_TRUE(v.ExitDelta());
	EXPECT_EQ(v.Epoch(), e + 1);
}

TEST(HistoryViewState, DeltaRejectionsChangeNothing)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 5);
	HistoryViewState v(*r.rec);
	ViewResult res;
	LogCapture log("HIS");
	EXPECT_FALSE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Now), res));
	EXPECT_FALSE(res.mOk);
	EXPECT_GT(strlen(res.mError), 0u);
	EXPECT_FALSE(v.EnterDelta(kTemp, MakeSide(SideKind::Sample, 2), MakeSide(SideKind::Sample, 2), res));
	EXPECT_FALSE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 5), res));
	EXPECT_NE(std::string(res.mError).find("out of range"), std::string::npos);
	EXPECT_FALSE(v.EnterDelta(kTemp, MakeSide(SideKind::Sample, 9), MakeSide(SideKind::Now), res));
	const float bad[] = { 0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() };
	for (float s : bad)
	{
		EXPECT_FALSE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 1), res, false, s));
	}
	EXPECT_TRUE(v.IsLive());
	EXPECT_EQ(v.Epoch(), 0u);
	EXPECT_EQ(log.Count(LogLevel::kError, ""), 8u);

	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 1), res));
	const uint64_t e = v.Epoch();
	for (float s : bad) { EXPECT_FALSE(v.SetDeltaScaleFixed(s, res)); }
	EXPECT_TRUE(v.Delta().autoScale);
	EXPECT_FALSE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Now), res));
	EXPECT_EQ(v.Delta().b.sample, 1u);
	EXPECT_EQ(v.Epoch(), e);
	v.ExitDelta();
	EXPECT_FALSE(v.SetDeltaScaleFixed(2.0f, res)) << "not in delta";
	EXPECT_FALSE(v.SetDeltaScaleAuto(res));
}

TEST(HistoryViewState, DeltaEvictedSampleSideIsAllowedAndUpdateIgnoresIt)
{
	LogCapture log("HIS");
	Rig r; AddLayers(r);
	StartWithWindow3(r);
	TakeN(r, 4);	// window [1..3]
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 1), res));
	const uint64_t e = v.Epoch();
	TakeN(r, 4);	// sample 1 evicted
	EXPECT_FALSE(v.Update());
	EXPECT_TRUE(v.InDelta());
	EXPECT_EQ(v.Epoch(), e);
	EXPECT_EQ(log.Count(LogLevel::kWarning, "delta"), 0u);
	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 0), res)) << "evicted side allowed";
}

TEST(HistoryViewState, DeltaTruncatedSideLeavesDeltaWithOneWarning)
{
	LogCapture log("HIS");
	Rig r; AddLayers(r); r.Start(); TakeN(r, 6);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(1, res));
	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 4), res));
	const uint64_t e = v.Epoch();
	r.rec->TruncateAfter(2);
	EXPECT_TRUE(v.Update());
	EXPECT_EQ(v.Mode(), ViewMode::Sample);
	EXPECT_EQ(v.Sample(), 1u);
	EXPECT_NE(v.Epoch(), e);
	EXPECT_EQ(log.Count(LogLevel::kWarning, "delta"), 1u);
	EXPECT_FALSE(v.Update());
	EXPECT_EQ(log.Count(LogLevel::kWarning, "delta"), 1u);
}

TEST(HistoryViewState, DeltaTruncatedHeldSampleGoesLive)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 6);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(5, res));
	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 1), res));
	r.rec->TruncateAfter(2);	// held 5 gone, side 1 remains
	EXPECT_FALSE(v.Update());	// no side lost: still in delta
	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Now), MakeSide(SideKind::Sample, 2), res));
	r.rec->TruncateAfter(1);	// side 2 gone and held 5 gone
	EXPECT_TRUE(v.Update());
	EXPECT_TRUE(v.IsLive());
}

TEST(HistoryViewState, FormatSideText)
{
	char buf[64];
	FormatSide(MakeSide(SideKind::Now), buf, sizeof(buf));
	EXPECT_STREQ(buf, "now");
	FormatSide(MakeSide(SideKind::Sample, 7), buf, sizeof(buf));
	EXPECT_STREQ(buf, "sample:7");
	Side s; s.kind = SideKind::Snapshot; s.snapshot = Dia::Core::Containers::String64("%s", "before");
	FormatSide(s, buf, sizeof(buf));
	EXPECT_STREQ(buf, "snapshot:before");
}

TEST(HistoryViewState, NotifyHistoryResetBumpsTheEpochEvenWhenAlreadyLive)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 3);
	HistoryViewState v(*r.rec);
	const uint64_t e0 = v.Epoch();
	v.NotifyHistoryReset();
	EXPECT_TRUE(v.IsLive());
	EXPECT_EQ(v.Epoch(), e0 + 1);
}

TEST(HistoryViewState, NotifyHistoryResetFromScrubAndDeltaGoesLiveWithOneEpochBump)
{
	Rig r; AddLayers(r); r.Start(); TakeN(r, 4);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(1, res));
	uint64_t e = v.Epoch();
	v.NotifyHistoryReset();
	EXPECT_TRUE(v.IsLive());
	EXPECT_EQ(v.Epoch(), e + 1);

	ASSERT_TRUE(v.EnterDelta(kTemp, MakeSide(SideKind::Sample, 0), MakeSide(SideKind::Sample, 2), res));
	e = v.Epoch();
	v.NotifyHistoryReset();
	EXPECT_TRUE(v.IsLive());
	EXPECT_FALSE(v.InDelta());
	EXPECT_EQ(v.Epoch(), e + 1);
	EXPECT_FALSE(v.ExitDelta());
}
