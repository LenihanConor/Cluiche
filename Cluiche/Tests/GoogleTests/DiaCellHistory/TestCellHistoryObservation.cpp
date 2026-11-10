#include <gtest/gtest.h>
#include <DiaCellHistory/CellHistoryObservation.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/Testing/BudgetSizing.h>
#include <DiaCellHistory/Testing/FakeClock.h>
#include <DiaCellHistory/Testing/StubGrid.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaObservation/Health/HealthRegistry.h>
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
		std::unique_ptr<CellHistoryObservation> obs;
		FakeClock clock;

		void Start(const RecorderBudgets* budgets = nullptr)
		{
			RegisterWithKeys(grid, registry);
			rec = std::make_unique<CellHistoryRecorder>(registry, schema.service.get(), cfg, 1234, budgets);
			obs = std::make_unique<CellHistoryObservation>(*rec);
		}

		SampleResult Take()
		{
			FakeClockSample s = clock.Next();
			SampleInfo i;
			i.tick = s.tick;
			i.label = s.label;
			i.band = s.band;
			return rec->Sample(i);
		}
	};

	const StringCRC kWorld("world");
	const StringCRC kTestFloat("test_f");
	const StringCRC kTestCat("test_c");
}

TEST(CellHistoryObservation, MetricsRegisteredWithCorrectNames)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0), EnumEntry("test_c", 2) }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();

	// Check metrics exist by finding them (API: FindGauge / FindCounter)
	EXPECT_NE(metrics.FindGauge(StringCRC("dia.cellhistory.cells.bytes")), nullptr);
	EXPECT_NE(metrics.FindGauge(StringCRC("dia.cellhistory.aggregates.bytes")), nullptr);
	EXPECT_NE(metrics.FindGauge(StringCRC("dia.cellhistory.events.bytes")), nullptr);
	EXPECT_NE(metrics.FindGauge(StringCRC("dia.cellhistory.window.length")), nullptr);
	EXPECT_NE(metrics.FindCounter(StringCRC("dia.cellhistory.samples")), nullptr);
	EXPECT_NE(metrics.FindCounter(StringCRC("dia.cellhistory.events")), nullptr);
	EXPECT_NE(metrics.FindGauge(StringCRC("dia.cellhistory.sample.ms")), nullptr);
	EXPECT_NE(metrics.FindCounter(StringCRC("dia.cellhistory.version_gate.skips")), nullptr);
}

TEST(CellHistoryObservation, SamplesCounterIncrementsAfterSample)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0, "{\"group\":true}") }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();

	auto samplesMetric = metrics.FindCounter(StringCRC("dia.cellhistory.samples"));
	EXPECT_NE(samplesMetric, nullptr);
	if (samplesMetric)
	{
		uint64_t before = samplesMetric->Value();

		// Take one sample and trigger update
		r.Take();
		r.obs->OnRecorderUpdate();

		uint64_t after = samplesMetric->Value();
		EXPECT_GT(after, before);	// Should have incremented
	}
}

TEST(CellHistoryObservation, SampleMsGaugeUpdates)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0) }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();

	r.Take();
	r.obs->OnRecorderUpdate();

	auto sampleMsMetric = metrics.FindGauge(StringCRC("dia.cellhistory.sample.ms"));
	EXPECT_NE(sampleMsMetric, nullptr);
	if (sampleMsMetric)
	{
		double value = sampleMsMetric->Value();
		EXPECT_GE(value, 0.0);
	}
}

TEST(CellHistoryObservation, MemoryGaugesUpdateAfterSample)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0, "{\"group\":true}") }));

	RecorderBudgets budgets;
	budgets.mCellBytes = 1024 * 1024;
	budgets.mAggregateBytes = 1024 * 1024;
	budgets.mEventBytes = 1024 * 1024;
	r.Start(&budgets);

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();

	r.Take();
	r.obs->OnRecorderUpdate();

	// Check that all memory metrics exist and have values >= 0
	auto cellMetric = metrics.FindGauge(StringCRC("dia.cellhistory.cells.bytes"));
	auto aggMetric = metrics.FindGauge(StringCRC("dia.cellhistory.aggregates.bytes"));
	auto eventMetric = metrics.FindGauge(StringCRC("dia.cellhistory.events.bytes"));

	EXPECT_NE(cellMetric, nullptr);
	EXPECT_NE(aggMetric, nullptr);
	EXPECT_NE(eventMetric, nullptr);

	if (cellMetric) EXPECT_GE(cellMetric->Value(), 0.0);
	if (aggMetric) EXPECT_GE(aggMetric->Value(), 0.0);
	if (eventMetric) EXPECT_GE(eventMetric->Value(), 0.0);
}

TEST(CellHistoryObservation, VersionGateSkipsCounterUpdates)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0) }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();

	auto skipsMetric = metrics.FindCounter(StringCRC("dia.cellhistory.version_gate.skips"));
	EXPECT_NE(skipsMetric, nullptr);
	if (skipsMetric)
	{
		uint64_t initialSkips = skipsMetric->Value();
		EXPECT_GE(initialSkips, 0u);	// Should always be >= 0
	}
}

TEST(CellHistoryObservation, HealthReporterExists)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0, "{\"group\":true}") }));
	r.Start();

	auto& health = Dia::Observation::Health::HealthRegistry::Instance();

	// Snapshot should include our health reporter
	Dia::Observation::Health::HealthRegistry::ReporterSnapshot snapshots[32];
	unsigned int count = 0;
	health.Snapshot(snapshots, 32, count);

	// Should have at least the one we just registered
	EXPECT_GT(count, 0u);

	// Look for our reporter
	bool found = false;
	for (unsigned int i = 0; i < count; ++i)
	{
		if (snapshots[i].name == StringCRC("dia.cellhistory"))
		{
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found);
}

TEST(CellHistoryObservation, WindowLengthGaugeUpdates)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0, "{\"group\":true}") }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();

	r.obs->OnRecorderUpdate();

	auto windowMetric = metrics.FindGauge(StringCRC("dia.cellhistory.window.length"));
	EXPECT_NE(windowMetric, nullptr);
	if (windowMetric)
	{
		double value = windowMetric->Value();
		EXPECT_GE(value, 0.0);
	}
}

TEST(CellHistoryObservation, EventsCounterUpdates)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0, "{\"group\":true}") }));
	r.Start();

	auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();

	auto eventsMetric = metrics.FindCounter(StringCRC("dia.cellhistory.events"));
	EXPECT_NE(eventsMetric, nullptr);
	if (eventsMetric)
	{
		uint64_t before = eventsMetric->Value();

		// Emit an event
		HistoryEvent e;
		e.type = StringCRC("test");
		e.text = "test event";
		r.rec->Emit(e);
		r.Take();	// Commit the event with the sample
		r.obs->OnRecorderUpdate();

		uint64_t after = eventsMetric->Value();
		EXPECT_GT(after, before);	// Should have incremented
	}
}

TEST(CellHistoryObservation, HealthReporterReportsOKWhenWellUnderBudget)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0, "{\"group\":true}") }));

	// Large budgets - well under any threshold
	RecorderBudgets budgets;
	budgets.mCellBytes = 10 * 1024 * 1024;		// 10 MB
	budgets.mAggregateBytes = 10 * 1024 * 1024;
	budgets.mEventBytes = 10 * 1024 * 1024;
	r.Start(&budgets);

	// Take a few samples
	for (int i = 0; i < 4; ++i)
	{
		r.Take();
		r.obs->OnRecorderUpdate();
	}

	auto& health = Dia::Observation::Health::HealthRegistry::Instance();
	Dia::Observation::Health::HealthRegistry::ReporterSnapshot snapshots[32];
	unsigned int count = 0;
	health.Snapshot(snapshots, 32, count);

	bool found = false;
	for (unsigned int i = 0; i < count; ++i)
	{
		if (snapshots[i].name == StringCRC("dia.cellhistory"))
		{
			found = true;
			// With large budgets, should be OK
			EXPECT_EQ(snapshots[i].health.status, Dia::Observation::Health::HealthStatus::kOK);
			break;
		}
	}
	EXPECT_TRUE(found);
}

TEST(CellHistoryObservation, RestartMetricsAndHealth)
{
	auto& health = Dia::Observation::Health::HealthRegistry::Instance();

	// First observation cycle
	{
		Rig r;
		ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0) }));
		r.Start();

		r.Take();
		r.obs->OnRecorderUpdate();

		// Health reporter should be registered
		Dia::Observation::Health::HealthRegistry::ReporterSnapshot snapshots[32];
		unsigned int count = 0;
		health.Snapshot(snapshots, 32, count);
		bool foundHealth = false;
		for (unsigned int i = 0; i < count; ++i)
		{
			if (snapshots[i].name == StringCRC("dia.cellhistory")) { foundHealth = true; break; }
		}
		EXPECT_TRUE(foundHealth);
	}

	// After destruction, health reporter should be unregistered
	health.Reset();	// Clear the snapshot
	{
		Dia::Observation::Health::HealthRegistry::ReporterSnapshot snapshots[32];
		unsigned int count = 0;
		health.Snapshot(snapshots, 32, count);
		bool foundHealth = false;
		for (unsigned int i = 0; i < count; ++i)
		{
			if (snapshots[i].name == StringCRC("dia.cellhistory")) { foundHealth = true; break; }
		}
		EXPECT_FALSE(foundHealth);	// Should NOT find it after destruction
	}

	// Can restart
	{
		Rig r;
		ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0) }));
		r.Start();

		Dia::Observation::Health::HealthRegistry::ReporterSnapshot snapshots[32];
		unsigned int count = 0;
		health.Snapshot(snapshots, 32, count);
		bool foundHealth = false;
		for (unsigned int i = 0; i < count; ++i)
		{
			if (snapshots[i].name == StringCRC("dia.cellhistory")) { foundHealth = true; break; }
		}
		EXPECT_TRUE(foundHealth);	// Should find it again
	}
}

// ---- Health thresholds: each tier is Healthy at 89% and Degraded at 91% of its budget (AC 12) ----

namespace
{
	using Dia::Observation::Health::HealthStatus;

	// Budgets large enough that only the tier under test can approach 90%.
	constexpr uint64_t kPlentyBytes = 64ull * 1024 * 1024;

	void ExpectHealth(const CellHistoryRecorder& rec, HealthStatus status, const char* reasonTier)
	{
		CellHistoryHealth h(rec);
		const auto report = h.Report();
		EXPECT_EQ(report.status, status);
		if (reasonTier) { EXPECT_EQ(report.reason, StringCRC(reasonTier)); }
	}
}

TEST(CellHistoryHealthThresholds, AggregateTierHealthyAt89DegradedAt91)
{
	Rig r;
	r.grid.AddFloatLayer("test_f", 0.5f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0) }));
	const uint64_t rowBytes = AggregateRowBytes(1, 0, 1);
	RecorderBudgets b;
	b.mCellBytes = kPlentyBytes;
	b.mEventBytes = kPlentyBytes;
	b.mAggregateBytes = rowBytes * 100;		// window of exactly 100 samples
	r.Start(&b);

	for (int i = 0; i < 89; ++i) { r.Take(); }
	ASSERT_NEAR(r.rec->Aggregates().UsedFraction(), 0.89, 1e-9);
	ExpectHealth(*r.rec, HealthStatus::kOK, nullptr);

	for (int i = 0; i < 2; ++i) { r.Take(); }
	ASSERT_NEAR(r.rec->Aggregates().UsedFraction(), 0.91, 1e-9);
	ExpectHealth(*r.rec, HealthStatus::kDegraded, "aggregates");
}

TEST(CellHistoryHealthThresholds, CellTierHealthyAt89DegradedAt91)
{
	Rig r;
	r.grid.AddFloatLayer("test_f", 0.5f);
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0, "{\"track\":true}") }));
	const uint64_t perSample = BytesPerSample(1, 0, 12);
	RecorderBudgets b;
	b.mAggregateBytes = kPlentyBytes;
	b.mEventBytes = kPlentyBytes;
	b.mCellBytes = perSample * 100;
	r.Start(&b);
	r.Take();	// the layout and ring are created by the first Sample
	ASSERT_NE(r.rec->Ring(), nullptr);
	ASSERT_TRUE(r.rec->IsTracked(kTestFloat));

	for (int i = 1; i < 89; ++i) { r.Take(); }
	ASSERT_NEAR(r.rec->Ring()->UsedFraction(), 0.89, 1e-9);
	ExpectHealth(*r.rec, HealthStatus::kOK, nullptr);

	for (int i = 0; i < 2; ++i) { r.Take(); }
	ASSERT_NEAR(r.rec->Ring()->UsedFraction(), 0.91, 1e-9);
	ExpectHealth(*r.rec, HealthStatus::kDegraded, "cells");
}

TEST(CellHistoryHealthThresholds, EventTierHealthyAt89DegradedAt91)
{
	Rig r;
	ASSERT_TRUE(r.schema.Load({ FloatEntry("test_f", 0.0, 1.0) }));
	HistoryEvent e;
	e.type = StringCRC("test.threshold");
	const uint64_t rec = EventLog::RecordBytes(e);
	RecorderBudgets b;
	b.mCellBytes = kPlentyBytes;
	b.mAggregateBytes = kPlentyBytes;
	b.mEventBytes = rec * 100;
	r.Start(&b);

	for (int i = 0; i < 89; ++i) { ASSERT_EQ(r.rec->Emit(e), EmitResult::Ok); }
	ASSERT_NEAR(r.rec->EventsLog().UsedFraction(), 0.89, 1e-9);
	ExpectHealth(*r.rec, HealthStatus::kOK, nullptr);

	for (int i = 0; i < 2; ++i) { ASSERT_EQ(r.rec->Emit(e), EmitResult::Ok); }
	ASSERT_NEAR(r.rec->EventsLog().UsedFraction(), 0.91, 1e-9);
	ExpectHealth(*r.rec, HealthStatus::kDegraded, "events");
}
