#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaAPI/DiaAPI.h>

#ifdef DIA_DEBUG

// JSON forms of the viewer commands, reachable over the E2E / DebugServer channel (DiaAPI JSON registry):
// cells.layer.base_by_id {"id"} and cells.overlay.toggle_by_id {"id"}. The CLI forms keep working alongside.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;

namespace
{
	struct JsonCmdTest : ::testing::Test
	{
		TestGrid gridA{ 4, 3 };
		TestGrid gridB{ 5, 2 };
		CellFieldRegistry cellField;
		StubProvider temp, rain, soil;
		FakeDebugDraw draw;

		void SetUp() override
		{
			FakePuId() = 1;
			temp.values = std::vector<float>(12, 1.0f);
			rain.values = std::vector<float>(12, 2.0f);
			soil.values = std::vector<float>(10, 3.0f);
			Dia::API::Initialize();
		}
		void TearDown() override { Dia::API::Shutdown(); }

		static Json::Value Id(const char* id) { Json::Value p(Json::objectValue); p["id"] = id; return p; }
		static Json::Value Run(const char* cmd, const Json::Value& p) { return Dia::API::ExecuteCommandJson(StringCRC(cmd), p); }
	};

	struct Rig
	{
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;
		Rig(CellFieldRegistry& cf) : registry(nullptr, &FakePuIdFn), uploader(cf, nullptr), domain(registry, uploader, nullptr) {}
		void Add(const CellLayerDesc& d) { ASSERT_EQ(registry.Register(d), CellRegisterResult::Ok); }
	};
}

TEST_F(JsonCmdTest, BaseByIdSwitchesBaseAndGrid)
{
	Rig rig(cellField);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	rig.domain.RegisterCommands();
	rig.domain.Update(draw);

	Json::Value r = Run("cells.layer.base_by_id", Id("rain"));
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["data"]["id"].asString(), "rain");
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("rain"));

	r = Run("cells.layer.base_by_id", Id("soil"));
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["data"]["grid_index"].asUInt(), 1u);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("soil"));
	EXPECT_EQ(rig.domain.ActiveGrid(), &gridB.desc);
}

TEST_F(JsonCmdTest, BaseByIdUnknownIdErrorsAndChangesNothing)
{
	Rig rig(cellField);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	rig.domain.RegisterCommands();
	rig.domain.Update(draw);
	const Json::Value r = Run("cells.layer.base_by_id", Id("nope"));
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_NE(r["error"].asString().find("nope"), std::string::npos);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("temp"));
}

TEST_F(JsonCmdTest, BaseByIdMissingOrBadParamErrorsAndChangesNothing)
{
	Rig rig(cellField);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	rig.domain.RegisterCommands();
	rig.domain.Update(draw);
	Json::Value num(Json::objectValue);
	num["id"] = 5;
	EXPECT_FALSE(Run("cells.layer.base_by_id", Json::Value(Json::objectValue))["success"].asBool());
	EXPECT_FALSE(Run("cells.layer.base_by_id", Json::Value())["success"].asBool());
	EXPECT_FALSE(Run("cells.layer.base_by_id", num)["success"].asBool());
	EXPECT_FALSE(Run("cells.layer.base_by_id", Id(""))["success"].asBool());
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("temp"));
}

TEST_F(JsonCmdTest, OverlayToggleByIdTogglesAndReportsState)
{
	Rig rig(cellField);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	ASSERT_EQ(rig.registry.RegisterOverlay(MakeOverlay("ov")), CellRegisterResult::Ok);
	rig.domain.RegisterCommands();
	rig.domain.Update(draw);

	Json::Value r = Run("cells.overlay.toggle_by_id", Id("ov"));
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_TRUE(r["data"]["enabled"].asBool());
	EXPECT_TRUE(rig.domain.IsOverlayEnabled(StringCRC("ov")));
	r = Run("cells.overlay.toggle_by_id", Id("ov"));
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_FALSE(r["data"]["enabled"].asBool());
	EXPECT_FALSE(rig.domain.IsOverlayEnabled(StringCRC("ov")));
}

TEST_F(JsonCmdTest, OverlayToggleByIdUnknownOrMissingErrorsAndChangesNothing)
{
	Rig rig(cellField);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	ASSERT_EQ(rig.registry.RegisterOverlay(MakeOverlay("ov")), CellRegisterResult::Ok);
	rig.domain.RegisterCommands();
	rig.domain.Update(draw);
	EXPECT_FALSE(Run("cells.overlay.toggle_by_id", Id("nope"))["success"].asBool());
	EXPECT_FALSE(Run("cells.overlay.toggle_by_id", Json::Value(Json::objectValue))["success"].asBool());
	EXPECT_FALSE(rig.domain.IsOverlayEnabled(StringCRC("ov")));
}

namespace
{
	// Returns the id of the layer flagged selected across all grids ("" if none); counts selected flags in `count`.
	std::string SelectedLayer(const Json::Value& data, uint32_t& count)
	{
		std::string id;
		count = 0;
		for (const Json::Value& g : data["grids"])
			for (const Json::Value& l : g["layers"])
			{
				if (l["selected"].asBool()) { id = l["id"].asString(); ++count; }
			}
		return id;
	}
}

TEST_F(JsonCmdTest, LayerListReportsActiveBaseDefaultSwitchAndFailedSwitch)
{
	Rig rig(cellField);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	rig.domain.RegisterCommands();
	rig.domain.Update(draw);

	Json::Value r = Run("cells.layer.list", Json::Value(Json::objectValue));
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["data"]["activeBase"].asString(), "temp");
	EXPECT_EQ(r["data"]["activeGridIndex"].asInt(), 0);
	uint32_t n = 0;
	EXPECT_EQ(SelectedLayer(r["data"], n), "temp");
	EXPECT_EQ(n, 1u);
	EXPECT_TRUE(r["data"]["grids"][0]["layers"][0].isMember("selected"));
	EXPECT_FALSE(r["data"]["grids"][0]["layers"][1]["selected"].asBool());
	EXPECT_FALSE(r["data"]["grids"][1]["layers"][0]["selected"].asBool());

	ASSERT_TRUE(Run("cells.layer.base_by_id", Id("soil"))["success"].asBool());
	r = Run("cells.layer.list", Json::Value(Json::objectValue));
	EXPECT_EQ(r["data"]["activeBase"].asString(), "soil");
	EXPECT_EQ(r["data"]["activeGridIndex"].asInt(), 1);
	EXPECT_EQ(SelectedLayer(r["data"], n), "soil");
	EXPECT_EQ(n, 1u);

	EXPECT_FALSE(Run("cells.layer.base_by_id", Id("nope"))["success"].asBool());
	r = Run("cells.layer.list", Json::Value(Json::objectValue));
	EXPECT_EQ(r["data"]["activeBase"].asString(), "soil");
	EXPECT_EQ(r["data"]["activeGridIndex"].asInt(), 1);
	EXPECT_EQ(SelectedLayer(r["data"], n), "soil");
	EXPECT_EQ(n, 1u);
}

TEST_F(JsonCmdTest, LayerListEmptyStateHasEmptyActiveBase)
{
	Rig rig(cellField);
	rig.domain.RegisterCommands();
	rig.domain.Update(draw);
	const Json::Value r = Run("cells.layer.list", Json::Value(Json::objectValue));
	ASSERT_TRUE(r["success"].asBool());
	ASSERT_TRUE(r["data"].isMember("activeBase"));
	EXPECT_EQ(r["data"]["activeBase"].asString(), "");
	EXPECT_EQ(r["data"]["activeGridIndex"].asInt(), -1);
	uint32_t n = 0;
	SelectedLayer(r["data"], n);
	EXPECT_EQ(n, 0u);
}

TEST_F(JsonCmdTest, CliFormsStillWorkAndJsonCommandsUnregister)
{
	Rig rig(cellField);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	rig.domain.RegisterCommands();
	rig.domain.Update(draw);
	Dia::API::CommandArgs args;
	args.positionalArgs.Add("rain");
	EXPECT_EQ(Dia::API::ExecuteCommand(StringCRC("cells.layer.base"), args), 0);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("rain"));

	rig.domain.UnregisterCommands();
	EXPECT_FALSE(Run("cells.layer.base_by_id", Id("temp"))["success"].asBool());
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("rain"));
}

#endif // DIA_DEBUG
