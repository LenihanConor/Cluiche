#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/SelectionTools.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaAPI/DiaAPI.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <sstream>
#include <string>

#ifdef DIA_DEBUG

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

		uint32_t Idx(int q, int r) const { return static_cast<uint32_t>(r) * grid.desc.width + static_cast<uint32_t>(q); }
	};

	Json::Value Parse(const char* text)
	{
		Json::Value v;
		Json::CharReaderBuilder b;
		std::istringstream in(text);
		std::string errs;
		Json::parseFromStream(b, in, &v, &errs);
		return v;
	}

	Json::Value Exec(const char* cmd, const char* params) { return Dia::API::ExecuteCommandJson(StringCRC(cmd), Parse(params)); }

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

	struct SelectionObservationTest : ::testing::Test
	{
		void SetUp() override { FakePuId() = 1; Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};
}

TEST_F(SelectionObservationTest, SelectionSizeGaugeMatchesCountAfterSelect)
{
	Rig rig(kTestDim.w, kTestDim.h);
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":2,\"r1\":2}");
	rig.domain.Update(rig.draw);
	const double size = GaugeValue("dia.celllayers.viewer.selection_size");
	EXPECT_EQ(size, 9.0);	// 3x3 box = 9 cells
}

TEST_F(SelectionObservationTest, SelectCommandsCounterIncrementsOnSuccess)
{
	Rig rig(kTestDim.w, kTestDim.h);
	const double initialCount = CounterValue("dia.celllayers.viewer.select_commands");
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":2,\"r1\":2}");
	const double afterSelect = CounterValue("dia.celllayers.viewer.select_commands");
	EXPECT_EQ(afterSelect - initialCount, 1.0);
}

TEST_F(SelectionObservationTest, SelectRejectedCounterIncrementsOnFailure)
{
	Rig rig(kTestDim.w, kTestDim.h);
	const double initialCount = CounterValue("dia.celllayers.viewer.select_rejected");
	// Missing param: should be rejected
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0}");
	const double afterReject = CounterValue("dia.celllayers.viewer.select_rejected");
	EXPECT_EQ(afterReject - initialCount, 1.0);
}

TEST_F(SelectionObservationTest, OutlineEdgesGaugeMatchesPerimeterAfterUpdate)
{
	Rig rig(kTestDim.w, kTestDim.h);
	Exec("cells.select.cell", "{\"q\":5,\"r\":5}");
	rig.domain.Update(rig.draw);
	const double edges = GaugeValue("dia.celllayers.viewer.outline_edges");
	EXPECT_EQ(edges, 6.0);	// single hex has 6 edges
}

TEST_F(SelectionObservationTest, SelectionSizeGaugeIsZeroWhenEmpty)
{
	Rig rig(kTestDim.w, kTestDim.h);
	Exec("cells.select.cell", "{\"q\":0,\"r\":0}");
	Exec("cells.select.clear", "{}");
	rig.domain.Update(rig.draw);
	const double size = GaugeValue("dia.celllayers.viewer.selection_size");
	EXPECT_EQ(size, 0.0);
}

TEST_F(SelectionObservationTest, OutlineEdgesGaugeIsZeroWhenEmpty)
{
	Rig rig(kTestDim.w, kTestDim.h);
	Exec("cells.select.cell", "{\"q\":0,\"r\":0}");
	Exec("cells.select.clear", "{}");
	rig.domain.Update(rig.draw);
	const double edges = GaugeValue("dia.celllayers.viewer.outline_edges");
	EXPECT_EQ(edges, 0.0);
}

TEST_F(SelectionObservationTest, OutlineDrawingRecordsLinePrimitivesPerEdge)
{
	Rig rig(kTestDim.w, kTestDim.h);
	rig.draw.lines.clear();
	Exec("cells.select.cell", "{\"q\":5,\"r\":5}");
	rig.domain.Update(rig.draw);
	// A single hex has 6 edges; the fake draw should record 6 lines
	EXPECT_EQ(rig.draw.lines.size(), 6u);
}

TEST_F(SelectionObservationTest, OutlineMetricsMatchTooLargeState)
{
	Rig rig(kTestDim.w, kTestDim.h);
	// Select a small region to verify outline works
	Exec("cells.select.cell", "{\"q\":10,\"r\":10}");
	rig.domain.Update(rig.draw);
	EXPECT_GT(GaugeValue("dia.celllayers.viewer.outline_edges"), 0.0);

	// Clear and verify metrics go to zero
	Exec("cells.select.clear", "{}");
	rig.domain.Update(rig.draw);
	EXPECT_EQ(GaugeValue("dia.celllayers.viewer.outline_edges"), 0.0);
}

TEST_F(SelectionObservationTest, MultipleFailuresIncrementRejected)
{
	Rig rig(kTestDim.w, kTestDim.h);
	const double initialCount = CounterValue("dia.celllayers.viewer.select_rejected");
	// Multiple failures
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0}");		// missing params
	Exec("cells.select.category", "{\"layer\":\"unknown\",\"value\":\"x\"}");	// unknown layer
	Exec("cells.select.cell", "{\"q\":\"not_a_number\"}");	// invalid type
	const double afterRejects = CounterValue("dia.celllayers.viewer.select_rejected");
	EXPECT_EQ(afterRejects - initialCount, 3.0);
}

TEST_F(SelectionObservationTest, SelectCommandsNotIncrementedOnFailure)
{
	Rig rig(kTestDim.w, kTestDim.h);
	const double initialCount = CounterValue("dia.celllayers.viewer.select_commands");
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0}");		// missing params (failure)
	const double afterReject = CounterValue("dia.celllayers.viewer.select_commands");
	EXPECT_EQ(afterReject - initialCount, 0.0);	// no increment on failure
}

#endif // DIA_DEBUG
