#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/SelectionTools.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaAPI/DiaAPI.h>

#include <sstream>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

// The seven cells.select.* DiaAPI JSON commands act on the active grid's selection (ActiveSelection()).

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;
typedef LogCapture::Level Level;

namespace
{
	struct Dim { uint16_t w; uint16_t h; };
	const Dim kDims[] = { { 66, 42 }, { 104, 64 } };

	// Enum values: grass 0, water 1, rock 2, sand 3, unused 4.
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
			t.mEnumValues.Reserve(5);
			for (const char* id : { "grass", "water", "rock", "sand", "unused" })
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
	Json::Value Exec(const char* cmd, const Json::Value& params) { return Dia::API::ExecuteCommandJson(StringCRC(cmd), params); }

	struct SelectionCommandsTest : ::testing::Test
	{
		void SetUp() override { FakePuId() = 1; Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};

	uint32_t CountWhere(const Rig& rig, uint32_t mod, uint32_t eq)
	{
		uint32_t n = 0;
		for (uint32_t i = 0; i < rig.grid.CellCount(); ++i) { if (i % mod == eq) { ++n; } }
		return n;
	}
}

TEST_F(SelectionCommandsTest, CellSelectsOneCellAndReportsCount)
{
	for (const Dim& d : kDims)
	{
		Rig rig(d.w, d.h);
		rig.domain.ActiveSelection()->Set(0, true);
		rig.domain.ActiveSelection()->Set(1, true);
		const Json::Value r = Exec("cells.select.cell", "{\"q\":3,\"r\":2}");
		ASSERT_TRUE(r["success"].asBool());
		EXPECT_TRUE(r["data"]["ok"].asBool());
		EXPECT_EQ(r["data"]["count"].asUInt(), 1u);
		EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(rig.Idx(3, 2)));
		EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 1u);
	}
}

TEST_F(SelectionCommandsTest, ToggleAddsThenRemoves)
{
	for (const Dim& d : kDims)
	{
		Rig rig(d.w, d.h);
		Json::Value r = Exec("cells.select.toggle", "{\"q\":4,\"r\":5}");
		ASSERT_TRUE(r["success"].asBool());
		EXPECT_EQ(r["data"]["count"].asUInt(), 1u);
		EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(rig.Idx(4, 5)));
		r = Exec("cells.select.toggle", "{\"q\":4,\"r\":5}");
		EXPECT_EQ(r["data"]["count"].asUInt(), 0u);
		EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 0u);
	}
}

TEST_F(SelectionCommandsTest, BoxAndReversedCornersSelectSameRect)
{
	for (const Dim& d : kDims)
	{
		Rig rig(d.w, d.h);
		Json::Value r = Exec("cells.select.box", "{\"q0\":2,\"r0\":1,\"q1\":4,\"r1\":3}");
		ASSERT_TRUE(r["success"].asBool());
		EXPECT_EQ(r["data"]["count"].asUInt(), 9u);
		const uint32_t v = rig.domain.ActiveSelection()->Version();
		r = Exec("cells.select.box", "{\"q0\":4,\"r0\":3,\"q1\":2,\"r1\":1}");
		ASSERT_TRUE(r["success"].asBool());
		EXPECT_EQ(r["data"]["count"].asUInt(), 9u);
		EXPECT_EQ(rig.domain.ActiveSelection()->Version(), v);	// same set: no net change
		EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(rig.Idx(3, 2)));
	}
}

TEST_F(SelectionCommandsTest, PolyMatchesTool)
{
	for (const Dim& d : kDims)
	{
		Rig rig(d.w, d.h);
		// Small square around the centre of cell (0,0) (world origin) selects exactly it.
		Json::Value r = Exec("cells.select.poly", "{\"points\":[[-0.5,-0.5],[0.5,-0.5],[0.5,0.5],[-0.5,0.5]]}");
		ASSERT_TRUE(r["success"].asBool());
		EXPECT_EQ(r["data"]["count"].asUInt(), 1u);
		EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(0));

		// Triangle: same cells as the tool called directly.
		r = Exec("cells.select.poly", "{\"points\":[[-1,-1],[30,-1],[-1,30]]}");
		ASSERT_TRUE(r["success"].asBool());
		CellSelection ref;
		ref.Reset(&rig.grid.desc);
		const SelectionTools::PolyPoint tri[3] = { { -1, -1 }, { 30, -1 }, { -1, 30 } };
		SelectionTools::SelectPolygon(ref, tri, 3);
		EXPECT_GT(ref.Count(), 1u);
		EXPECT_EQ(r["data"]["count"].asUInt(), ref.Count());
		EXPECT_EQ(ToVector(*rig.domain.ActiveSelection()), ToVector(ref));
	}
}

TEST_F(SelectionCommandsTest, CategoryByEnumIdAndByNumber)
{
	for (const Dim& d : kDims)
	{
		Rig rig(d.w, d.h);
		Json::Value r = Exec("cells.select.category", "{\"layer\":\"terrain\",\"value\":\"water\"}");
		ASSERT_TRUE(r["success"].asBool());
		EXPECT_EQ(r["data"]["count"].asUInt(), CountWhere(rig, 4, 1));
		EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(1));
		const auto byId = ToVector(*rig.domain.ActiveSelection());

		Exec("cells.select.category", "{\"layer\":\"terrain\",\"value\":\"sand\"}");
		r = Exec("cells.select.category", "{\"layer\":\"terrain\",\"value\":1}");	// JSON number
		ASSERT_TRUE(r["success"].asBool());
		EXPECT_EQ(ToVector(*rig.domain.ActiveSelection()), byId);

		Exec("cells.select.category", "{\"layer\":\"terrain\",\"value\":\"sand\"}");
		r = Exec("cells.select.category", "{\"layer\":\"terrain\",\"value\":\"1\"}");	// numeric string
		ASSERT_TRUE(r["success"].asBool());
		EXPECT_EQ(ToVector(*rig.domain.ActiveSelection()), byId);
	}
}

TEST_F(SelectionCommandsTest, CellsReportsRequestedAndAcceptedIgnoringOutOfBoundsAndDuplicates)
{
	for (const Dim& d : kDims)
	{
		Rig rig(d.w, d.h);
		const Json::Value r = Exec("cells.select.cells", "{\"cells\":[[5,5],[1,2],[5,5],[-1,0],[9999,3],[0,0]]}");
		ASSERT_TRUE(r["success"].asBool());
		EXPECT_EQ(r["data"]["count"].asUInt(), 3u);
		EXPECT_EQ(r["data"]["requested"].asUInt(), 6u);
		ASSERT_EQ(r["data"]["accepted"].size(), 3u);
		EXPECT_EQ(r["data"]["accepted"][0][0].asInt(), 5);
		EXPECT_EQ(r["data"]["accepted"][0][1].asInt(), 5);
		EXPECT_EQ(r["data"]["accepted"][1][0].asInt(), 1);
		EXPECT_EQ(r["data"]["accepted"][1][1].asInt(), 2);
		EXPECT_EQ(r["data"]["accepted"][2][0].asInt(), 0);
		EXPECT_FALSE(r["data"].isMember("overCap"));
		EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(rig.Idx(1, 2)));
		EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 3u);
	}
}

TEST_F(SelectionCommandsTest, CellsOverCapReportsFlagAndWarnsOnce)
{
	Rig rig(104, 64);
	Json::Value cells(Json::arrayValue);
	for (int i = 0; i < 2100; ++i)
	{
		Json::Value p(Json::arrayValue);
		p.append(i % 100);
		p.append(i / 100);
		cells.append(p);
	}
	Json::Value params(Json::objectValue);
	params["cells"] = cells;

	LogCapture log;
	log.Clear();
	const Json::Value r = Exec("cells.select.cells", params);
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_TRUE(r["data"]["overCap"].asBool());
	EXPECT_EQ(r["data"]["requested"].asUInt(), 2100u);
	EXPECT_EQ(r["data"]["count"].asUInt(), 2048u);
	EXPECT_EQ(r["data"]["accepted"].size(), 2048u);
	EXPECT_EQ(log.Warnings(), 1u);
}

TEST_F(SelectionCommandsTest, ClearEmptiesSelection)
{
	for (const Dim& d : kDims)
	{
		Rig rig(d.w, d.h);
		Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":3,\"r1\":3}");
		ASSERT_EQ(rig.domain.ActiveSelectionView()->Count(), 16u);
		const Json::Value r = Exec("cells.select.clear", "{}");
		ASSERT_TRUE(r["success"].asBool());
		EXPECT_EQ(r["data"]["count"].asUInt(), 0u);
		EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 0u);
	}
}

TEST_F(SelectionCommandsTest, BadArgsFailLeaveSelectionAndVersionAndLogInfo)
{
	struct Bad { const char* cmd; const char* params; const char* msg; };
	const Bad bad[] = {
		{ "cells.select.cell", "{}", "'q'" },
		{ "cells.select.cell", "{\"q\":1}", "'r'" },
		{ "cells.select.cell", "{\"q\":1.5,\"r\":2}", "'q'" },
		{ "cells.select.cell", "{\"q\":\"1\",\"r\":2}", "'q'" },
		{ "cells.select.cell", "{\"q\":1,\"r\":null}", "'r'" },
		{ "cells.select.toggle", "{\"q\":1}", "'r'" },
		{ "cells.select.toggle", "{\"q\":true,\"r\":1}", "'q'" },
		{ "cells.select.box", "{\"q0\":1,\"r0\":1,\"q1\":2}", "'r1'" },
		{ "cells.select.box", "{\"q0\":1,\"r0\":1,\"q1\":\"x\",\"r1\":2}", "'q1'" },
		{ "cells.select.poly", "{}", "'points'" },
		{ "cells.select.poly", "{\"points\":[[0,0],[1,1]]}", "at least 3 points" },
		{ "cells.select.poly", "{\"points\":[[0,0],[1,1],[\"a\",2]]}", "point" },
		{ "cells.select.poly", "{\"points\":[[0,0],[1,1],[2]]}", "point" },
		{ "cells.select.poly", "{\"points\":[[0,0],[1,1],5]}", "point" },
		{ "cells.select.poly", "{\"points\":\"x\"}", "'points'" },
		{ "cells.select.category", "{\"value\":\"water\"}", "'layer'" },
		{ "cells.select.category", "{\"layer\":\"terrain\"}", "'value'" },
		{ "cells.select.category", "{\"layer\":\"nope\",\"value\":\"water\"}", "unknown layer id 'nope'" },
		{ "cells.select.category", "{\"layer\":\"terrain\",\"value\":\"lava\"}", "unknown value 'lava'" },
		{ "cells.select.category", "{\"layer\":\"temp\",\"value\":1}", "layer 'temp' is not a category layer" },
		{ "cells.select.cells", "{}", "'cells'" },
		{ "cells.select.cells", "{\"cells\":[[1,2],[3]]}", "pair" },
		{ "cells.select.cells", "{\"cells\":[[1,2],[\"a\",3]]}", "pair" },
		{ "cells.select.cells", "{\"cells\":[[1,2.5]]}", "pair" },
	};
	Rig rig(66, 42);
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":2,\"r1\":2}");
	const auto before = ToVector(*rig.domain.ActiveSelection());
	const uint32_t version = rig.domain.ActiveSelection()->Version();
	ASSERT_EQ(before.size(), 9u);

	LogCapture log;
	for (const Bad& b : bad)
	{
		log.Clear();
		const Json::Value r = Exec(b.cmd, b.params);
		EXPECT_FALSE(r["success"].asBool()) << b.cmd << " " << b.params;
		EXPECT_NE(r["error"].asString().find(b.msg), std::string::npos) << b.cmd << " " << b.params << " -> " << r["error"].asString();
		EXPECT_EQ(log.Count(Level::kInfo), 1u) << b.cmd << " " << b.params;
		EXPECT_EQ(ToVector(*rig.domain.ActiveSelection()), before) << b.cmd << " " << b.params;
		EXPECT_EQ(rig.domain.ActiveSelection()->Version(), version) << b.cmd << " " << b.params;
	}
	EXPECT_FALSE(Exec("cells.select.cell", Json::Value())["success"].asBool());
}

TEST_F(SelectionCommandsTest, NoActiveGridFailsEveryCommand)
{
	CellFieldRegistry cf;
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	LayerUploader uploader(cf, nullptr);
	CellLayerViewerDomain domain(registry, uploader, nullptr);
	FakeDebugDraw draw;
	domain.RegisterCommands();
	domain.Update(draw);
	ASSERT_EQ(domain.ActiveSelectionView(), nullptr);

	const char* const calls[][2] = {
		{ "cells.select.cell", "{\"q\":1,\"r\":1}" },
		{ "cells.select.toggle", "{\"q\":1,\"r\":1}" },
		{ "cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":1,\"r1\":1}" },
		{ "cells.select.poly", "{\"points\":[[0,0],[1,0],[0,1]]}" },
		{ "cells.select.category", "{\"layer\":\"a\",\"value\":1}" },
		{ "cells.select.cells", "{\"cells\":[[1,1]]}" },
		{ "cells.select.clear", "{}" },
	};
	for (const auto& c : calls)
	{
		const Json::Value r = Exec(c[0], c[1]);
		EXPECT_FALSE(r["success"].asBool()) << c[0];
		EXPECT_NE(r["error"].asString().find("no active grid"), std::string::npos) << c[0];
	}
}

TEST_F(SelectionCommandsTest, SuccessBumpsVersionOnceFailureDoesNot)
{
	Rig rig(66, 42);
	CellSelection* sel = rig.domain.ActiveSelection();
	uint32_t v = sel->Version();
	ASSERT_TRUE(Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":5,\"r1\":5}")["success"].asBool());
	EXPECT_EQ(sel->Version(), v + 1);
	v = sel->Version();
	ASSERT_TRUE(Exec("cells.select.category", "{\"layer\":\"terrain\",\"value\":\"rock\"}")["success"].asBool());
	EXPECT_EQ(sel->Version(), v + 1);
	v = sel->Version();
	ASSERT_TRUE(Exec("cells.select.cells", "{\"cells\":[[1,1],[2,2],[3,3]]}")["success"].asBool());
	EXPECT_EQ(sel->Version(), v + 1);
	v = sel->Version();
	EXPECT_FALSE(Exec("cells.select.cell", "{\"q\":\"x\",\"r\":1}")["success"].asBool());
	EXPECT_EQ(sel->Version(), v);
}

TEST_F(SelectionCommandsTest, OutOfBoundsCoordinatesIgnored)
{
	Rig rig(66, 42);
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	const uint32_t v = rig.domain.ActiveSelection()->Version();
	const Json::Value r = Exec("cells.select.toggle", "{\"q\":-1,\"r\":9999}");
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["data"]["count"].asUInt(), 1u);
	EXPECT_EQ(rig.domain.ActiveSelection()->Version(), v);
}

TEST_F(SelectionCommandsTest, CommandsUnregisterWithDomain)
{
	Rig rig(66, 42);
	rig.domain.UnregisterCommands();
	EXPECT_FALSE(Exec("cells.select.clear", "{}")["success"].asBool());
}

#endif // DIA_DEBUG
