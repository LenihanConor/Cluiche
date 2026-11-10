#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/ViewerCommands.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaAPI/DiaAPI.h>

#include <sstream>
#include <string>

#ifdef DIA_DEBUG

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;

namespace
{
	// Layers: terrain (category), temp (float), zone (category).
	struct Rig
	{
		TestGrid grid;
		CellFieldRegistry cellField;
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		StubProvider terrain, temp, zone;
		FakeDebugDraw draw;
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;
		ViewerCommands commands{ domain };

		static void AddEnum(UploaderSchemaFixture& f, const char* name, const char* label)
		{
			Dia::ValueSchema::ValueInfo t = Dia::ValueSchema::Testing::MakeValueInfo("cells", name);
			t.mType = Dia::ValueSchema::ValueType::Enum;
			Dia::ValueSchema::SetText(t.mLabel, label);
			t.mEnumValues.Reserve(2);
			for (const char* id : { "a", "b" })
			{
				Dia::ValueSchema::EnumValue v;
				Dia::ValueSchema::SetText(v.mId, id);
				t.mEnumValues.Add(v);
			}
			f.Add(t);
		}

		static const Dia::ValueSchema::ValueSchemaService* Build(UploaderSchemaFixture& f)
		{
			AddEnum(f, "terrain", "Terrain");
			f.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
			AddEnum(f, "zone", "Zone");
			return &f.Publish();
		}

		Rig(uint16_t w, uint16_t h)
			: grid(w, h)
			, service(Build(fx))
			, registry(nullptr, &FakePuIdFn)
			, uploader(cellField, nullptr)
			, domain(registry, uploader, service)
		{
			const size_t n = static_cast<size_t>(w) * h;
			terrain.values = std::vector<float>(n, 0.0f);
			temp.values = std::vector<float>(n, 1.0f);
			zone.values = std::vector<float>(n, 1.0f);
			registry.Register(terrain.MakeDesc("terrain", &grid.desc, CellValueKind::Category, "cells/terrain"));
			registry.Register(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
			registry.Register(zone.MakeDesc("zone", &grid.desc, CellValueKind::Category, "cells/zone"));
			domain.Update(draw);
		}

		Json::Value State()
		{
			Json::Value out(Json::objectValue);
			domain.GetJSONState(out);
			return out["selection"];
		}
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

	Json::Value Exec(const char* cmd, const char* params) { return ViewerCommands::Exec(cmd, Parse(params)); }

	struct SelectionPanelStateTest : ::testing::Test
	{
		void SetUp() override { FakePuId() = 1; Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};
}

TEST_F(SelectionPanelStateTest, EmptyStateHasDefaults)
{
	Rig rig(20, 20);
	const Json::Value s = rig.State();
	EXPECT_EQ(s["count"].asUInt(), 0u);
	EXPECT_EQ(s["tool"].asString(), "select");
	EXPECT_EQ(s["group"].asString(), "terrain");
	EXPECT_FALSE(s["tooLarge"].asBool());
	EXPECT_FALSE(s.isMember("tooLargeText"));
}

TEST_F(SelectionPanelStateTest, CountFollowsSelection)
{
	Rig rig(20, 20);
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":2,\"r1\":2}");
	EXPECT_EQ(rig.State()["count"].asUInt(), 9u);
	Exec("cells.select.clear", "{}");
	EXPECT_EQ(rig.State()["count"].asUInt(), 0u);
}

TEST_F(SelectionPanelStateTest, GroupsListCategoryLayersWithDefaultSelected)
{
	Rig rig(20, 20);
	const Json::Value g = rig.State()["groups"];
	ASSERT_EQ(g.size(), 2u);	// the float layer is not a group
	EXPECT_EQ(g[0u]["id"].asString(), "terrain");
	EXPECT_EQ(g[0u]["label"].asString(), "Terrain");
	EXPECT_TRUE(g[0u]["selected"].asBool());
	EXPECT_EQ(g[1u]["id"].asString(), "zone");
	EXPECT_EQ(g[1u]["label"].asString(), "Zone");
	EXPECT_FALSE(g[1u]["selected"].asBool());
}

TEST_F(SelectionPanelStateTest, ToolCommandSetsToolAndState)
{
	Rig rig(20, 20);
	const Json::Value r = Exec("cells.select.tool", "{\"tool\":\"pan\"}");
	EXPECT_TRUE(r["data"]["ok"].asBool());
	EXPECT_EQ(r["data"]["tool"].asString(), "pan");
	EXPECT_EQ(rig.State()["tool"].asString(), "pan");
	EXPECT_TRUE(rig.domain.Input().PanTool());
	Exec("cells.select.tool", "{\"tool\":\"select\"}");
	EXPECT_EQ(rig.State()["tool"].asString(), "select");
}

TEST_F(SelectionPanelStateTest, ToolCommandRejectsUnknownTool)
{
	Rig rig(20, 20);
	const Json::Value r = Exec("cells.select.tool", "{\"tool\":\"lasso\"}");
	EXPECT_EQ(r["error"].asString(), "unknown tool 'lasso'");
	EXPECT_EQ(rig.State()["tool"].asString(), "select");
	EXPECT_TRUE(Exec("cells.select.tool", "{}").isMember("error"));
}

TEST_F(SelectionPanelStateTest, GroupCommandSetsGroupAndState)
{
	Rig rig(20, 20);
	const Json::Value r = Exec("cells.select.group", "{\"layer\":\"zone\"}");
	EXPECT_TRUE(r["data"]["ok"].asBool());
	EXPECT_EQ(r["data"]["group"].asString(), "zone");
	const Json::Value s = rig.State();
	EXPECT_EQ(s["group"].asString(), "zone");
	EXPECT_FALSE(s["groups"][0u]["selected"].asBool());
	EXPECT_TRUE(s["groups"][1u]["selected"].asBool());
	EXPECT_EQ(rig.domain.Input().SelectionGroup(), StringCRC("zone"));
}

TEST_F(SelectionPanelStateTest, GroupCommandRejectsNonCategoryAndUnknown)
{
	Rig rig(20, 20);
	EXPECT_EQ(Exec("cells.select.group", "{\"layer\":\"temp\"}")["error"].asString(), "layer 'temp' is not a category layer of the active grid");
	EXPECT_EQ(Exec("cells.select.group", "{\"layer\":\"nope\"}")["error"].asString(), "layer 'nope' is not a category layer of the active grid");
	EXPECT_TRUE(Exec("cells.select.group", "{}").isMember("error"));
	EXPECT_EQ(rig.State()["group"].asString(), "terrain");
}

TEST_F(SelectionPanelStateTest, PanelCommandsRunTheSameCode)
{
	Rig rig(20, 20);
	Json::Value a(Json::objectValue);
	a["tool"] = "pan";
	rig.domain.OnCommand(StringCRC("select_tool"), a);
	EXPECT_EQ(rig.State()["tool"].asString(), "pan");
	Json::Value g(Json::objectValue);
	g["layer"] = "zone";
	rig.domain.OnCommand(StringCRC("select_group"), g);
	EXPECT_EQ(rig.State()["group"].asString(), "zone");
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	EXPECT_EQ(rig.State()["count"].asUInt(), 1u);
	rig.domain.OnCommand(StringCRC("select_clear"), Json::Value(Json::objectValue));
	EXPECT_EQ(rig.State()["count"].asUInt(), 0u);
}

TEST_F(SelectionPanelStateTest, TooLargeBannerWhenSelectionExceedsOutlineCap)
{
	Rig rig(104, 64);
	// Isolated cells (q%3==0, r%2==0): 35 * 32 = 1120 cells * 6 edges = 6720 > the 4000 edge cap.
	std::string cells = "{\"cells\":[";
	uint32_t n = 0;
	for (int q = 0; q < 104; q += 3)
	{
		for (int r = 0; r < 64; r += 2)
		{
			if (n > 0) { cells += ","; }
			cells += "[" + std::to_string(q) + "," + std::to_string(r) + "]";
			++n;
		}
	}
	cells += "]}";
	ASSERT_TRUE(Exec("cells.select.cells", cells.c_str())["data"]["ok"].asBool());
	rig.domain.Update(rig.draw);
	const Json::Value s = rig.State();
	EXPECT_EQ(s["count"].asUInt(), n);
	EXPECT_TRUE(s["tooLarge"].asBool());
	EXPECT_EQ(s["tooLargeText"].asString(), "Selection too large to outline (" + std::to_string(n) + " cells)");

	Exec("cells.select.clear", "{}");
	rig.domain.Update(rig.draw);
	const Json::Value cleared = rig.State();
	EXPECT_FALSE(cleared["tooLarge"].asBool());
	EXPECT_FALSE(cleared.isMember("tooLargeText"));
}

TEST_F(SelectionPanelStateTest, SmallSelectionIsNotTooLarge)
{
	Rig rig(20, 20);
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":5,\"r1\":5}");
	rig.domain.Update(rig.draw);
	EXPECT_FALSE(rig.State()["tooLarge"].asBool());
}

#endif // DIA_DEBUG
