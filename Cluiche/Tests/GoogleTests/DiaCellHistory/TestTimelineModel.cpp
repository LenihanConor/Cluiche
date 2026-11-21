#include "HistoryTestRig.h"
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/TimelineModel.h>
#include <DiaCellHistory/Testing/FixtureEventSchemas.h>

#include <cmath>
#include <limits>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using Dia::Core::StringCRC;

namespace
{
	std::string EventEntry(const char* key, const char* colour)
	{
		std::string s = std::string("{\"key\":\"") + key + "\",\"kind\":\"event\",\"type\":\"string\",\"label\":\"E\",\"description\":\"D\"";
		if (colour) { s += std::string(",\"ui\":{\"colour\":\"") + colour + "\"}"; }
		return s + "}";
	}

	void AddLayers(Rig& r, bool withEvents = true)
	{
		r.grid.AddFloatLayer("temp", 0.0f);
		r.grid.AddCategoryLayer("biome", 1);
		std::vector<std::string> entries = { FloatEntry("temp", 0, 100000), EnumEntry("biome", 3) };
		if (withEvents)
		{
			entries.push_back(EventEntry("ev.red", "#ff0000"));
			entries.push_back(EventEntry("ev.blue", "#0000ff"));
			entries.push_back(EventEntry("ev.plain", nullptr));
			entries.push_back(EventEntry("ev.d", "#010203"));
			entries.push_back(EventEntry("ev.e", "#040506"));
			entries.push_back(EventEntry("ev.f", "#070809"));
		}
		ASSERT_TRUE(r.schema.Load(entries));
	}

	// Samples with temp == sample index.
	void TakeRamp(Rig& r, int n)
	{
		for (int i = 0; i < n; ++i)
		{
			r.grid.SetAll("temp", float(r.rec->SampleCount())); r.grid.BumpVersion("temp");
			r.Take();
		}
	}

	void EmitEvent(Rig& r, const char* type)
	{
		HistoryEvent e;
		e.type = StringCRC(type);
		r.rec->Emit(e);
	}

	TimelineModel Built(Rig& r, uint32_t buckets = TimelineModel::kDefaultBucketCount, const char* layer = "temp")
	{
		HistoryViewState v(*r.rec);
		TimelineModel m;
		m.Build(*r.rec, v, StringCRC(layer), r.schema.service.get(), &r.cfg, buckets);
		return m;
	}
}

TEST(TimelineModel, EmptyRecorder)
{
	Rig r; AddLayers(r); r.Start();
	HistoryViewState v(*r.rec);
	TimelineModel m;
	EXPECT_TRUE(m.NeedsRebuild(*r.rec, v, StringCRC("temp")));
	m.Build(*r.rec, v, StringCRC("temp"), r.schema.service.get(), &r.cfg);
	EXPECT_EQ(m.Range().mCount, 0u);
	EXPECT_EQ(m.BucketCount(), 0u);
	EXPECT_EQ(m.SparklineCount(), 0u);
	EXPECT_EQ(m.BandTickCount(), 0u);
	EXPECT_FALSE(m.HasLiveMarker());
	EXPECT_FALSE(m.PreWindowDim().mPresent);
	EXPECT_FALSE(m.NeedsRebuild(*r.rec, v, StringCRC("temp")));
}

TEST(TimelineModel, OneSample)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 1);
	TimelineModel m = Built(r);
	EXPECT_EQ(m.Range().mCount, 1u);
	EXPECT_EQ(m.Range().mFirstSample, 0u);
	EXPECT_EQ(m.Range().mLatestSample, 0u);
	ASSERT_EQ(m.BucketCount(), 1u);
	EXPECT_EQ(m.Bucket(0).mFirstSample, 0u);
	EXPECT_EQ(m.Bucket(0).mLastSample, 0u);
	EXPECT_TRUE(m.HasLiveMarker());
	EXPECT_EQ(m.LiveMarker(), 0u);
	EXPECT_EQ(m.Position().mMode, ViewMode::Live);
	EXPECT_EQ(m.Position().mSample, 0u);
	ASSERT_EQ(m.SparklineCount(), 1u);
	EXPECT_TRUE(m.SparklinePoint(0).mValid);
}

TEST(TimelineModel, BucketsPartitionAllSamplesExactlyOnce)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 2080);
	TimelineModel m = Built(r, 256);
	ASSERT_EQ(m.BucketCount(), 256u);
	uint32_t expectedFirst = 0;
	uint64_t covered = 0;
	for (uint32_t i = 0; i < m.BucketCount(); ++i)
	{
		const TimelineBucket& b = m.Bucket(i);
		EXPECT_EQ(b.mFirstSample, expectedFirst) << "bucket " << i;
		ASSERT_GE(b.mLastSample, b.mFirstSample) << "bucket " << i;
		covered += b.mLastSample - b.mFirstSample + 1;
		expectedFirst = b.mLastSample + 1;
	}
	EXPECT_EQ(expectedFirst, 2080u);
	EXPECT_EQ(covered, 2080u);
}

TEST(TimelineModel, UnevenDivisionStillPartitionsExactly)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 1000);
	TimelineModel m = Built(r, 7);
	ASSERT_EQ(m.BucketCount(), 7u);
	uint32_t next = 0;
	for (uint32_t i = 0; i < 7; ++i)
	{
		EXPECT_EQ(m.Bucket(i).mFirstSample, next);
		next = m.Bucket(i).mLastSample + 1;
	}
	EXPECT_EQ(next, 1000u);
}

TEST(TimelineModel, BucketCountLargerThanSamplesGivesOnePerSample)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 5);
	TimelineModel m = Built(r, 1000);
	ASSERT_EQ(m.BucketCount(), 5u);
	for (uint32_t i = 0; i < 5; ++i)
	{
		EXPECT_EQ(m.Bucket(i).mFirstSample, i);
		EXPECT_EQ(m.Bucket(i).mLastSample, i);
	}
}

TEST(TimelineModel, BucketCountClamped)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 5000);
	TimelineModel zero = Built(r, 0);
	ASSERT_EQ(zero.BucketCount(), 1u);
	EXPECT_EQ(zero.Bucket(0).mFirstSample, 0u);
	EXPECT_EQ(zero.Bucket(0).mLastSample, 4999u);
	TimelineModel huge = Built(r, 100000);
	EXPECT_EQ(huge.BucketCount(), TimelineModel::kMaxBucketCount);
	EXPECT_EQ(huge.Bucket(huge.BucketCount() - 1).mLastSample, 4999u);
}

TEST(TimelineModel, SingleEventIsNeverHiddenAtCoarseZoom)
{
	Rig r; AddLayers(r); r.Start();
	TakeRamp(r, 700);
	EmitEvent(r, "ev.red");
	TakeRamp(r, 1380);	// the event belongs to sample 700
	TimelineModel m = Built(r, 16);
	uint32_t withEvents = 0;
	uint32_t total = 0;
	for (uint32_t i = 0; i < m.BucketCount(); ++i)
	{
		total += m.Bucket(i).mEventCount;
		if (m.Bucket(i).mEventCount > 0)
		{
			++withEvents;
			EXPECT_GE(700u, m.Bucket(i).mFirstSample);
			EXPECT_LE(700u, m.Bucket(i).mLastSample);
			EXPECT_EQ(m.Bucket(i).mColour, 0xFF0000FFu);
			ASSERT_EQ(m.Bucket(i).mTypeCount, 1u);
			EXPECT_EQ(m.Bucket(i).mTypes[0].mCount, 1u);
		}
	}
	EXPECT_EQ(withEvents, 1u);
	EXPECT_EQ(total, 1u);
}

TEST(TimelineModel, EventDensityCountsAndDominantType)
{
	Rig r; AddLayers(r); r.Start();
	// Samples 0..9 land in one bucket at bucketCount 1.
	EmitEvent(r, "ev.blue"); EmitEvent(r, "ev.red"); EmitEvent(r, "ev.red"); TakeRamp(r, 1);
	EmitEvent(r, "ev.red"); TakeRamp(r, 1);
	EmitEvent(r, "ev.blue"); TakeRamp(r, 8);
	TimelineModel m = Built(r, 1);
	ASSERT_EQ(m.BucketCount(), 1u);
	const TimelineBucket& b = m.Bucket(0);
	EXPECT_EQ(b.mEventCount, 5u);
	ASSERT_EQ(b.mTypeCount, 2u);
	EXPECT_EQ(b.mTypes[0].mType.Value(), StringCRC("ev.red").Value());
	EXPECT_EQ(b.mTypes[0].mCount, 3u);
	EXPECT_EQ(b.mTypes[1].mType.Value(), StringCRC("ev.blue").Value());
	EXPECT_EQ(b.mTypes[1].mCount, 2u);
	EXPECT_EQ(b.mColour, 0xFF0000FFu);
}

TEST(TimelineModel, TiedTypesTakeFirstAppearingAsDominant)
{
	Rig r; AddLayers(r); r.Start();
	EmitEvent(r, "ev.blue"); EmitEvent(r, "ev.red"); TakeRamp(r, 3);
	TimelineModel m = Built(r, 1);
	ASSERT_EQ(m.Bucket(0).mTypeCount, 2u);
	EXPECT_EQ(m.Bucket(0).mColour, 0x0000FFFFu);
}

TEST(TimelineModel, PerTypeListIsCapped)
{
	Rig r; AddLayers(r); r.Start();
	const char* types[] = { "ev.red", "ev.blue", "ev.plain", "ev.d", "ev.e", "ev.f" };
	for (const char* t : types) { EmitEvent(r, t); }
	TakeRamp(r, 2);
	TimelineModel m = Built(r, 1);
	EXPECT_EQ(m.Bucket(0).mEventCount, 6u);
	EXPECT_EQ(m.Bucket(0).mTypeCount, TimelineBucket::kMaxTypesPerBucket);
}

TEST(TimelineModel, EventColoursComeFromSchemaWithDefaultFallback)
{
	Rig r; AddLayers(r); r.Start();
	EmitEvent(r, "ev.blue"); TakeRamp(r, 1);
	EmitEvent(r, "ev.plain"); TakeRamp(r, 1);		// schema entry without ui.colour
	EmitEvent(r, "ev.not.in.schema"); TakeRamp(r, 1);
	TimelineModel m = Built(r, 16);
	ASSERT_EQ(m.BucketCount(), 3u);
	EXPECT_EQ(m.Bucket(0).mColour, 0x0000FFFFu);
	EXPECT_EQ(m.Bucket(1).mColour, TimelineModel::kDefaultEventColour);
	EXPECT_EQ(m.Bucket(2).mColour, TimelineModel::kDefaultEventColour);
	EXPECT_EQ(m.Bucket(2).mEventCount, 1u);
}

TEST(TimelineModel, EventColoursFromFixtureSchemaWithoutUiAreDefault)
{
	FixtureSchemaSet fixture;
	ASSERT_TRUE(fixture.Load());
	Rig r;
	r.grid.AddFloatLayer("temp", 0.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100) }));
	r.Start();
	HistoryEvent e; e.type = StringCRC("test.simple");
	r.rec->Emit(e);
	r.Take();
	HistoryViewState v(*r.rec);
	TimelineModel m;
	m.Build(*r.rec, v, StringCRC("temp"), fixture.Service(), &r.cfg);
	ASSERT_EQ(m.BucketCount(), 1u);
	EXPECT_EQ(m.Bucket(0).mColour, TimelineModel::kDefaultEventColour);
}

TEST(TimelineModel, BandTicksMarkEveryYearStartWithNames)
{
	Rig r; AddLayers(r);
	r.cfg.mBandCount = 2;
	r.cfg.mBands[0] = Dia::Core::Containers::String32("%s", "Week");
	r.cfg.mBands[1] = Dia::Core::Containers::String32("%s", "Year");
	r.Start(); TakeRamp(r, 110);
	TimelineModel m = Built(r);
	ASSERT_EQ(m.BandTickCount(), 3u);
	EXPECT_EQ(m.BandTick(0).mSample, 0u);
	EXPECT_EQ(m.BandTick(1).mSample, 52u);
	EXPECT_EQ(m.BandTick(2).mSample, 104u);
	EXPECT_EQ(m.BandTick(1).mBand, 1);
	EXPECT_STREQ(m.BandTick(1).mName.AsCStr(), "Year");
	EXPECT_FALSE(m.BandTicksTruncated());
}

TEST(TimelineModel, BandTickNameFallsBackWhenNotConfigured)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 60);
	TimelineModel m = Built(r);
	ASSERT_EQ(m.BandTickCount(), 2u);
	EXPECT_STREQ(m.BandTick(0).mName.AsCStr(), "band1");
}

TEST(TimelineModel, NoGapsOrDimWhenNothingEvicted)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 20);
	TimelineModel m = Built(r);
	EXPECT_EQ(m.AggregateGapCount(), 0u);
	EXPECT_EQ(m.CellGapCount(), 0u);
	EXPECT_EQ(m.EventGapCount(), 0u);
	EXPECT_FALSE(m.PreWindowDim().mPresent);
}

TEST(TimelineModel, AggregateEvictionProducesGapsAndSparklineHoles)
{
	Rig r; AddLayers(r);
	RecorderBudgets b;
	b.mAggregateBytes = BudgetBytesForWindow(5, AggregateRowBytes(1, 3, 1));
	b.mCellBytes = 1 << 20;
	b.mEventBytes = 1 << 16;
	r.Start(&b); TakeRamp(r, 20);
	TimelineModel m = Built(r);
	ASSERT_GE(m.AggregateGapCount(), 1u);
	EXPECT_EQ(m.AggregateGap(0).mFirst, 0u);
	EXPECT_GE(m.AggregateGap(0).mLast, 1u);
	// Evicted samples have no aggregate: those sparkline points are invalid, the retained ones carry values.
	ASSERT_EQ(m.SparklineCount(), 20u);
	EXPECT_FALSE(m.SparklinePoint(0).mValid);
	EXPECT_TRUE(m.SparklinePoint(19).mValid);
	EXPECT_FLOAT_EQ(m.SparklinePoint(19).mValue, 19.0f);
}

TEST(TimelineModel, CellWindowEvictionGivesGapAndPreWindowDim)
{
	Rig r; AddLayers(r);
	r.cfg.mTrackCount = 1;
	r.cfg.mTrack[0] = Dia::Core::Containers::String64("%s", "biome");
	RecorderBudgets b;
	b.mAggregateBytes = 1 << 20;
	b.mCellBytes = BudgetBytesForWindow(3, BytesPerSample(0, 1, 12));
	b.mEventBytes = 1 << 16;
	r.Start(&b); TakeRamp(r, 20);
	TimelineModel m = Built(r);
	ASSERT_GE(m.CellGapCount(), 1u);
	EXPECT_EQ(m.CellGap(0).mFirst, 0u);
	ASSERT_TRUE(m.PreWindowDim().mPresent);
	EXPECT_EQ(m.PreWindowDim().mFirst, 0u);
	EXPECT_EQ(m.PreWindowDim().mLast, r.rec->FirstRetained() - 1);
	EXPECT_EQ(m.PreWindowDim().mLast, 16u);
	EXPECT_EQ(m.AggregateGapCount(), 0u);
}

TEST(TimelineModel, EventEvictionGivesEventGap)
{
	Rig r; AddLayers(r);
	RecorderBudgets b;
	b.mAggregateBytes = 1 << 20;
	b.mCellBytes = 1 << 20;
	b.mEventBytes = 256;
	r.Start(&b);
	for (int i = 0; i < 100; ++i) { EmitEvent(r, "ev.red"); TakeRamp(r, 1); }
	TimelineModel m = Built(r);
	ASSERT_GE(m.EventGapCount(), 1u);
	EXPECT_EQ(m.EventGap(0).mFirst, 0u);
	EXPECT_EQ(m.AggregateGapCount(), 0u);
}

TEST(TimelineModel, SparklineIsCappedAndCoversTheRun)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 2080);
	TimelineModel m = Built(r, 4096);
	ASSERT_EQ(m.SparklineCount(), TimelineModel::kMaxSparkPoints);
	EXPECT_EQ(m.SparklinePoint(0).mFirstSample, 0u);
	EXPECT_EQ(m.SparklinePoint(255).mLastSample, 2079u);
	// A ramp downsampled by mean-of-bucket stays monotonic.
	for (uint32_t i = 1; i < m.SparklineCount(); ++i)
	{
		ASSERT_TRUE(m.SparklinePoint(i).mValid);
		EXPECT_GT(m.SparklinePoint(i).mValue, m.SparklinePoint(i - 1).mValue);
	}
}

TEST(TimelineModel, SparklineExactValuesForSmallRun)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 6);
	TimelineModel m = Built(r);
	ASSERT_EQ(m.SparklineCount(), 6u);
	for (uint32_t i = 0; i < 6; ++i)
	{
		EXPECT_TRUE(m.SparklinePoint(i).mValid);
		EXPECT_EQ(m.SparklinePoint(i).mFirstSample, i);
		EXPECT_FLOAT_EQ(m.SparklinePoint(i).mValue, float(i));
	}
}

TEST(TimelineModel, SparklineMeanOfBucket)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 6);
	TimelineModel m = Built(r, 2);	// buckets [0..2] [3..5]; spark has one point per bucket here
	ASSERT_EQ(m.SparklineCount(), 2u);
	EXPECT_FLOAT_EQ(m.SparklinePoint(0).mValue, 1.0f);
	EXPECT_FLOAT_EQ(m.SparklinePoint(1).mValue, 4.0f);
}

TEST(TimelineModel, NaNAggregateIsAGapNotZero)
{
	Rig r; AddLayers(r); r.Start();
	r.grid.SetAll("temp", 5.0f); r.grid.BumpVersion("temp"); r.Take();
	r.grid.SetAll("temp", std::numeric_limits<float>::quiet_NaN()); r.grid.BumpVersion("temp"); r.Take();
	r.grid.SetAll("temp", 7.0f); r.grid.BumpVersion("temp"); r.Take();
	TimelineModel m = Built(r);
	ASSERT_EQ(m.SparklineCount(), 3u);
	EXPECT_TRUE(m.SparklinePoint(0).mValid);
	EXPECT_FALSE(m.SparklinePoint(1).mValid);
	EXPECT_TRUE(m.SparklinePoint(2).mValid);
	EXPECT_FLOAT_EQ(m.SparklinePoint(2).mValue, 7.0f);
}

TEST(TimelineModel, UnknownOrUnrecordedLayerGivesEmptySparkline)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 10);
	EXPECT_EQ(Built(r, 256, "no.such.layer").SparklineCount(), 0u);
	EXPECT_EQ(Built(r, 256, "biome").SparklineCount(), 0u);	// category layer has no float world mean
	TimelineModel m = Built(r, 256, "temp");
	EXPECT_EQ(m.SparklineCount(), 10u);
}

TEST(TimelineModel, PositionFollowsViewMode)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 10);
	HistoryViewState v(*r.rec);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(3, res));
	TimelineModel m;
	m.Build(*r.rec, v, StringCRC("temp"), r.schema.service.get(), &r.cfg);
	EXPECT_EQ(m.Position().mMode, ViewMode::Sample);
	EXPECT_EQ(m.Position().mSample, 3u);
	EXPECT_EQ(m.LiveMarker(), 9u);
	v.GoLive();
	m.Build(*r.rec, v, StringCRC("temp"), r.schema.service.get(), &r.cfg);
	EXPECT_EQ(m.Position().mMode, ViewMode::Live);
	EXPECT_EQ(m.Position().mSample, 9u);
}

TEST(TimelineModel, NoRebuildWhenNothingChanged)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 5);
	HistoryViewState v(*r.rec);
	TimelineModel m;
	EXPECT_TRUE(m.NeedsRebuild(*r.rec, v, StringCRC("temp")));
	m.Build(*r.rec, v, StringCRC("temp"), r.schema.service.get(), &r.cfg);
	EXPECT_FALSE(m.NeedsRebuild(*r.rec, v, StringCRC("temp")));
	v.Update();
	EXPECT_FALSE(m.NeedsRebuild(*r.rec, v, StringCRC("temp")));
}

TEST(TimelineModel, RebuildOnNewSample)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 5);
	HistoryViewState v(*r.rec);
	TimelineModel m;
	m.Build(*r.rec, v, StringCRC("temp"), r.schema.service.get(), &r.cfg);
	TakeRamp(r, 1);
	EXPECT_TRUE(m.NeedsRebuild(*r.rec, v, StringCRC("temp")));
	m.Build(*r.rec, v, StringCRC("temp"), r.schema.service.get(), &r.cfg);
	EXPECT_FALSE(m.NeedsRebuild(*r.rec, v, StringCRC("temp")));
	EXPECT_EQ(m.Range().mCount, 6u);
}

TEST(TimelineModel, RebuildOnViewChange)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 5);
	HistoryViewState v(*r.rec);
	TimelineModel m;
	m.Build(*r.rec, v, StringCRC("temp"), r.schema.service.get(), &r.cfg);
	ViewResult res;
	ASSERT_TRUE(v.ViewSample(2, res));
	EXPECT_TRUE(m.NeedsRebuild(*r.rec, v, StringCRC("temp")));
	m.Build(*r.rec, v, StringCRC("temp"), r.schema.service.get(), &r.cfg);
	EXPECT_FALSE(m.NeedsRebuild(*r.rec, v, StringCRC("temp")));
	v.GoLive();
	EXPECT_TRUE(m.NeedsRebuild(*r.rec, v, StringCRC("temp")));
}

TEST(TimelineModel, RebuildOnBaseLayerChange)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 5);
	HistoryViewState v(*r.rec);
	TimelineModel m;
	m.Build(*r.rec, v, StringCRC("temp"), r.schema.service.get(), &r.cfg);
	EXPECT_TRUE(m.NeedsRebuild(*r.rec, v, StringCRC("biome")));
	m.Build(*r.rec, v, StringCRC("biome"), r.schema.service.get(), &r.cfg);
	EXPECT_FALSE(m.NeedsRebuild(*r.rec, v, StringCRC("biome")));
	EXPECT_TRUE(m.NeedsRebuild(*r.rec, v, StringCRC("temp")));
}

TEST(TimelineModel, BuildAt2080SamplesIsFast)
{
	Rig r; AddLayers(r); r.Start(); TakeRamp(r, 2080);
	for (int i = 0; i < 20; ++i) { if (i % 2 == 0) { EmitEvent(r, "ev.red"); } TakeRamp(r, 1); }
	HistoryViewState v(*r.rec);
	TimelineModel m;
	m.Build(*r.rec, v, StringCRC("temp"), r.schema.service.get(), &r.cfg);
	RecordProperty("BuildMs", std::to_string(m.LastBuildMs()));
	EXPECT_GE(m.LastBuildMs(), 0.0);
	EXPECT_LT(m.LastBuildMs(), 50.0);
	EXPECT_EQ(m.BuildCount(), 1u);
}

TEST(TimelineModel, ToJsonCarriesTheCardFields)
{
	Rig r; AddLayers(r); r.Start();
	EmitEvent(r, "ev.red"); TakeRamp(r, 60);
	TimelineModel m = Built(r, 8);
	const Json::Value j = m.ToJson();
	EXPECT_EQ(j["range"]["count"].asUInt(), 60u);
	EXPECT_EQ(j["range"]["latest"].asUInt(), 59u);
	EXPECT_STREQ(j["position"]["mode"].asCString(), "live");
	EXPECT_EQ(j["liveMarker"].asUInt(), 59u);
	EXPECT_EQ(j["buckets"].size(), 8u);
	EXPECT_EQ(j["buckets"][0]["events"].asUInt(), 1u);
	EXPECT_EQ(j["buckets"][0]["colour"].asUInt(), 0xFF0000FFu);
	EXPECT_EQ(j["bandTicks"].size(), 2u);
	EXPECT_TRUE(j["gaps"].isMember("aggregate"));
	EXPECT_TRUE(j["preWindowDim"].isNull());
	EXPECT_EQ(j["sparkline"].size(), 8u);
}
