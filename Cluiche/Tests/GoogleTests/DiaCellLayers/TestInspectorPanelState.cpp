#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/CellDetailTestSupport.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaAPI/DiaAPI.h>

#include <sstream>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

// The Selection Inspector card state: GetJSONState "inspector" (Feature 4 task 9), built from InspectorState's cached stats, plus the
// cells.inspector.expand command and the panel commands select_pin / select_unpin / inspector_expand.
// Rig: 20x20 grid. temp (float 0..200, unit C, group Terrain, base) = index * 0.5; terrain (category grass/water, group Terrain) = index % 2;
// rain, wind (floats, group Weather). Shown set = temp + terrain. Box (0,0)-(2,2): temp 0..21, mean 10.5.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;

namespace
{
	struct Rig
	{
		TestGrid grid;
		CellFieldRegistry cellField;
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		StubProvider temp, terrain, rain, wind;
		StubDetailProvider detail;
		FakeDebugDraw draw;
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;

		static const Dia::ValueSchema::ValueSchemaService* Build(UploaderSchemaFixture& f)
		{
			Dia::ValueSchema::ValueInfo t = Dia::ValueSchema::Testing::MakeValueInfo("cells", "terrain");
			t.mType = Dia::ValueSchema::ValueType::Enum;
			Dia::ValueSchema::SetText(t.mLabel, "Terrain");
			t.mEnumValues.Reserve(2);
			const char* ids[] = { "grass", "water" };
			const char* labels[] = { "Grass", "Water" };
			for (int i = 0; i < 2; ++i)
			{
				Dia::ValueSchema::EnumValue v;
				Dia::ValueSchema::SetText(v.mId, ids[i]);
				Dia::ValueSchema::SetText(v.mLabel, labels[i]);
				t.mEnumValues.Add(v);
			}
			f.Add(t);
			Dia::ValueSchema::ValueInfo temp = UploaderSchemaFixture::FloatEntry("temp", 0, 200);
			Dia::ValueSchema::SetText(temp.mLabel, "Temperature");
			Dia::ValueSchema::SetText(temp.mUnit, "C");
			Dia::ValueSchema::SetText(temp.mDescription, "Air temperature");
			f.Add(temp);
			f.Add(UploaderSchemaFixture::FloatEntry("rain", 0, 100));
			f.Add(UploaderSchemaFixture::FloatEntry("wind", 0, 10));
			return &f.Publish();
		}

		void Add(StubProvider& p, const char* id, const char* group, CellValueKind kind)
		{
			CellLayerDesc d = p.MakeDesc(id, &grid.desc, kind, (std::string("cells/") + id).c_str());
			d.group.Clear();
			d.group.Append(group);
			ASSERT_EQ(CellRegisterResult::Ok, registry.Register(d));
		}

		Rig(uint16_t w = 20, uint16_t h = 20)
			: grid(w, h)
			, service(Build(fx))
			, registry(nullptr, &FakePuIdFn)
			, uploader(cellField, nullptr)
			, domain(registry, uploader, service)
		{
			const size_t n = static_cast<size_t>(w) * h;
			temp.values.resize(n); terrain.values.resize(n); wind.values.resize(n);
			rain.values = std::vector<float>(n, 10.0f);
			for (size_t i = 0; i < n; ++i)
			{
				temp.values[i] = static_cast<float>(i) * 0.5f;
				terrain.values[i] = static_cast<float>(i % 2);
				wind.values[i] = static_cast<float>(i % 4);
			}
			Add(temp, "temp", "Terrain", CellValueKind::Float);
			Add(terrain, "terrain", "Terrain", CellValueKind::Category);
			Add(rain, "rain", "Weather", CellValueKind::Float);
			Add(wind, "wind", "Weather", CellValueKind::Float);
			domain.RegisterCommands();
			domain.Update(draw);
		}

		void Settle() { for (int i = 0; i < 4; ++i) { domain.Update(draw); } }
		uint32_t Reads() const { return temp.readCalls + terrain.readCalls + rain.readCalls + wind.readCalls; }

		Json::Value Card()
		{
			Json::Value out(Json::objectValue);
			domain.GetJSONState(out);
			return out["inspector"];
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

	Json::Value Exec(const char* cmd, const char* params) { return Dia::API::ExecuteCommandJson(StringCRC(cmd), Parse(params)); }

	const Json::Value* FindLayer(const Json::Value& card, const char* layer)
	{
		for (const Json::Value& l : card["layers"]) { if (l["id"].asString() == layer) { return &l; } }
		return nullptr;
	}

	bool Contains(const Json::Value& arr, const char* text)
	{
		for (const Json::Value& v : arr) { if (v.asString() == text) { return true; } }
		return false;
	}

	void SelectBox(const char* box = "{\"q0\":0,\"r0\":0,\"q1\":2,\"r1\":2}")
	{
		ASSERT_TRUE(Exec("cells.select.box", box)["success"].asBool());
	}

	struct InspectorPanelStateTest : ::testing::Test
	{
		void SetUp() override { FakePuId() = 1; Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};
}

TEST_F(InspectorPanelStateTest, NoSelectionComputesNothing)
{
	Rig rig;
	rig.Settle();
	const uint32_t reads = rig.Reads();
	const Json::Value c = rig.Card();
	ASSERT_TRUE(c.isObject());
	EXPECT_FALSE(c["hasSelection"].asBool());
	EXPECT_EQ(0u, c["layers"].size());
	EXPECT_FALSE(c.isMember("cell"));
	EXPECT_FALSE(c.isMember("pinned"));
	EXPECT_TRUE(Contains(c["expandedGroups"], "Terrain"));
	EXPECT_EQ(reads, rig.Reads());
}

TEST_F(InspectorPanelStateTest, LayersComeFromTheCachedStats)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	const Json::Value c = rig.Card();
	EXPECT_TRUE(c["hasSelection"].asBool());
	EXPECT_FALSE(c["updating"].asBool());
	EXPECT_EQ("cells/temp", c["base"].asString());
	ASSERT_EQ(2u, c["layers"].size());

	const Json::Value* t = FindLayer(c, "temp");
	ASSERT_NE(nullptr, t);
	EXPECT_EQ("cells/temp", (*t)["schemaKey"].asString());
	EXPECT_EQ("Temperature", (*t)["label"].asString());
	EXPECT_EQ("C", (*t)["unit"].asString());
	EXPECT_EQ("float", (*t)["kind"].asString());
	EXPECT_EQ("Terrain", (*t)["group"].asString());
	EXPECT_EQ("Air temperature", (*t)["tooltip"].asString());
	EXPECT_EQ(9u, (*t)["count"].asUInt());
	EXPECT_EQ(0u, (*t)["nanCount"].asUInt());
	EXPECT_DOUBLE_EQ(0.0, (*t)["min"].asDouble());
	EXPECT_DOUBLE_EQ(10.5, (*t)["mean"].asDouble());
	EXPECT_DOUBLE_EQ(21.0, (*t)["max"].asDouble());
	EXPECT_DOUBLE_EQ(0.0, (*t)["rangeMin"].asDouble());
	EXPECT_DOUBLE_EQ(200.0, (*t)["rangeMax"].asDouble());
	EXPECT_FALSE(t->isMember("a"));
	EXPECT_FALSE(t->isMember("delta"));

	const Json::Value* g = FindLayer(c, "terrain");
	ASSERT_NE(nullptr, g);
	EXPECT_EQ("category", (*g)["kind"].asString());
	EXPECT_TRUE((*g)["min"].isNull());
	ASSERT_EQ(2u, (*g)["categories"].size());
	EXPECT_EQ("grass", (*g)["categories"][0u]["id"].asString());
	EXPECT_EQ("Grass", (*g)["categories"][0u]["label"].asString());
	EXPECT_EQ(6u, (*g)["categories"][0u]["count"].asUInt());	// even indices {0,2,20,22,40,42}
	EXPECT_EQ(3u, (*g)["categories"][1u]["count"].asUInt());
	EXPECT_EQ(0u, (*g)["other"].asUInt());
}

TEST_F(InspectorPanelStateTest, RenderingTheStateDoesNotRecomputeOrRead)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	const uint32_t passes = rig.domain.Inspector().PassCount();
	const uint32_t reads = rig.Reads();
	for (int i = 0; i < 3; ++i) { rig.Card(); }
	EXPECT_EQ(passes, rig.domain.Inspector().PassCount());
	EXPECT_EQ(reads, rig.Reads());
}

TEST_F(InspectorPanelStateTest, UpdatingMarkIsSetForTheFrameAPassIsPending)
{
	Rig rig;
	rig.Settle();
	SelectBox();
	rig.domain.Update(rig.draw);	// detects the change
	EXPECT_TRUE(rig.Card()["updating"].asBool());
	rig.domain.Update(rig.draw);	// runs the pass
	EXPECT_FALSE(rig.Card()["updating"].asBool());
}

TEST_F(InspectorPanelStateTest, FloatBaseHistogram)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	const Json::Value h = rig.Card()["histogram"];
	ASSERT_TRUE(h.isObject());
	ASSERT_EQ(16u, h["bins"].size());
	EXPECT_EQ(9u, h["count"].asUInt());
	EXPECT_TRUE(h["valid"].asBool());
	EXPECT_DOUBLE_EQ(0.0, h["minRange"].asDouble());
	EXPECT_DOUBLE_EQ(200.0, h["maxRange"].asDouble());
	EXPECT_EQ(0u, h["outOfRangeLow"].asUInt());
	EXPECT_EQ(0u, h["outOfRangeHigh"].asUInt());
}

TEST_F(InspectorPanelStateTest, CategoryBaseHistogramIsTheCategoryCounts)
{
	Rig rig;
	ASSERT_TRUE(rig.domain.SetBase(StringCRC("terrain")));
	SelectBox();
	rig.Settle();
	const Json::Value h = rig.Card()["histogram"];
	ASSERT_TRUE(h.isObject());
	ASSERT_EQ(2u, h["categories"].size());
	EXPECT_EQ(6u, h["categories"][0u]["count"].asUInt());
	EXPECT_TRUE(h.isMember("other"));
	EXPECT_FALSE(h.isMember("bins"));
}

TEST_F(InspectorPanelStateTest, PinnedCardShowsAAndDelta)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	ASSERT_TRUE(Exec("cells.select.pin", "{}")["success"].asBool());
	SelectBox("{\"q0\":5,\"r0\":0,\"q1\":6,\"r1\":0}");	// indices 5,6: temp 2.5, 3.0
	rig.Settle();
	const Json::Value c = rig.Card();
	EXPECT_EQ(9u, c["pinned"]["count"].asUInt());
	EXPECT_FALSE(c["pinned"]["frozen"].asBool());
	const Json::Value* t = FindLayer(c, "temp");
	ASSERT_NE(nullptr, t);
	EXPECT_EQ(2u, (*t)["count"].asUInt());
	EXPECT_EQ(9u, (*t)["a"]["count"].asUInt());
	EXPECT_DOUBLE_EQ(10.5, (*t)["a"]["mean"].asDouble());
	EXPECT_EQ(-7, (*t)["delta"]["count"].asInt());
	EXPECT_DOUBLE_EQ(2.75 - 10.5, (*t)["delta"]["mean"].asDouble());
	EXPECT_DOUBLE_EQ(2.5 - 0.0, (*t)["delta"]["min"].asDouble());
	EXPECT_DOUBLE_EQ(3.0 - 21.0, (*t)["delta"]["max"].asDouble());
}

TEST_F(InspectorPanelStateTest, FrozenCardMarksFrozenAndKeepsAUnchanged)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	ASSERT_TRUE(Exec("cells.select.pin", "{\"freeze\":true}")["success"].asBool());
	SelectBox("{\"q0\":5,\"r0\":0,\"q1\":6,\"r1\":0}");
	rig.Settle();
	const Json::Value c = rig.Card();
	EXPECT_TRUE(c["pinned"]["frozen"].asBool());
	const Json::Value* t = FindLayer(c, "temp");
	ASSERT_NE(nullptr, t);
	EXPECT_EQ(9u, (*t)["a"]["count"].asUInt());
	EXPECT_FALSE((*t)["a"].isMember("notFrozen"));
}

TEST_F(InspectorPanelStateTest, SingleCellBlockOnlyForExactlyOneCell)
{
	Rig rig;
	rig.detail.sectionTitle = "Ecology";
	rig.detail.valueKey = StringCRC("cells/temp");
	rig.detail.valueToEmit = 0.25f;
	rig.detail.rowsToEmit = 1;
	rig.registry.Details().Register(&rig.grid.desc, "Eco", rig.detail.Delegate());
	SelectBox();
	rig.Settle();
	EXPECT_FALSE(rig.Card().isMember("cell"));
	ASSERT_TRUE(Exec("cells.select.cell", "{\"q\":3,\"r\":2}")["success"].asBool());	// index 43
	rig.Settle();
	const Json::Value c = rig.Card();
	ASSERT_TRUE(c.isMember("cell"));
	EXPECT_EQ(43u, c["cell"]["index"].asUInt());
	EXPECT_EQ(3u, c["cell"]["q"].asUInt());
	EXPECT_EQ(2u, c["cell"]["r"].asUInt());
	ASSERT_EQ(2u, c["cell"]["rows"].size());
	EXPECT_EQ("temp", c["cell"]["rows"][0u]["layer"].asString());
	EXPECT_DOUBLE_EQ(21.5, c["cell"]["rows"][0u]["value"].asDouble());
	EXPECT_EQ("section", c["cell"]["details"][0u]["kind"].asString());
	EXPECT_GE(c["cell"]["details"].size(), 3u);
}

// ---------------------------------------------------------------- cells.inspector.expand

TEST_F(InspectorPanelStateTest, ExpandCommandShowsAndHidesAGroupsLayers)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	EXPECT_EQ(nullptr, FindLayer(rig.Card(), "rain"));
	const Json::Value r = Exec("cells.inspector.expand", "{\"group\":\"Weather\",\"expanded\":true}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_TRUE(r["data"]["ok"].asBool());
	rig.Settle();
	Json::Value c = rig.Card();
	EXPECT_TRUE(Contains(c["expandedGroups"], "Weather"));
	ASSERT_NE(nullptr, FindLayer(c, "rain"));
	ASSERT_NE(nullptr, FindLayer(c, "wind"));
	EXPECT_EQ("Weather", (*FindLayer(c, "wind"))["group"].asString());
	EXPECT_EQ(9u, (*FindLayer(c, "wind"))["count"].asUInt());

	ASSERT_TRUE(Exec("cells.inspector.expand", "{\"group\":\"Weather\",\"expanded\":false}")["success"].asBool());
	rig.Settle();
	c = rig.Card();
	EXPECT_FALSE(Contains(c["expandedGroups"], "Weather"));
	EXPECT_EQ(nullptr, FindLayer(c, "rain"));
}

TEST_F(InspectorPanelStateTest, ExpandCommandRejectsBadArguments)
{
	Rig rig;
	EXPECT_TRUE(Exec("cells.inspector.expand", "{\"expanded\":true}").isMember("error"));
	EXPECT_TRUE(Exec("cells.inspector.expand", "{\"group\":5,\"expanded\":true}").isMember("error"));
	EXPECT_TRUE(Exec("cells.inspector.expand", "{\"group\":\"Weather\"}").isMember("error"));
	EXPECT_TRUE(Exec("cells.inspector.expand", "{\"group\":\"Weather\",\"expanded\":\"yes\"}").isMember("error"));
	EXPECT_TRUE(Exec("cells.inspector.expand", "{\"group\":\"Nope\",\"expanded\":true}").isMember("error"));
	EXPECT_FALSE(rig.domain.Inspector().IsGroupExpanded("Weather"));
	EXPECT_FALSE(rig.domain.Inspector().IsGroupExpanded("Nope"));
}

TEST_F(InspectorPanelStateTest, ExpandCommandIsUnregisteredWithTheDomain)
{
	{
		Rig rig;
		EXPECT_TRUE(Exec("cells.inspector.expand", "{\"group\":\"Weather\",\"expanded\":true}")["success"].asBool());
		rig.domain.UnregisterCommands();
	}
	EXPECT_FALSE(Exec("cells.inspector.expand", "{\"group\":\"Weather\",\"expanded\":true}")["success"].asBool());
}

// ---------------------------------------------------------------- panel commands

TEST_F(InspectorPanelStateTest, PanelCommandsRunTheSameCodeAsTheDiaApiCommands)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	rig.domain.OnCommand(StringCRC("select_pin"), Parse("{}"));
	EXPECT_TRUE(rig.domain.Inspector().IsPinned());
	EXPECT_FALSE(rig.domain.Inspector().IsFrozen());
	rig.domain.OnCommand(StringCRC("select_pin"), Parse("{\"freeze\":true}"));
	EXPECT_TRUE(rig.domain.Inspector().IsFrozen());
	rig.domain.OnCommand(StringCRC("select_unpin"), Parse("{}"));
	EXPECT_FALSE(rig.domain.Inspector().IsPinned());

	rig.domain.OnCommand(StringCRC("inspector_expand"), Parse("{\"group\":\"Weather\",\"expanded\":true}"));
	EXPECT_TRUE(rig.domain.Inspector().IsGroupExpanded("Weather"));
	rig.domain.OnCommand(StringCRC("inspector_expand"), Parse("{\"group\":\"Weather\",\"expanded\":false}"));
	EXPECT_FALSE(rig.domain.Inspector().IsGroupExpanded("Weather"));
}

#endif // DIA_DEBUG
