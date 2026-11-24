#include <gtest/gtest.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/Testing/BudgetSizing.h>
#include <DiaCellHistory/Testing/FakeClock.h>
#include <DiaCellHistory/Testing/StubGrid.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using Dia::CellLayers::CellLayerRegistry;
using Dia::CellLayers::Testing::LogCapture;
using Dia::Core::StringCRC;

namespace
{
	// Schema entry text for a layer under `for: cells`. historyBlock may be null.
	std::string FloatEntry(const char* key, double mn, double mx, const char* historyBlock = nullptr)
	{
		std::string s = std::string("{\"key\":\"") + key + "\",\"kind\":\"parameter\",\"label\":\"L\",\"description\":\"D\",\"type\":\"float\",\"min\":"
			+ std::to_string(mn) + ",\"max\":" + std::to_string(mx);
		if (historyBlock) { s += std::string(",\"history\":") + historyBlock; }
		return s + "}";
	}
	std::string EnumEntry(const char* key, int count, const char* historyBlock = nullptr)
	{
		std::string s = std::string("{\"key\":\"") + key + "\",\"kind\":\"parameter\",\"label\":\"L\",\"description\":\"D\",\"type\":\"enum\",\"values\":[";
		for (int i = 0; i < count; ++i)
		{
			if (i) { s += ","; }
			s += std::string("{\"id\":\"") + char('a' + i) + "\",\"label\":\"V\"}";
		}
		s += "]";
		if (historyBlock) { s += std::string(",\"history\":") + historyBlock; }
		return s + "}";
	}

	struct SchemaSet
	{
		Dia::ValueSchema::ValueSchemaRegistry registry;
		std::unique_ptr<Dia::ValueSchema::ValueSchemaService> service;

		bool Load(const std::vector<std::string>& entries)
		{
			std::string doc = "{\"format\":\"dia.value_schema\",\"version\":1,\"for\":\"cells\",\"entries\":[";
			for (size_t i = 0; i < entries.size(); ++i) { if (i) { doc += ","; } doc += entries[i]; }
			doc += "]}";
			Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(doc.c_str(), "rec.json");
			if (!r.mOk) { ADD_FAILURE() << r.mFailure.mReason.AsCStr(); return false; }
			for (uint32_t i = 0; i < r.mEntries.Size(); ++i) { registry.Register(r.mEntries[i]); }
			registry.Publish();
			service = std::make_unique<Dia::ValueSchema::ValueSchemaService>(registry);
			return true;
		}
	};

	// Registers every stub layer with schemaKey "cells/<id>" so schema lookups resolve.
	void RegisterWithKeys(StubGrid& g, CellLayerRegistry& reg, uint32_t from = 0)
	{
		for (uint32_t i = from; i < g.LayerCount(); ++i)
		{
			Dia::CellLayers::CellLayerDesc d;
			g.Describe(i, d);
			d.schemaKey = StringCRC((std::string("cells/") + d.id.AsChar()).c_str());
			ASSERT_EQ(reg.Register(d), Dia::CellLayers::CellRegisterResult::Ok);
		}
	}

	struct Rig
	{
		StubGrid grid{ 4, 3 };	// 12 cells
		SchemaSet schema;
		CellLayerRegistry registry;
		HistoryConfig cfg;
		std::unique_ptr<CellHistoryRecorder> rec;
		FakeClock clock;

		void Start(bool withSchema = true, const RecorderBudgets* budgets = nullptr)
		{
			RegisterWithKeys(grid, registry);
			rec = std::make_unique<CellHistoryRecorder>(registry, withSchema ? schema.service.get() : nullptr, cfg, 1234, budgets);
		}
		SampleResult Take() { FakeClockSample s = clock.Next(); SampleInfo i; i.tick = s.tick; i.label = s.label; i.band = s.band; return rec->Sample(i); }
	};

	const StringCRC kWorld("world");
}

TEST(CellHistoryRecorder, SampleIndicesAreMonotonicAndHeadersRoundTrip)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 1.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100) }));
	r.Start();
	for (uint32_t i = 0; i < 3; ++i)
	{
		const SampleResult res = r.Take();
		EXPECT_EQ(res.mSample, i);
		EXPECT_EQ(res.mLayersRecorded, 1u);
	}
	EXPECT_EQ(r.rec->SampleCount(), 3u);
	FakeClock expect;
	for (uint32_t i = 0; i < 3; ++i)
	{
		const FakeClockSample e = expect.Next();
		ASSERT_TRUE(r.rec->HasHeader(i));
		EXPECT_EQ(r.rec->Header(i).mTick, e.tick);
		EXPECT_STREQ(r.rec->Header(i).mLabel.AsCStr(), e.label);
		EXPECT_EQ(r.rec->Header(i).mBand, e.band);
	}
	EXPECT_FALSE(r.rec->HasHeader(3));
	EXPECT_STREQ(r.rec->Header(3).mLabel.AsCStr(), "");
}

TEST(CellHistoryRecorder, AggregatesMatchDirectComputationNaNExcluded)
{
	Rig r;
	r.grid.AddFloatLayer("temp");
	r.grid.AddCategoryLayer("biome");
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 3) }));
	float sum = 0; uint32_t n = 0;
	for (uint32_t c = 0; c < 12; ++c)
	{
		const float v = (c == 3) ? std::nanf("") : float(c) * 2.0f;
		r.grid.Set("temp", c, v);
		if (!std::isnan(v)) { sum += v; ++n; }
		r.grid.Set("biome", c, float(c % 3));
	}
	r.Start();
	r.Take();
	AggregateStats st;
	ASSERT_TRUE(r.rec->Aggregate(0, StringCRC("temp"), kWorld, st));
	EXPECT_FLOAT_EQ(st.mMin, 0.0f);
	EXPECT_FLOAT_EQ(st.mMax, 22.0f);
	EXPECT_FLOAT_EQ(st.mMean, sum / float(n));
	uint16_t counts[3] = {};
	ASSERT_TRUE(r.rec->CategoryCounts(0, StringCRC("biome"), kWorld, counts, 3));
	EXPECT_EQ(counts[0], 4);
	EXPECT_EQ(counts[1], 4);
	EXPECT_EQ(counts[2], 4);
}

TEST(CellHistoryRecorder, GroupMembershipFollowsTheGroupingLayerAcrossSamples)
{
	Rig r;
	r.grid.AddFloatLayer("temp");
	r.grid.AddCategoryLayer("biome");
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100), EnumEntry("biome", 2, "{\"group\":true}") }));
	for (uint32_t c = 0; c < 12; ++c) { r.grid.Set("temp", c, float(c)); r.grid.Set("biome", c, c < 6 ? 0.0f : 1.0f); }
	r.Start();
	r.Take();
	EXPECT_EQ(r.rec->GroupCount(), 3u);	// world, biome=a, biome=b
	AggregateStats st;
	ASSERT_TRUE(r.rec->Aggregate(0, StringCRC("temp"), StringCRC("biome=a"), st));
	EXPECT_FLOAT_EQ(st.mMean, 2.5f);
	ASSERT_TRUE(r.rec->Aggregate(0, StringCRC("temp"), StringCRC("biome=b"), st));
	EXPECT_FLOAT_EQ(st.mMean, 8.5f);

	for (uint32_t c = 0; c < 3; ++c) { r.grid.Set("biome", c, 1.0f); }
	r.grid.BumpVersion("biome");
	r.Take();
	ASSERT_TRUE(r.rec->Aggregate(1, StringCRC("temp"), StringCRC("biome=a"), st));
	EXPECT_FLOAT_EQ(st.mMean, 4.0f);	// cells 3..5
	ASSERT_TRUE(r.rec->Aggregate(1, StringCRC("temp"), StringCRC("biome=b"), st));
	EXPECT_FLOAT_EQ(st.mMean, (0 + 1 + 2 + 6 + 7 + 8 + 9 + 10 + 11) / 9.0f);
	EXPECT_EQ(r.grid.ReadCount("temp"), 1u);	// membership changed but temp itself did not: not re-read
	uint16_t counts[2] = {};
	ASSERT_TRUE(r.rec->CategoryCounts(1, StringCRC("biome"), StringCRC("biome=a"), counts, 2));
	EXPECT_EQ(counts[0], 3);
	EXPECT_EQ(counts[1], 0);
}

TEST(CellHistoryRecorder, LayersBeyondTheCapAreSkippedWithOneWarning)
{
	LogCapture log("HIS");
	Rig r;
	for (int i = 0; i < 66; ++i) { r.grid.AddFloatLayer(("l" + std::to_string(i)).c_str(), float(i)); }
	r.Start(false);
	r.Take();
	r.Take();
	EXPECT_EQ(r.rec->RecordedLayerCount(), 64u);
	EXPECT_EQ(r.rec->SkippedLayerCount(), 2u);
	EXPECT_EQ(log.Warnings("aggregate layers"), 1u);
	AggregateStats st;
	EXPECT_TRUE(r.rec->Aggregate(1, StringCRC("l63"), kWorld, st));
	EXPECT_FALSE(r.rec->Aggregate(1, StringCRC("l64"), kWorld, st));
	EXPECT_FALSE(r.rec->Aggregate(1, StringCRC("l65"), kWorld, st));
}

TEST(CellHistoryRecorder, ConfiguredLowerCapApplies)
{
	Rig r;
	for (int i = 0; i < 4; ++i) { r.grid.AddFloatLayer(("l" + std::to_string(i)).c_str()); }
	r.cfg.mMaxAggregateLayers = 2;
	r.Start(false);
	r.Take();
	EXPECT_EQ(r.rec->RecordedLayerCount(), 2u);
	EXPECT_EQ(r.rec->SkippedLayerCount(), 2u);
}

TEST(CellHistoryRecorder, AggregateFalseLayerIsNotRecorded)
{
	Rig r;
	r.grid.AddFloatLayer("kept", 1.0f);
	r.grid.AddFloatLayer("hidden", 2.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("kept", 0, 10), FloatEntry("hidden", 0, 10, "{\"aggregate\":false}") }));
	r.Start();
	r.Take();
	AggregateStats st;
	EXPECT_TRUE(r.rec->Aggregate(0, StringCRC("kept"), kWorld, st));
	EXPECT_FALSE(r.rec->Aggregate(0, StringCRC("hidden"), kWorld, st));
	EXPECT_EQ(r.rec->RecordedLayerCount(), 1u);
	EXPECT_EQ(r.grid.ReadCount("hidden"), 0u);
}

TEST(CellHistoryRecorder, UnchangedVersionIsNotReadAgainAndRowIsReused)
{
	Rig r;
	r.grid.AddFloatLayer("a", 1.0f);
	r.grid.AddFloatLayer("b", 2.0f);
	r.Start(false);
	r.Take();
	EXPECT_EQ(r.grid.ReadCount("a"), 1u);
	EXPECT_EQ(r.grid.ReadCount("b"), 1u);
	EXPECT_EQ(r.rec->VersionGatedSkips(), 0u);

	r.Take();
	EXPECT_EQ(r.grid.ReadCount("a"), 1u);
	EXPECT_EQ(r.grid.ReadCount("b"), 1u);
	EXPECT_EQ(r.rec->VersionGatedSkips(), 2u);
	AggregateStats s0, s1;
	ASSERT_TRUE(r.rec->Aggregate(0, StringCRC("a"), kWorld, s0));
	ASSERT_TRUE(r.rec->Aggregate(1, StringCRC("a"), kWorld, s1));
	EXPECT_FLOAT_EQ(s1.mMin, s0.mMin);
	EXPECT_FLOAT_EQ(s1.mMean, s0.mMean);
	EXPECT_FLOAT_EQ(s1.mMax, s0.mMax);

	r.grid.SetAll("a", 5.0f);
	r.grid.BumpVersion("a");
	r.Take();
	EXPECT_EQ(r.grid.ReadCount("a"), 2u);
	EXPECT_EQ(r.grid.ReadCount("b"), 1u);
	EXPECT_EQ(r.rec->VersionGatedSkips(), 3u);
	ASSERT_TRUE(r.rec->Aggregate(2, StringCRC("a"), kWorld, s1));
	EXPECT_FLOAT_EQ(s1.mMean, 5.0f);
	ASSERT_TRUE(r.rec->Aggregate(2, StringCRC("b"), kWorld, s1));
	EXPECT_FLOAT_EQ(s1.mMean, 2.0f);
}

TEST(CellHistoryRecorder, TrackedLayerWithUnchangedVersionStillGetsItsCellSlot)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 40.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100, "{\"track\":true}") }));
	r.Start();
	r.Take();
	r.Take();
	EXPECT_EQ(r.grid.ReadCount("temp"), 1u);
	std::vector<float> v(12);
	ASSERT_TRUE(r.rec->CellValues(1, StringCRC("temp"), v.data(), 12));
	EXPECT_NEAR(v[5], 40.0f, 100.0f / 65535.0f);
}

TEST(CellHistoryRecorder, RecordsLiveValuesNeverTheScrubbedView)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 7.0f);
	r.Start(false);
	Dia::CellLayers::Testing::StubViewValueSource src;
	src.live = false;
	src.values.assign(12, 999.0f);
	ASSERT_TRUE(r.registry.SetViewValueSource(&src));
	r.Take();
	AggregateStats st;
	ASSERT_TRUE(r.rec->Aggregate(0, StringCRC("temp"), kWorld, st));
	EXPECT_FLOAT_EQ(st.mMax, 7.0f);
	EXPECT_EQ(src.getValuesCalls, 0u);
	EXPECT_EQ(r.grid.ReadCount("temp"), 1u);
}

TEST(CellHistoryRecorder, SampleWithNoRecordedLayersWarnsAndKeepsOnlyTheHeader)
{
	LogCapture log("HIS");
	Rig r;
	r.Start(false);	// nothing registered
	const SampleResult a = r.Take();
	const SampleResult b = r.Take();
	EXPECT_EQ(a.mSample, 0u);
	EXPECT_EQ(b.mSample, 1u);
	EXPECT_EQ(a.mLayersRecorded, 0u);
	EXPECT_EQ(log.Warnings("no recorded layers"), 1u);
	EXPECT_TRUE(r.rec->HasHeader(1));
	AggregateStats st;
	float v[12];
	EXPECT_FALSE(r.rec->Aggregate(0, StringCRC("x"), kWorld, st));
	EXPECT_FALSE(r.rec->CellValues(0, StringCRC("x"), v, 12));
}

TEST(CellHistoryRecorder, EventsAttachToTheNextSampleOnly)
{
	Rig r;
	r.grid.AddFloatLayer("temp");
	r.Start(false);
	HistoryEvent e;
	e.type = StringCRC("test.simple");
	ASSERT_EQ(r.rec->Emit(e), EmitResult::Ok);
	r.Take();	// sample 0 takes the pending event
	ASSERT_EQ(r.rec->Emit(e), EmitResult::Ok);
	EXPECT_EQ(r.rec->Events(0, 5).Count(), 1u);	// the second is still pending
	r.Take();
	EXPECT_EQ(r.rec->Events(0, 0).Count(), 1u);
	EXPECT_EQ(r.rec->Events(1, 1).Count(), 1u);
	EXPECT_EQ(r.rec->Events(0, 1).Count(), 2u);
}

TEST(CellHistoryRecorder, EvictedSamplesReadAsFalse)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 50.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100, "{\"track\":true}") }));
	RecorderBudgets b;
	b.mAggregateBytes = BudgetBytesForWindow(5, AggregateRowBytes(1, 0, 1));
	b.mCellBytes = BudgetBytesForWindow(3, BytesPerSample(1, 0, 12));
	b.mEventBytes = 4096;
	r.Start(true, &b);
	for (int i = 0; i < 8; ++i) { r.Take(); }
	AggregateStats st;
	for (uint32_t s = 0; s < 3; ++s) { EXPECT_FALSE(r.rec->Aggregate(s, StringCRC("temp"), kWorld, st)) << s; EXPECT_FALSE(r.rec->HasHeader(s)) << s; }
	for (uint32_t s = 3; s < 8; ++s) { EXPECT_TRUE(r.rec->Aggregate(s, StringCRC("temp"), kWorld, st)) << s; EXPECT_TRUE(r.rec->HasHeader(s)) << s; }
	EXPECT_EQ(r.rec->SampleCount(), 8u);
	EXPECT_EQ(r.rec->FirstRetained(), 5u);
	float v[12];
	EXPECT_FALSE(r.rec->CellValues(4, StringCRC("temp"), v, 12));
	EXPECT_TRUE(r.rec->CellValues(5, StringCRC("temp"), v, 12));
	EXPECT_FALSE(r.rec->CellValues(5, StringCRC("unknown"), v, 12));
	uint16_t c[1];
	EXPECT_FALSE(r.rec->CategoryCounts(7, StringCRC("temp"), kWorld, c, 1));	// float layer has no counts
}

TEST(CellHistoryRecorder, CellValuesAreReadableWithinTheQuantisationBound)
{
	Rig r;
	r.grid.AddFloatLayer("temp");
	r.grid.AddCategoryLayer("biome");
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100, "{\"track\":true}"), EnumEntry("biome", 4) }));
	r.cfg.mTrackCount = 1;
	r.cfg.mTrack[0] = Dia::Core::Containers::String64("%s", "biome");
	for (uint32_t c = 0; c < 12; ++c) { r.grid.Set("temp", c, 3.3f * float(c)); r.grid.Set("biome", c, float(c % 4)); }
	r.Start();
	r.Take();
	float t[12], b[12];
	ASSERT_TRUE(r.rec->CellValues(0, StringCRC("temp"), t, 12));
	ASSERT_TRUE(r.rec->CellValues(0, StringCRC("biome"), b, 12));
	for (uint32_t c = 0; c < 12; ++c)
	{
		EXPECT_NEAR(t[c], 3.3f * float(c), 100.0f / 65535.0f) << c;
		EXPECT_FLOAT_EQ(b[c], float(c % 4)) << c;
	}
}

TEST(CellHistoryRecorder, FloatLayerWithoutSchemaRangeCannotBeTracked)
{
	LogCapture log("HIS");
	Rig r;
	r.grid.AddFloatLayer("temp", 3.0f);
	r.Start(false);
	r.Take();
	EXPECT_FALSE(r.rec->TrackLayer(StringCRC("temp")));
	EXPECT_GE(log.Count(Dia::Observation::Log::LogLevel::kError, "range"), 1u);
	AggregateStats st;
	EXPECT_TRUE(r.rec->Aggregate(0, StringCRC("temp"), kWorld, st));	// aggregates still work
}

TEST(CellHistoryRecorder, LayerRegisteredAfterTheFirstSampleIsSkippedWithAWarning)
{
	LogCapture log("HIS");
	Rig r;
	r.grid.AddFloatLayer("early", 1.0f);
	r.Start(false);
	r.Take();
	r.grid.AddFloatLayer("late", 2.0f);
	RegisterWithKeys(r.grid, r.registry, 1);
	r.Take();
	r.Take();
	EXPECT_EQ(r.rec->SkippedLayerCount(), 1u);
	EXPECT_EQ(log.Warnings("after the first sample"), 1u);
	AggregateStats st;
	EXPECT_FALSE(r.rec->Aggregate(2, StringCRC("late"), kWorld, st));
	EXPECT_TRUE(r.rec->Aggregate(2, StringCRC("early"), kWorld, st));
}

TEST(CellHistoryRecorder, RunIdIsDeterministicPerSeed)
{
	StubGrid g;
	CellLayerRegistry reg;
	HistoryConfig cfg;
	CellHistoryRecorder a(reg, nullptr, cfg, 42), b(reg, nullptr, cfg, 42), c(reg, nullptr, cfg, 43);
	EXPECT_EQ(a.RunId(), b.RunId());
	EXPECT_NE(a.RunId(), c.RunId());
}

TEST(CellHistoryRecorder, TruncateAfterDropsLaterSamplesAndResumesNumbering)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 1.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100, "{\"track\":true}") }));
	r.Start();
	HistoryEvent e;
	e.type = StringCRC("test.simple");
	for (int i = 0; i < 5; ++i) { r.rec->Emit(e); r.Take(); }
	r.rec->TruncateAfter(2);
	EXPECT_EQ(r.rec->SampleCount(), 3u);
	AggregateStats st;
	float v[12];
	EXPECT_TRUE(r.rec->Aggregate(2, StringCRC("temp"), kWorld, st));
	EXPECT_FALSE(r.rec->Aggregate(3, StringCRC("temp"), kWorld, st));
	EXPECT_FALSE(r.rec->CellValues(3, StringCRC("temp"), v, 12));
	EXPECT_FALSE(r.rec->HasHeader(3));
	EXPECT_TRUE(r.rec->HasHeader(2));
	EXPECT_EQ(r.rec->Events(3, 4).Count(), 0u);
	EXPECT_EQ(r.rec->Events(0, 2).Count(), 3u);

	r.grid.SetAll("temp", 9.0f);	// no version bump: the gate memory must have been reset by the rewind
	const SampleResult next = r.Take();
	EXPECT_EQ(next.mSample, 3u);
	ASSERT_TRUE(r.rec->Aggregate(3, StringCRC("temp"), kWorld, st));
	EXPECT_FLOAT_EQ(st.mMean, 9.0f);
	EXPECT_EQ(r.grid.ReadCount("temp"), 2u);
}

TEST(CellHistoryRecorder, TruncateAfterDropsAnEventEmittedButNotYetAttached)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 1.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100) }));
	r.Start();
	HistoryEvent e;
	e.type = StringCRC("test.simple");
	for (int i = 0; i < 3; ++i) { r.rec->Emit(e); r.Take(); }
	ASSERT_EQ(r.rec->Emit(e), EmitResult::Ok);		// pending: belongs to sample 3
	r.rec->TruncateAfter(2);
	r.Take();										// sample 3
	EXPECT_EQ(r.rec->Events(3, 3).Count(), 0u);
	EXPECT_EQ(r.rec->Events(0, 9).Count(), 3u);
}

TEST(CellHistoryRecorder, ResetHistoryClearsEveryTierAndRestartsNumberingWithANewRunId)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 1.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100, "{\"track\":true}") }));
	r.Start();
	HistoryEvent e;
	e.type = StringCRC("test.simple");
	for (int i = 0; i < 5; ++i) { r.rec->Emit(e); r.Take(); }
	r.rec->Emit(e);		// pending
	const uint64_t oldId = r.rec->RunId();
	r.rec->ResetHistory(12345);
	EXPECT_NE(r.rec->RunId(), oldId);
	EXPECT_EQ(r.rec->RunId(), r.rec->RunId());
	EXPECT_EQ(r.rec->SampleCount(), 0u);
	EXPECT_FALSE(r.rec->HasHeader(0));
	AggregateStats st;
	float v[12];
	EXPECT_FALSE(r.rec->Aggregate(0, StringCRC("temp"), kWorld, st));
	EXPECT_FALSE(r.rec->CellValues(0, StringCRC("temp"), v, 12));
	EXPECT_EQ(r.rec->Events(0, 99).Count(), 0u);
	EXPECT_TRUE(r.rec->IsTracked(StringCRC("temp")));		// layout and tracking are kept
	EXPECT_EQ(r.rec->RecordedLayerCount(), 1u);

	r.grid.SetAll("temp", 9.0f);	// no version bump: the gate memory must be reset
	const SampleResult next = r.Take();
	EXPECT_EQ(next.mSample, 0u);
	EXPECT_EQ(next.mLayersRecorded, 1u);
	ASSERT_TRUE(r.rec->Aggregate(0, StringCRC("temp"), kWorld, st));
	EXPECT_FLOAT_EQ(st.mMean, 9.0f);
	ASSERT_TRUE(r.rec->CellValues(0, StringCRC("temp"), v, 12));
	EXPECT_NEAR(v[0], 9.0f, 0.01f);
	EXPECT_EQ(r.rec->Events(0, 0).Count(), 0u);		// the pending event did not survive
	EXPECT_EQ(r.rec->FirstRetained(), 0u);
}

TEST(CellHistoryRecorder, ResetHistoryIsDeterministicPerSeedAndNeverKeepsTheOldId)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 1.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100) }));
	r.Start();		// recorder seed 99
	r.rec->ResetHistory(5);
	const uint64_t a = r.rec->RunId();
	r.rec->ResetHistory(5);
	EXPECT_NE(r.rec->RunId(), a);		// the same seed twice in a row still changes the id
	const uint64_t b = r.rec->RunId();
	r.rec->ResetHistory(6);
	EXPECT_NE(r.rec->RunId(), b);
}

TEST(CellHistoryRecorder, ActiveGridReportsTheRecordedGridBeforeAndAfterTheFirstSample)
{
	Rig r;
	r.grid.AddFloatLayer("temp", 1.0f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("temp", 0, 100) }));
	r.Start();
	const Dia::Graphics::CellGridDesc* g = r.rec->ActiveGrid();
	ASSERT_NE(g, nullptr);
	EXPECT_EQ(g->width, 4);
	EXPECT_EQ(g->height, 3);
	r.Take();
	EXPECT_EQ(r.rec->ActiveGrid(), g);
}


namespace
{
	const char* WorldName(void* user) { return static_cast<const char*>(user); }
}

TEST(CellHistoryRecorderWorldId, EmptyWithoutAProviderAndFollowsTheInstalledOne)
{
	CellLayerRegistry registry;
	HistoryConfig cfg;
	CellHistoryRecorder rec(registry, nullptr, cfg, 1);
	EXPECT_STREQ(rec.WorldId(), "");
	rec.SetWorldIdProvider(&WorldName, const_cast<char*>("coast_range"));
	EXPECT_STREQ(rec.WorldId(), "coast_range");
	rec.SetWorldIdProvider(nullptr, nullptr);
	EXPECT_STREQ(rec.WorldId(), "");
	rec.SetWorldIdProvider([](void*) -> const char* { return nullptr; }, nullptr);
	EXPECT_STREQ(rec.WorldId(), "");
}
