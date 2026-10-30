#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaAPI/DiaAPI.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <sstream>
#include <string>

#ifdef DIA_DEBUG

// Query observation metrics: dia.celllayers.query.run (counter), .rejected (counter),
// .evaluate_ms (gauge), .last_match_count (gauge).

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;

namespace
{
	struct Dim { uint16_t w; uint16_t h; };
	const Dim kTestDim = { 66, 42 };

	// Enum values: grass 0, water 1, rock 2, sand 3.
	struct Rig
	{
		TestGrid grid;
		CellFieldRegistry cellField;
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		StubProvider terrain, temp;
		FakeDebugDraw draw;
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;

		static const Dia::ValueSchema::ValueSchemaService* Build(UploaderSchemaFixture& f)
		{
			Dia::ValueSchema::ValueInfo t = Dia::ValueSchema::Testing::MakeValueInfo("cells", "terrain");
			t.mType = Dia::ValueSchema::ValueType::Enum;
			t.mEnumValues.Reserve(4);
			for (const char* id : { "grass", "water", "rock", "sand" })
			{
				Dia::ValueSchema::EnumValue v;
				Dia::ValueSchema::SetText(v.mId, id);
				t.mEnumValues.Add(v);
			}
			f.Add(t);
			f.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
			return &f.Publish();
		}

		Rig(uint16_t w, uint16_t h)
			: grid(w, h)
			, service(Build(fx))
			, registry(nullptr, &FakePuIdFn)
			, uploader(cellField, nullptr)
			, domain(registry, uploader, service)
		{
			terrain.values.resize(static_cast<size_t>(w) * h);
			for (size_t i = 0; i < terrain.values.size(); ++i) { terrain.values[i] = static_cast<float>(i % 4); }
			temp.values = std::vector<float>(static_cast<size_t>(w) * h, 1.0f);
			registry.Register(terrain.MakeDesc("terrain", &grid.desc, CellValueKind::Category, "cells/terrain"));
			registry.Register(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
			domain.RegisterCommands();
			domain.Update(draw);
		}

		uint32_t WaterCount() const
		{
			uint32_t n = 0;
			for (uint32_t i = 0; i < grid.CellCount(); ++i) { if (i % 4 == 1) { ++n; } }
			return n;
		}
	};

	Json::Value Expr(const char* text)
	{
		Json::Value p(Json::objectValue);
		p["expr"] = text;
		return p;
	}

	Json::Value Exec(const char* cmd, const Json::Value& params) { return Dia::API::ExecuteCommandJson(StringCRC(cmd), params); }

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

	struct QueryObservationTest : ::testing::Test
	{
		void SetUp() override { FakePuId() = 1; Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};
}

TEST_F(QueryObservationTest, SelectQueryRunCounterIncrementsOnSuccess)
{
	Rig rig(kTestDim.w, kTestDim.h);
	const double initialCount = CounterValue("dia.celllayers.query.run");
	Exec("cells.select.query", Expr("terrain == water"));
	const double afterSelect = CounterValue("dia.celllayers.query.run");
	EXPECT_EQ(afterSelect - initialCount, 1.0);
}

TEST_F(QueryObservationTest, SelectQueryRejectedCounterIncrementsOnFailure)
{
	Rig rig(kTestDim.w, kTestDim.h);
	const double initialCount = CounterValue("dia.celllayers.query.rejected");
	// Bad query: missing operand
	Exec("cells.select.query", Expr("terrain >"));
	const double afterReject = CounterValue("dia.celllayers.query.rejected");
	EXPECT_EQ(afterReject - initialCount, 1.0);
}

TEST_F(QueryObservationTest, CheckQueryDoesNotIncrementRunOrRejected)
{
	Rig rig(kTestDim.w, kTestDim.h);
	const double initialRun = CounterValue("dia.celllayers.query.run");
	const double initialRejected = CounterValue("dia.celllayers.query.rejected");

	// Successful check
	Exec("cells.query.check", Expr("terrain == water"));
	EXPECT_EQ(CounterValue("dia.celllayers.query.run"), initialRun);
	EXPECT_EQ(CounterValue("dia.celllayers.query.rejected"), initialRejected);

	// Failed check
	Exec("cells.query.check", Expr("terrain >"));
	EXPECT_EQ(CounterValue("dia.celllayers.query.run"), initialRun);
	EXPECT_EQ(CounterValue("dia.celllayers.query.rejected"), initialRejected);
}

TEST_F(QueryObservationTest, SelectQueryUpdatesLastMatchCount)
{
	Rig rig(kTestDim.w, kTestDim.h);
	const uint32_t expectedWaterCount = rig.WaterCount();
	Exec("cells.select.query", Expr("terrain == water"));
	const double matchCount = GaugeValue("dia.celllayers.query.last_match_count");
	EXPECT_EQ(matchCount, static_cast<double>(expectedWaterCount));
}

TEST_F(QueryObservationTest, CheckQueryUpdatesLastMatchCount)
{
	Rig rig(kTestDim.w, kTestDim.h);
	const uint32_t expectedWaterCount = rig.WaterCount();
	Exec("cells.query.check", Expr("terrain == water"));
	const double matchCount = GaugeValue("dia.celllayers.query.last_match_count");
	EXPECT_EQ(matchCount, static_cast<double>(expectedWaterCount));
}

TEST_F(QueryObservationTest, EvaluateMsGaugeIsNonNegativeAfterQuery)
{
	Rig rig(kTestDim.w, kTestDim.h);
	Exec("cells.select.query", Expr("terrain == water"));
	const double ms = GaugeValue("dia.celllayers.query.evaluate_ms");
	EXPECT_GE(ms, 0.0);
}

TEST_F(QueryObservationTest, EvaluateMsGaugeIsNonNegativeAfterCheck)
{
	Rig rig(kTestDim.w, kTestDim.h);
	Exec("cells.query.check", Expr("terrain == water"));
	const double ms = GaugeValue("dia.celllayers.query.evaluate_ms");
	EXPECT_GE(ms, 0.0);
}

TEST_F(QueryObservationTest, MultipleSelectQueriesIncrementRunCounter)
{
	Rig rig(kTestDim.w, kTestDim.h);
	const double initialCount = CounterValue("dia.celllayers.query.run");
	Exec("cells.select.query", Expr("terrain == water"));
	Exec("cells.select.query", Expr("terrain == grass"));
	Exec("cells.select.query", Expr("temp >= 1"));
	const double afterQueries = CounterValue("dia.celllayers.query.run");
	EXPECT_EQ(afterQueries - initialCount, 3.0);
}

TEST_F(QueryObservationTest, MultipleSelectRejectsIncrementRejectedCounter)
{
	Rig rig(kTestDim.w, kTestDim.h);
	const double initialCount = CounterValue("dia.celllayers.query.rejected");
	Exec("cells.select.query", Expr("terrain >"));			// bad parse
	Exec("cells.select.query", Expr("unknown == 1"));		// unknown layer
	Exec("cells.select.query", Expr(""));					// empty
	const double afterRejects = CounterValue("dia.celllayers.query.rejected");
	EXPECT_EQ(afterRejects - initialCount, 3.0);
}

#endif // DIA_DEBUG
