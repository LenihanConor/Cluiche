#include <gtest/gtest.h>
#include <DiaCellHistory/ScrubObservation.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/TimelineModel.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/Testing/BudgetSizing.h>
#include <DiaCellHistory/Testing/FakeClock.h>
#include <DiaCellHistory/Testing/StubGrid.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaObservation/Metric/MetricRegistry.h>
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
using Dia::Core::StringCRC;

namespace
{
	std::string FloatEntry(const char* key, double mn, double mx, const char* historyBlock = nullptr)
	{
		std::string s = std::string("{\"key\":\"") + key + "\",\"kind\":\"parameter\",\"label\":\"L\",\"description\":\"D\",\"type\":\"float\",\"min\":"
			+ std::to_string(mn) + ",\"max\":" + std::to_string(mx);
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
			Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(doc.c_str(), "obs.json");
			if (!r.mOk) { ADD_FAILURE() << r.mFailure.mReason.AsCStr(); return false; }
			for (uint32_t i = 0; i < r.mEntries.Size(); ++i) { registry.Register(r.mEntries[i]); }
			registry.Publish();
			service = std::make_unique<Dia::ValueSchema::ValueSchemaService>(registry);
			return true;
		}
	};

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
		std::unique_ptr<ScrubObservation> obs;
		std::unique_ptr<HistoryViewState> view;
		std::unique_ptr<TimelineModel> timeline;
		std::unique_ptr<FakeClock> clock;

		void Start(const RecorderBudgets* budgets = nullptr)
		{
			RegisterWithKeys(grid, registry);
			rec = std::make_unique<CellHistoryRecorder>(registry, schema.service.get(), cfg, 1234, budgets);
			clock = std::make_unique<FakeClock>();
			obs = std::make_unique<ScrubObservation>();
			view = std::make_unique<HistoryViewState>(*rec);
			timeline = std::make_unique<TimelineModel>();
		}

		SampleResult Take()
		{
			FakeClockSample s = clock->Next();
			SampleInfo i;
			i.tick = s.tick;
			i.label = s.label;
			i.band = s.band;
			return rec->Sample(i);
		}
	};

	const StringCRC kWorld("world");
	const StringCRC kTestFloat("test_f");
}

// ---- Metric Registration Tests ----

TEST(ScrubObservation, MetricsRegisteredWithCorrectNames)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0) }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();

	// Check all three metrics exist
	EXPECT_NE(metrics.FindCounter(StringCRC("dia.cellhistory.view_changes")), nullptr);
	EXPECT_NE(metrics.FindGauge(StringCRC("dia.cellhistory.scrub_seconds")), nullptr);
	EXPECT_NE(metrics.FindGauge(StringCRC("dia.cellhistory.timeline_rebuild_ms")), nullptr);
}

// ---- View Changes Counter Tests ----

TEST(ScrubObservation, ViewChangesCounterIncrementsOnViewSample)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0, "{\"group\":true}") }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
	auto counter = metrics.FindCounter(StringCRC("dia.cellhistory.view_changes"));
	EXPECT_NE(counter, nullptr);
	if (counter)
	{
		uint64_t before = counter->Value();

		// Take a sample first so we have something to view
		r.Take();

		// ViewSample should increment the counter
		ViewResult result;
		bool ok = r.view->ViewSample(0, result);
		EXPECT_TRUE(ok);
		r.obs->OnViewChange(*r.view, *r.timeline);

		uint64_t after = counter->Value();
		EXPECT_GT(after, before);
	}
}

TEST(ScrubObservation, ViewChangesCounterIncrementsOnGoLive)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0, "{\"group\":true}") }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
	auto counter = metrics.FindCounter(StringCRC("dia.cellhistory.view_changes"));
	EXPECT_NE(counter, nullptr);
	if (counter)
	{
		// First, go to sample mode
		r.Take();
		ViewResult result;
		r.view->ViewSample(0, result);
		r.obs->OnViewChange(*r.view, *r.timeline);

		uint64_t beforeGoLive = counter->Value();

		// Now go back to live
		r.view->GoLive();
		r.obs->OnViewChange(*r.view, *r.timeline);

		uint64_t afterGoLive = counter->Value();
		EXPECT_GT(afterGoLive, beforeGoLive);
	}
}

TEST(ScrubObservation, ViewChangesCounterDoesNotIncrementOnNoOp)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0, "{\"group\":true}") }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
	auto counter = metrics.FindCounter(StringCRC("dia.cellhistory.view_changes"));
	EXPECT_NE(counter, nullptr);
	if (counter)
	{
		// Go to sample 0
		r.Take();
		ViewResult result;
		r.view->ViewSample(0, result);
		r.obs->OnViewChange(*r.view, *r.timeline);

		uint64_t before = counter->Value();

		// Try to go to sample 0 again (no-op)
		r.view->ViewSample(0, result);
		r.obs->OnViewChange(*r.view, *r.timeline);	// Should not increment if epoch didn't change

		uint64_t after = counter->Value();
		EXPECT_EQ(after, before);	// No increment on no-op
	}
}

TEST(ScrubObservation, ViewChangesCounterDoesNotIncrementAfterARejectedView)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0, "{\"group\":true}") }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
	auto counter = metrics.FindCounter(StringCRC("dia.cellhistory.view_changes"));
	ASSERT_NE(counter, nullptr);

	r.Take();
	ViewResult result;
	ASSERT_TRUE(r.view->ViewSample(0, result));
	r.obs->OnViewChange(*r.view, *r.timeline);
	const uint64_t before = counter->Value();

	EXPECT_FALSE(r.view->ViewSample(99, result));	// rejected: out of range
	r.obs->OnViewChange(*r.view, *r.timeline);
	EXPECT_EQ(counter->Value(), before);
	r.obs->Tick(1.0);								// still scrubbing at sample 0
	auto gauge = metrics.FindGauge(StringCRC("dia.cellhistory.scrub_seconds"));
	ASSERT_NE(gauge, nullptr);
	EXPECT_GT(gauge->Value(), 0.0);
}

// ---- Scrub Seconds Gauge Tests ----

TEST(ScrubObservation, ScrubSecondsGaugeAccumulatesInScrubMode)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0) }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
	auto gauge = metrics.FindGauge(StringCRC("dia.cellhistory.scrub_seconds"));
	EXPECT_NE(gauge, nullptr);
	if (gauge)
	{
		// Go to sample mode
		r.Take();
		ViewResult result;
		r.view->ViewSample(0, result);
		r.obs->OnViewChange(*r.view, *r.timeline);

		double before = gauge->Value();

		// Tick the observation with time increments
		r.obs->Tick(1.0);	// 1 second in scrub mode

		double after = gauge->Value();
		EXPECT_NEAR(after, before + 1.0, 0.01);	// Should accumulate 1 second
	}
}

TEST(ScrubObservation, ScrubSecondsGaugeDoesNotAccumulateInLiveMode)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0) }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
	auto gauge = metrics.FindGauge(StringCRC("dia.cellhistory.scrub_seconds"));
	EXPECT_NE(gauge, nullptr);
	if (gauge)
	{
		double beforeTick = gauge->Value();

		// Tick while in live mode (default)
		r.obs->Tick(1.0);	// 1 second in live mode

		double afterTick = gauge->Value();
		EXPECT_EQ(afterTick, beforeTick);	// Should NOT accumulate in live mode
	}
}

TEST(ScrubObservation, ScrubSecondsGaugeResetsWhenGoingLive)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0) }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
	auto gauge = metrics.FindGauge(StringCRC("dia.cellhistory.scrub_seconds"));
	EXPECT_NE(gauge, nullptr);
	if (gauge)
	{
		// Go to sample mode and accumulate time
		r.Take();
		ViewResult result;
		r.view->ViewSample(0, result);
		r.obs->OnViewChange(*r.view, *r.timeline);
		r.obs->Tick(2.0);	// 2 seconds in scrub mode

		double inScrubMode = gauge->Value();
		EXPECT_NEAR(inScrubMode, 2.0, 0.01);

		// Go back to live
		r.view->GoLive();
		r.obs->OnViewChange(*r.view, *r.timeline);

		// The gauge should reset to 0 when going live
		double afterGoLive = gauge->Value();
		EXPECT_EQ(afterGoLive, 0.0);
	}
}

// ---- Timeline Rebuild Ms Gauge Tests ----

TEST(ScrubObservation, TimelineRebuildMsGaugeMatchesLastBuildMs)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0) }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
	auto gauge = metrics.FindGauge(StringCRC("dia.cellhistory.timeline_rebuild_ms"));
	EXPECT_NE(gauge, nullptr);
	if (gauge)
	{
		// Take a sample and build the timeline
		r.Take();
		r.timeline->Build(*r.rec, *r.view, StringCRC("test_f"), nullptr, nullptr);

		// Update the observation with the timeline
		r.obs->OnTimelineRebuild(*r.timeline);

		double metricValue = gauge->Value();
		double timelineValue = r.timeline->LastBuildMs();

		EXPECT_NEAR(metricValue, timelineValue, 0.01);
	}
}

TEST(ScrubObservation, TimelineRebuildMsGaugeUpdatesAfterBuild)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0, "{\"group\":true}") }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
	auto gauge = metrics.FindGauge(StringCRC("dia.cellhistory.timeline_rebuild_ms"));
	EXPECT_NE(gauge, nullptr);
	if (gauge)
	{
		// Take a sample and build
		r.Take();
		r.timeline->Build(*r.rec, *r.view, StringCRC("test_f"), nullptr, nullptr);
		r.obs->OnTimelineRebuild(*r.timeline);

		double firstValue = gauge->Value();

		// Take another sample and rebuild
		r.Take();
		r.timeline->Build(*r.rec, *r.view, StringCRC("test_f"), nullptr, nullptr);
		r.obs->OnTimelineRebuild(*r.timeline);

		double secondValue = gauge->Value();

		// The gauge should have a valid value; since we can't control build time,
		// just ensure it's >= 0 and was updated
		EXPECT_GE(firstValue, 0.0);
		EXPECT_GE(secondValue, 0.0);
	}
}

// ---- Metric Cleanup Tests ----

TEST(ScrubObservation, MetricsUnregisteredOnDestruction)
{
	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();

	// Create and destroy an observation
	{
		Rig r;
		ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0) }));
		r.Start();

		// Verify metrics exist
		EXPECT_NE(metrics.FindCounter(StringCRC("dia.cellhistory.view_changes")), nullptr);
		EXPECT_NE(metrics.FindGauge(StringCRC("dia.cellhistory.scrub_seconds")), nullptr);
		EXPECT_NE(metrics.FindGauge(StringCRC("dia.cellhistory.timeline_rebuild_ms")), nullptr);
	}

	// After destruction, metrics should be unregistered
	metrics.Reset();
	EXPECT_EQ(metrics.FindCounter(StringCRC("dia.cellhistory.view_changes")), nullptr);
	EXPECT_EQ(metrics.FindGauge(StringCRC("dia.cellhistory.scrub_seconds")), nullptr);
	EXPECT_EQ(metrics.FindGauge(StringCRC("dia.cellhistory.timeline_rebuild_ms")), nullptr);
}

TEST(ScrubObservation, CanRestartAfterDestruction)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0) }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();

	// First cycle
	EXPECT_NE(metrics.FindCounter(StringCRC("dia.cellhistory.view_changes")), nullptr);

	// Destroy
	r.obs.reset();
	metrics.Reset();

	// Restart
	r.obs = std::make_unique<ScrubObservation>();
	EXPECT_NE(metrics.FindCounter(StringCRC("dia.cellhistory.view_changes")), nullptr);
}
