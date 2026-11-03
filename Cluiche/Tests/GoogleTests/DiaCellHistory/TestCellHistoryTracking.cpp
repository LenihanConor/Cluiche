#include "HistoryTestRig.h"
#include <DiaCellLayers/Testing/LogCapture.h>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::CellLayers::Testing::LogCapture;
using Dia::Core::StringCRC;

namespace
{
	typedef CellHistoryRecorder::TrackResult TrackResult;
	typedef CellHistoryRecorder::HistoryStatus HistoryStatus;

	struct RingState
	{
		uint32_t tracked, window, retained;
		uint64_t bytes;
		bool operator==(const RingState& o) const { return tracked == o.tracked && window == o.window && retained == o.retained && bytes == o.bytes; }
	};
	RingState StateOf(const CellHistoryRecorder& rec)
	{
		const CellRing* ring = rec.Ring();
		if (ring == nullptr) { return { 0, 0, 0, 0 }; }
		return { ring->TrackedCount(), ring->WindowLength(), ring->RetainedCount(), ring->BytesUsed() };
	}

	// temp (float 0..100), biome (category), raw (float without range), hidden (aggregate:false) on `grid`; oth on `other`.
	void SetUpLayers(Rig& r)
	{
		r.grid.AddFloatLayer("temp", 40.0f);
		r.grid.AddCategoryLayer("biome", 1);
		r.grid.AddFloatLayer("raw", 5.0f);
		r.grid.AddFloatLayer("hidden", 5.0f);
		r.other.AddFloatLayer("oth", 1.0f);
		ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3), RangelessFloatEntry("raw"),
			FloatEntry("hidden", 0, 10, "{\"aggregate\":false}"), FloatEntry("oth", 0, 10) }));
	}

	bool TrackOne(CellHistoryRecorder& rec, const char* id, TrackResult& out) { const StringCRC c(id); return rec.Track(&c, 1, out); }
}

TEST(CellHistoryTracking, LateTrackedLayerHasDataOnlyFromTheNextSample)
{
	Rig r;
	SetUpLayers(r);
	r.Start();
	for (int i = 0; i < 3; ++i) { r.Take(); }
	TrackResult res;
	ASSERT_TRUE(TrackOne(*r.rec, "temp", res)) << res.mError;
	EXPECT_TRUE(res.mOk);
	EXPECT_EQ(res.mChanged, 1u);
	EXPECT_GT(res.mWindowLength, 3u);
	EXPECT_EQ(res.mWindowLength, r.rec->Ring()->WindowLength());
	EXPECT_TRUE(r.rec->IsTracked(StringCRC("temp")));
	r.Take();
	r.Take();
	float v[12];
	for (uint32_t s = 0; s < 3; ++s) { EXPECT_FALSE(r.rec->CellValues(s, StringCRC("temp"), v, 12)) << s; }
	for (uint32_t s = 3; s < 5; ++s)
	{
		ASSERT_TRUE(r.rec->CellValues(s, StringCRC("temp"), v, 12)) << s;
		EXPECT_NEAR(v[0], 40.0f, 100.0f / 65535.0f);
	}
	EXPECT_EQ(r.rec->SampleCount(), 5u);
}

TEST(CellHistoryTracking, TrackBeforeTheFirstSampleResolvesTheLayout)
{
	Rig r;
	SetUpLayers(r);
	r.Start();
	TrackResult res;
	ASSERT_TRUE(TrackOne(*r.rec, "biome", res)) << res.mError;
	r.Take();
	float v[12];
	ASSERT_TRUE(r.rec->CellValues(0, StringCRC("biome"), v, 12));
	EXPECT_FLOAT_EQ(v[0], 1.0f);
}

TEST(CellHistoryTracking, TrackingChangesBytesPerSampleAndShrinksTheWindowDroppingTheOldest)
{
	Rig r;
	SetUpLayers(r);
	r.cfg.mTrackCount = 1;
	r.cfg.mTrack[0] = Dia::Core::Containers::String64("%s", "biome");
	RecorderBudgets b;
	b.mAggregateBytes = 1 << 20;
	b.mCellBytes = BudgetBytesForWindow(6, BytesPerSample(0, 1, 12));	// 6 samples of the category layer alone
	b.mEventBytes = 4096;
	r.Start(&b);
	for (int i = 0; i < 6; ++i) { r.Take(); }
	ASSERT_EQ(r.rec->Ring()->WindowLength(), 6u);
	ASSERT_EQ(r.rec->FirstRetained(), 0u);

	TrackResult res;
	ASSERT_TRUE(TrackOne(*r.rec, "temp", res)) << res.mError;
	EXPECT_EQ(res.mWindowLength, WindowLengthFor(b.mCellBytes, BytesPerSample(1, 1, 12)));	// 72 / 36 = 2
	EXPECT_EQ(res.mWindowLength, 2u);
	EXPECT_EQ(r.rec->FirstRetained(), 4u);	// samples 0..3 dropped, 4 and 5 kept
	float v[12];
	EXPECT_FALSE(r.rec->CellValues(3, StringCRC("biome"), v, 12));
	EXPECT_TRUE(r.rec->CellValues(4, StringCRC("biome"), v, 12));
	EXPECT_TRUE(r.rec->CellValues(5, StringCRC("biome"), v, 12));
	EXPECT_FALSE(r.rec->CellValues(5, StringCRC("temp"), v, 12));	// temp has data from the next sample on
	r.Take();
	EXPECT_TRUE(r.rec->CellValues(6, StringCRC("temp"), v, 12));
}

TEST(CellHistoryTracking, UntrackFreesTheRingAndTheWindowGrows)
{
	Rig r;
	SetUpLayers(r);
	r.cfg.mTrackCount = 2;
	r.cfg.mTrack[0] = Dia::Core::Containers::String64("%s", "biome");
	r.cfg.mTrack[1] = Dia::Core::Containers::String64("%s", "temp");
	RecorderBudgets b;
	b.mAggregateBytes = 1 << 20;
	b.mCellBytes = BudgetBytesForWindow(4, BytesPerSample(1, 1, 12));
	b.mEventBytes = 4096;
	r.Start(&b);
	for (int i = 0; i < 3; ++i) { r.Take(); }
	ASSERT_EQ(r.rec->Ring()->WindowLength(), 4u);

	TrackResult res;
	const StringCRC temp("temp");
	ASSERT_TRUE(r.rec->Untrack(&temp, 1, res)) << res.mError;
	EXPECT_EQ(res.mChanged, 1u);
	EXPECT_EQ(res.mWindowLength, 12u);	// 144 / 12
	EXPECT_FALSE(r.rec->IsTracked(temp));
	EXPECT_EQ(r.rec->Ring()->TrackedCount(), 1u);
	float v[12];
	EXPECT_FALSE(r.rec->CellValues(2, temp, v, 12));
	EXPECT_TRUE(r.rec->CellValues(2, StringCRC("biome"), v, 12));	// the other ring keeps its data

	const StringCRC biome("biome");
	ASSERT_TRUE(r.rec->Untrack(&biome, 1, res)) << res.mError;
	EXPECT_EQ(r.rec->Ring()->TrackedCount(), 0u);
	EXPECT_EQ(r.rec->Ring()->BytesUsed(), 0u);
	EXPECT_FALSE(r.rec->CellValues(2, biome, v, 12));
	r.Take();	// sampling with nothing tracked keeps working
	AggregateStats st;
	EXPECT_TRUE(r.rec->Aggregate(3, temp, StringCRC("world"), st));
}

TEST(CellHistoryTracking, UntrackedLayerCanBeTrackedAgain)
{
	Rig r;
	SetUpLayers(r);
	r.Start();
	r.Take();
	TrackResult res;
	ASSERT_TRUE(TrackOne(*r.rec, "temp", res));
	r.Take();
	const StringCRC temp("temp");
	ASSERT_TRUE(r.rec->Untrack(&temp, 1, res));
	r.Take();
	ASSERT_TRUE(TrackOne(*r.rec, "temp", res));
	r.Take();
	float v[12];
	EXPECT_FALSE(r.rec->CellValues(0, temp, v, 12));
	EXPECT_FALSE(r.rec->CellValues(2, temp, v, 12));
	EXPECT_TRUE(r.rec->CellValues(3, temp, v, 12));
}

TEST(CellHistoryTracking, EachInvalidLayerIsRejectedWithAnErrorAndChangesNothing)
{
	struct Case { const char* layer; const char* reasonSubstr; };
	const Case cases[] = {
		{ "nonexistent", "does not exist" },
		{ "oth", "another grid" },
		{ "raw", "min/max" },
		{ "hidden", "not a recorded layer" } };
	for (const Case& c : cases)
	{
		LogCapture log("HIS");
		Rig r;
		SetUpLayers(r);
		r.Start();
		r.Take();
		const RingState before = StateOf(*r.rec);
		TrackResult res;
		EXPECT_FALSE(TrackOne(*r.rec, c.layer, res)) << c.layer;
		EXPECT_FALSE(res.mOk) << c.layer;
		EXPECT_NE(std::string(res.mError).find(c.layer), std::string::npos) << c.layer << ": " << res.mError;
		EXPECT_GE(log.Errors(c.reasonSubstr), 1u) << c.layer;
		EXPECT_GE(log.Errors(c.layer), 1u) << c.layer;
		EXPECT_FALSE(r.rec->IsTracked(StringCRC(c.layer))) << c.layer;
		EXPECT_TRUE(before == StateOf(*r.rec)) << c.layer;
	}
}

TEST(CellHistoryTracking, FloatLayerWithoutAnySchemaIsRejected)
{
	LogCapture log("HIS");
	Rig r;
	r.grid.AddFloatLayer("temp", 1.0f);
	r.Start();
	r.Take();
	TrackResult res;
	EXPECT_FALSE(TrackOne(*r.rec, "temp", res));
	EXPECT_GE(log.Errors("min/max"), 1u);
}

TEST(CellHistoryTracking, MixedValidAndInvalidListChangesNothing)
{
	LogCapture log("HIS");
	Rig r;
	SetUpLayers(r);
	r.Start();
	r.Take();
	const RingState before = StateOf(*r.rec);
	const StringCRC list[3] = { StringCRC("temp"), StringCRC("biome"), StringCRC("nonexistent") };
	TrackResult res;
	EXPECT_FALSE(r.rec->Track(list, 3, res));
	EXPECT_GE(log.Errors("nonexistent"), 1u);
	EXPECT_FALSE(r.rec->IsTracked(StringCRC("temp")));
	EXPECT_FALSE(r.rec->IsTracked(StringCRC("biome")));
	EXPECT_TRUE(before == StateOf(*r.rec));
	r.Take();
	float v[12];
	EXPECT_FALSE(r.rec->CellValues(1, StringCRC("temp"), v, 12));
}

TEST(CellHistoryTracking, ValidListTracksAllLayers)
{
	Rig r;
	SetUpLayers(r);
	r.Start();
	r.Take();
	const StringCRC list[2] = { StringCRC("temp"), StringCRC("biome") };
	TrackResult res;
	ASSERT_TRUE(r.rec->Track(list, 2, res)) << res.mError;
	EXPECT_EQ(res.mChanged, 2u);
	EXPECT_TRUE(r.rec->IsTracked(StringCRC("temp")));
	EXPECT_TRUE(r.rec->IsTracked(StringCRC("biome")));
}

TEST(CellHistoryTracking, EmptyListIsRejected)
{
	LogCapture log("HIS");
	Rig r;
	SetUpLayers(r);
	r.Start();
	r.Take();
	TrackResult res;
	EXPECT_FALSE(r.rec->Track(nullptr, 0, res));
	EXPECT_GE(log.Errors("no layers"), 1u);
	const StringCRC c("temp");
	EXPECT_FALSE(r.rec->Untrack(&c, 0, res));
	EXPECT_GE(log.Errors("no layers"), 2u);
}

TEST(CellHistoryTracking, RetrackingIsAnIdempotentSuccess)
{
	LogCapture log("HIS");
	Rig r;
	SetUpLayers(r);
	r.Start();
	r.Take();
	TrackResult first;
	ASSERT_TRUE(TrackOne(*r.rec, "temp", first));
	r.Take();
	const RingState before = StateOf(*r.rec);
	TrackResult second;
	EXPECT_TRUE(TrackOne(*r.rec, "temp", second));
	EXPECT_EQ(second.mChanged, 0u);
	EXPECT_EQ(second.mWindowLength, first.mWindowLength);
	EXPECT_TRUE(before == StateOf(*r.rec));
	EXPECT_EQ(log.Errors(), 0u);
	// Duplicates inside one list count once.
	const StringCRC dup[2] = { StringCRC("biome"), StringCRC("biome") };
	TrackResult third;
	ASSERT_TRUE(r.rec->Track(dup, 2, third));
	EXPECT_EQ(third.mChanged, 1u);
}

TEST(CellHistoryTracking, TrackAndUntrackLogInfoWithTheWindowLength)
{
	LogCapture log("HIS");
	Rig r;
	SetUpLayers(r);
	r.Start();
	r.Take();
	TrackResult res;
	ASSERT_TRUE(TrackOne(*r.rec, "temp", res));
	const std::string window = std::to_string(res.mWindowLength);
	EXPECT_GE(log.Count(Dia::Observation::Log::LogLevel::kInfo, ("window is now " + window).c_str()), 1u);
	const StringCRC temp("temp");
	ASSERT_TRUE(r.rec->Untrack(&temp, 1, res));
	EXPECT_GE(log.Count(Dia::Observation::Log::LogLevel::kInfo, "untracked 1 layer"), 1u);
}

TEST(CellHistoryTracking, UntrackRejectsUnknownAndUntrackedLayersAndChangesNothing)
{
	LogCapture log("HIS");
	Rig r;
	SetUpLayers(r);
	r.Start();
	r.Take();
	TrackResult res;
	ASSERT_TRUE(TrackOne(*r.rec, "temp", res));
	const RingState before = StateOf(*r.rec);
	const StringCRC unknown("nonexistent"), notTracked("biome"), temp("temp");
	EXPECT_FALSE(r.rec->Untrack(&unknown, 1, res));
	EXPECT_FALSE(res.mOk);
	EXPECT_FALSE(r.rec->Untrack(&notTracked, 1, res));
	const StringCRC mixed[2] = { temp, unknown };
	EXPECT_FALSE(r.rec->Untrack(mixed, 2, res));
	EXPECT_GE(log.Errors("nonexistent"), 2u);
	EXPECT_GE(log.Errors("biome"), 1u);
	EXPECT_TRUE(r.rec->IsTracked(temp));
	EXPECT_TRUE(before == StateOf(*r.rec));
}

TEST(CellHistoryTracking, TrackFailsCleanlyWhenTheCellBudgetCannotHoldTheLayer)
{
	LogCapture log("HIS");
	Rig r;
	SetUpLayers(r);
	r.cfg.mTrackCount = 1;
	r.cfg.mTrack[0] = Dia::Core::Containers::String64("%s", "biome");
	RecorderBudgets b;
	b.mAggregateBytes = 1 << 20;
	b.mCellBytes = 20;	// holds 1 sample of the category layer (12) but not category + float (36)
	b.mEventBytes = 4096;
	r.Start(&b);
	r.Take();
	const RingState before = StateOf(*r.rec);
	TrackResult res;
	EXPECT_FALSE(TrackOne(*r.rec, "temp", res));
	EXPECT_GE(log.Errors(), 1u);
	EXPECT_FALSE(r.rec->IsTracked(StringCRC("temp")));
	EXPECT_TRUE(before == StateOf(*r.rec));
}

TEST(CellHistoryTracking, StatusReportsEveryField)
{
	Rig r;
	SetUpLayers(r);
	r.cfg.mTrackCount = 1;
	r.cfg.mTrack[0] = Dia::Core::Containers::String64("%s", "biome");
	RecorderBudgets b;
	b.mAggregateBytes = BudgetBytesForWindow(5, AggregateRowBytes(1, 3, 1));	// temp (float) + biome (3 values), world only
	b.mCellBytes = BudgetBytesForWindow(3, BytesPerSample(0, 1, 12));
	b.mEventBytes = 4096;
	r.Start(&b);
	for (int i = 0; i < 8; ++i) { r.Take(); }
	HistoryEvent e;
	e.type = StringCRC("fixture.event");
	r.rec->Emit(e);
	r.Take();	// commit to sample 8

	HistoryStatus s;
	r.rec->BuildStatus(s);
	EXPECT_EQ(s.mSampleCount, 9u);
	EXPECT_EQ(s.mRunId, r.rec->RunId());
	EXPECT_EQ(s.mRecordedLayers, r.rec->RecordedLayerCount());
	EXPECT_EQ(s.mSkippedLayers, r.rec->SkippedLayerCount());
	EXPECT_EQ(s.mVersionGatedSkips, r.rec->VersionGatedSkips());
	EXPECT_EQ(s.mWindowLength, 3u);
	EXPECT_EQ(s.mCellRetained, 3u);
	EXPECT_EQ(s.mCellFirst, 6u);
	EXPECT_EQ(s.mCellLatest, 8u);
	EXPECT_EQ(s.mCellBudget, b.mCellBytes);
	EXPECT_EQ(s.mCellBytes, r.rec->Ring()->BytesUsed());
	EXPECT_GT(s.mCellBytes, 0u);
	EXPECT_EQ(s.mAggregateWindow, r.rec->Aggregates().WindowLength());
	EXPECT_EQ(s.mAggregateFirst, r.rec->AggregateFirstRetained());
	EXPECT_EQ(s.mAggregateLatest, 8u);
	EXPECT_EQ(s.mAggregateBudget, b.mAggregateBytes);
	EXPECT_EQ(s.mAggregateBytes, r.rec->Aggregates().BytesUsed());
	EXPECT_EQ(s.mEventBudget, b.mEventBytes);
	EXPECT_EQ(s.mEventCount, 1u);
	EXPECT_EQ(s.mEventLatest, 8u);
	EXPECT_GT(s.mEventBytes, 0u);
	ASSERT_EQ(s.mTrackedCount, 1u);
	EXPECT_EQ(s.mTracked[0], StringCRC("biome"));
	// Eviction shows up as gaps.
	ASSERT_GE(s.mCellGapCount, 1u);
	EXPECT_EQ(s.mCellGaps[0].mFirst, 0u);
	EXPECT_EQ(s.mCellGaps[s.mCellGapCount - 1].mLast, 5u);
	ASSERT_GE(s.mAggregateGapCount, 1u);
	EXPECT_EQ(s.mAggregateGaps[0].mFirst, 0u);
	EXPECT_EQ(s.mEventGapCount, 0u);
}

TEST(CellHistoryTracking, StatusBeforeAnySampleIsEmpty)
{
	Rig r;
	SetUpLayers(r);
	r.Start();
	HistoryStatus s;
	r.rec->BuildStatus(s);
	EXPECT_EQ(s.mSampleCount, 0u);
	EXPECT_EQ(s.mWindowLength, 0u);
	EXPECT_EQ(s.mTrackedCount, 0u);
	EXPECT_EQ(s.mCellGapCount, 0u);
	EXPECT_EQ(s.mCellBytes, 0u);
}
