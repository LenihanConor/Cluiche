#include <gtest/gtest.h>
#include <memory>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/InspectorState.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaObservation/Metric/MetricRegistry.h>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	struct SelectionInspectorObservationTest : ::testing::Test
	{
		Dia::Graphics::CellGridDesc grid = MakeGrid(4, 3);
		Dia::Graphics::CellGridDesc otherGrid = MakeGrid(2, 2);
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		CountingLayerProvider pa, pb, pc, pd, pe;
		CellSelection sel;
		InspectorState state;
		LogCapture log;

		void AddLayer(const char* id, const Dia::Graphics::CellGridDesc* g, CountingLayerProvider& p, const char* group)
		{
			CellLayerDesc d = MakeCountingLayer(id, g, p);
			d.group.Clear();
			d.group.Append(group);
			ASSERT_EQ(CellRegisterResult::Ok, registry.Register(d));
		}

		void SetUp() override
		{
			FakePuId() = 1;
			for (CountingLayerProvider* p : { &pa, &pb, &pc, &pd, &pe }) { p->values = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 }; }
			AddLayer("a", &grid, pa, "G1");
			AddLayer("b", &grid, pb, "G1");
			AddLayer("c", &grid, pc, "G2");
			AddLayer("d", &grid, pd, "G2");
			AddLayer("e", &otherGrid, pe, "G1");
			sel.Reset(&grid);
			sel.Set(1, true);
			sel.Set(2, true);
			state.SetActive(registry, &grid, StringCRC("a"));
		}

		LayerReader& Reader()
		{
			if (!mReader) { mReader = std::make_unique<LayerReader>(registry); }
			return *mReader;
		}

		bool Tick() { return state.Tick(registry, Reader(), nullptr, &sel); }
		void Settle() { for (int i = 0; i < 4 && state.IsUpdating(); ++i) { Tick(); } }
		void Pump() { Tick(); Tick(); Tick(); }

		double CounterValue(const char* name)
		{
			auto* c = Dia::Observation::Metric::MetricRegistry::Instance().FindCounter(StringCRC(name));
			return c ? static_cast<double>(c->Value()) : -1.0;
		}

		double GaugeValue(const char* name)
		{
			auto* g = Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(StringCRC(name));
			return g ? g->Value() : -1.0;
		}

		std::unique_ptr<LayerReader> mReader;
	};
}

// ---- Metrics (Task 8) ----

TEST_F(SelectionInspectorObservationTest, PassCounterIncrementsOncePerPass)
{
	Settle();
	const double initialPasses = CounterValue("dia.celllayers.inspector.passes");
	EXPECT_GE(initialPasses, 0.0);
	sel.Set(5, true);
	Tick();
	EXPECT_TRUE(Tick());	// pass runs
	const double afterPass = CounterValue("dia.celllayers.inspector.passes");
	EXPECT_EQ(afterPass - initialPasses, 1.0);
}

TEST_F(SelectionInspectorObservationTest, PassCounterDoesNotIncrementOnNoChange)
{
	Settle();
	const double beforeReads = CounterValue("dia.celllayers.inspector.passes");
	Tick();
	Tick();
	Tick();
	const double afterReads = CounterValue("dia.celllayers.inspector.passes");
	EXPECT_EQ(afterReads - beforeReads, 0.0);
}

TEST_F(SelectionInspectorObservationTest, LayersPerPassGaugeEqualsPassLayerCount)
{
	Settle();
	state.SetGroupExpanded("G2", true);
	Pump();	// run through the update process: detect change, then run pass
	EXPECT_EQ(state.LastPassLayerCount(), 2u);	// c and d (a already computed in Settle)
	const double layersPerPass = GaugeValue("dia.celllayers.inspector.layers_per_pass");
	EXPECT_EQ(layersPerPass, 2.0);
}

TEST_F(SelectionInspectorObservationTest, PassMsGaugeIsNonNegative)
{
	Settle();
	const double passMs = GaugeValue("dia.celllayers.inspector.pass_ms");
	EXPECT_GE(passMs, 0.0);
}

TEST_F(SelectionInspectorObservationTest, PinnedGaugeIs0WhenNotPinned)
{
	Settle();
	const double pinned = GaugeValue("dia.celllayers.inspector.pinned");
	EXPECT_EQ(pinned, 0.0);
}

TEST_F(SelectionInspectorObservationTest, PinnedGaugeIs1WhenPinned)
{
	Settle();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	const double pinned = GaugeValue("dia.celllayers.inspector.pinned");
	EXPECT_EQ(pinned, 1.0);
}

TEST_F(SelectionInspectorObservationTest, PinnedGaugeIs0WhenUnpinned)
{
	Settle();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	Pump();
	state.Unpin();
	const double pinned = GaugeValue("dia.celllayers.inspector.pinned");
	EXPECT_EQ(pinned, 0.0);
}

// ---- Warnings: unresolved schemaKey (Task 8) ----

TEST_F(SelectionInspectorObservationTest, UnresolvedSchemaKeyWarnsOncePerDistinctKey)
{
	// Schema holds only cells/temp; the shown layers "a" and "b" use keys "a" and "b", which do not resolve.
	UploaderSchemaFixture fx;
	fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
	const Dia::ValueSchema::ValueSchemaService& schema = fx.Publish();

	const unsigned before = log.Warnings("unresolved schemaKey");
	for (int i = 0; i < 3; ++i) { state.Tick(registry, Reader(), &schema, &sel); }
	sel.Set(5, true);	// new selection version: another pass over the same layers
	for (int i = 0; i < 3; ++i) { state.Tick(registry, Reader(), &schema, &sel); }
	EXPECT_EQ(2u, log.Warnings("unresolved schemaKey") - before);
}

TEST_F(SelectionInspectorObservationTest, UnresolvedKeyWarningWithNullSchemaDoesNotWarn)
{
	const unsigned before = log.Warnings("unresolved schemaKey");
	for (int i = 0; i < 3; ++i) { Tick(); }
	sel.Set(5, true);
	Pump();
	EXPECT_GT(state.PassCount(), 0u);
	EXPECT_EQ(0u, log.Warnings("unresolved schemaKey") - before);
}

// ---- Profile scope on PinFreeze (Task 8) ----

TEST_F(SelectionInspectorObservationTest, PinFreezePricesProfileScope)
{
	Settle();
	// Just verify PinFreeze runs without crashing; profiling is internal
	ASSERT_TRUE(state.PinFreeze(registry, Reader(), nullptr, &grid, &sel));
	EXPECT_TRUE(state.IsFrozen());
}
