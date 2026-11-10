#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/ViewerCommands.h>
#include <DiaCellLayers/InspectorModel.h>
#include <DiaCellLayers/Testing/CellDetailTestSupport.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaAPI/DiaAPI.h>

#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

// cells.inspect and InspectorModel::BuildStatsJson (Selection Inspector, Feature 4 task 7).
// Rig: 20x20 grid. temp (float, group Terrain, base) = index * 0.5; terrain (category grass/water, group Terrain) = index % 2;
// rain (float, group Weather) = 10; wind (float, group Weather) = index % 4. Shown set = temp + terrain.
// The box (0,0)-(2,2) selects indices {0,1,2,20,21,22,40,41,42}: temp 0,.5,1,10,10.5,11,20,20.5,21 (sum 94.5, mean 10.5).

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
		StubProvider temp, terrain, rain, wind, late;
		StubDetailProvider detail;
		FakeDebugDraw draw;
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;
		ViewerCommands commands{ domain };

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
			f.Add(temp);
			f.Add(UploaderSchemaFixture::FloatEntry("rain", 0, 100));
			f.Add(UploaderSchemaFixture::FloatEntry("wind", 0, 10));
			f.Add(UploaderSchemaFixture::FloatEntry("late", 0, 10));
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
			late.values = std::vector<float>(n, 3.0f);
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
			domain.Update(draw);
		}

		void Settle() { for (int i = 0; i < 4; ++i) { domain.Update(draw); } }
		uint32_t Reads() const { return temp.readCalls + terrain.readCalls + rain.readCalls + wind.readCalls + late.readCalls; }
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

	const Json::Value* FindLayer(const Json::Value& data, const char* layer)
	{
		for (const Json::Value& l : data["layers"]) { if (l["layer"].asString() == layer) { return &l; } }
		return nullptr;
	}

	void SelectBox(const char* box = "{\"q0\":0,\"r0\":0,\"q1\":2,\"r1\":2}")
	{
		ASSERT_TRUE(Exec("cells.select.box", box)["success"].asBool());
	}

	struct SelectionInspectCommandTest : ::testing::Test
	{
		void SetUp() override { FakePuId() = 1; Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};

	// Model-level fixture: no domain, no card.
	struct SelectionInspectJsonModelTest : ::testing::Test
	{
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		Dia::Graphics::CellGridDesc grid = MakeGrid(4, 3);
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		CountingLayerProvider pTemp, pBiome;
		CellSelection sel;
		InspectorState state;
		LayerReader* reader = nullptr;

		void SetUp() override
		{
			FakePuId() = 1;
			fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
			fx.Add(UploaderSchemaFixture::EnumEntry("biome", 3));
			service = &fx.Publish();
			pTemp.values = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
			pBiome.values = { 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2 };
			CellLayerDesc t = MakeCountingLayer("temp", &grid, pTemp);
			t.schemaKey = StringCRC("cells/temp");
			CellLayerDesc b = MakeCountingLayer("biome", &grid, pBiome, CellValueKind::Category);
			b.schemaKey = StringCRC("cells/biome");
			registry.Register(t);
			registry.Register(b);
			sel.Reset(&grid);
			state.SetActive(registry, &grid, StringCRC("temp"));
			reader = new LayerReader(registry);
		}
		void TearDown() override { delete reader; }
		bool Build(const InspectRequest& req, Json::Value& out, const ICellSelectionView* s = nullptr)
		{
			return InspectorModel::BuildStatsJson(registry, *reader, service, state, s != nullptr ? s : &sel, req, out);
		}
	};
}

// ---------------------------------------------------------------- command: forms

TEST_F(SelectionInspectCommandTest, DefaultFormReturnsShownLayersOnly)
{
	Rig rig;
	SelectBox();
	const Json::Value r = Exec("cells.inspect", "{}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	const Json::Value& d = r["data"];
	EXPECT_TRUE(d["ok"].asBool());
	EXPECT_EQ(9u, d["selected"].asUInt());
	ASSERT_EQ(2u, d["layers"].size());
	EXPECT_EQ("temp", d["layers"][0u]["layer"].asString());	// base first
	EXPECT_EQ("terrain", d["layers"][1u]["layer"].asString());
	EXPECT_EQ(nullptr, FindLayer(d, "rain"));
	EXPECT_EQ("cells/temp", d["base"].asString());
	EXPECT_EQ(20u, d["grid"]["width"].asUInt());
	EXPECT_EQ(20u, d["grid"]["height"].asUInt());
	EXPECT_TRUE(d.isMember("updating"));
}

TEST_F(SelectionInspectCommandTest, NamedGroupReturnsThatGroupsLayers)
{
	Rig rig;
	SelectBox();
	const Json::Value r = Exec("cells.inspect", "{\"group\":\"Weather\"}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	const Json::Value& d = r["data"];
	ASSERT_EQ(2u, d["layers"].size());
	EXPECT_NE(nullptr, FindLayer(d, "rain"));
	EXPECT_NE(nullptr, FindLayer(d, "wind"));
	EXPECT_EQ(nullptr, FindLayer(d, "temp"));
	EXPECT_EQ(10.0, FindLayer(d, "rain")->operator[]("mean").asDouble());
	EXPECT_EQ(9u, (*FindLayer(d, "rain"))["count"].asUInt());
}

TEST_F(SelectionInspectCommandTest, LayerListInRequestOrder)
{
	Rig rig;
	SelectBox();
	const Json::Value r = Exec("cells.inspect", "{\"layers\":[\"wind\",\"temp\"]}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	const Json::Value& d = r["data"];
	ASSERT_EQ(2u, d["layers"].size());
	EXPECT_EQ("wind", d["layers"][0u]["layer"].asString());
	EXPECT_EQ("temp", d["layers"][1u]["layer"].asString());
	// wind = index % 4 over {0,1,2,20,21,22,40,41,42} = 0,1,2,0,1,2,0,1,2
	const Json::Value& w = d["layers"][0u];
	EXPECT_EQ(0.0, w["min"].asDouble());
	EXPECT_EQ(2.0, w["max"].asDouble());
	EXPECT_EQ(1.0, w["mean"].asDouble());
}

TEST_F(SelectionInspectCommandTest, SingleLayerIdForm)
{
	Rig rig;
	SelectBox();
	const Json::Value r = Exec("cells.inspect", "{\"layerId\":\"rain\"}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	ASSERT_EQ(1u, r["data"]["layers"].size());
	EXPECT_EQ("rain", r["data"]["layers"][0u]["layer"].asString());
}

TEST_F(SelectionInspectCommandTest, AllIncludesCollapsedGroupsAndOtherGroups)
{
	Rig rig;
	SelectBox();
	const Json::Value r = Exec("cells.inspect", "{\"all\":true}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_EQ(4u, r["data"]["layers"].size());
	for (const char* id : { "temp", "terrain", "rain", "wind" }) { EXPECT_NE(nullptr, FindLayer(r["data"], id)) << id; }
}

TEST_F(SelectionInspectCommandTest, UnknownLayerIsAnErrorNamingIt)
{
	Rig rig;
	SelectBox();
	const Json::Value r = Exec("cells.inspect", "{\"layers\":[\"temp\",\"nope\"]}");
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_NE(r["error"].asString().find("nope"), std::string::npos);
}

TEST_F(SelectionInspectCommandTest, UnknownGroupIsAnError)
{
	Rig rig;
	SelectBox();
	const Json::Value r = Exec("cells.inspect", "{\"group\":\"Nope\"}");
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_NE(r["error"].asString().find("Nope"), std::string::npos);
}

TEST_F(SelectionInspectCommandTest, NoActiveGridIsAnError)
{
	CellFieldRegistry cf;
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	LayerUploader uploader(cf, nullptr);
	CellLayerViewerDomain domain(registry, uploader, nullptr);
	FakeDebugDraw draw;
	ViewerCommands commands(domain);
	domain.Update(draw);
	const Json::Value r = Exec("cells.inspect", "{}");
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_NE(r["error"].asString().find("no active grid"), std::string::npos);
}

// ---------------------------------------------------------------- command: values

TEST_F(SelectionInspectCommandTest, FloatStatsMatchHandComputedValues)
{
	Rig rig;
	SelectBox();
	const Json::Value r = Exec("cells.inspect", "{}");
	ASSERT_TRUE(r["success"].asBool());
	const Json::Value& t = r["data"]["layers"][0u];
	EXPECT_EQ("cells/temp", t["schemaKey"].asString());
	EXPECT_EQ("float", t["kind"].asString());
	EXPECT_EQ("Temperature", t["label"].asString());
	EXPECT_EQ("C", t["unit"].asString());
	EXPECT_TRUE(t["base"].asBool());
	EXPECT_EQ(9u, t["count"].asUInt());
	EXPECT_EQ(0u, t["nanCount"].asUInt());
	EXPECT_EQ(0.0, t["min"].asDouble());
	EXPECT_EQ(21.0, t["max"].asDouble());
	EXPECT_DOUBLE_EQ(10.5, t["mean"].asDouble());
}

TEST_F(SelectionInspectCommandTest, CategoryBlockHasCountsLabelsAndOther)
{
	Rig rig;
	SelectBox();
	const Json::Value r = Exec("cells.inspect", "{}");
	ASSERT_TRUE(r["success"].asBool());
	const Json::Value& c = r["data"]["layers"][1u];
	EXPECT_EQ("category", c["kind"].asString());
	EXPECT_FALSE(c.isMember("mean"));
	ASSERT_EQ(2u, c["categories"].size());
	EXPECT_EQ("grass", c["categories"][0u]["id"].asString());
	EXPECT_EQ("Grass", c["categories"][0u]["label"].asString());
	EXPECT_EQ(6u, c["categories"][0u]["count"].asUInt());
	EXPECT_EQ("water", c["categories"][1u]["id"].asString());
	EXPECT_EQ(3u, c["categories"][1u]["count"].asUInt());
	EXPECT_EQ(0u, c["other"].asUInt());
	EXPECT_EQ(0u, c["nanCount"].asUInt());
}

TEST_F(SelectionInspectCommandTest, HistogramBlockForFloatBase)
{
	Rig rig;
	SelectBox();
	const Json::Value r = Exec("cells.inspect", "{}");
	ASSERT_TRUE(r["success"].asBool());
	const Json::Value& h = r["data"]["histogram"];
	ASSERT_TRUE(h.isObject());
	EXPECT_TRUE(h["valid"].asBool());
	EXPECT_EQ(0.0, h["minRange"].asDouble());
	EXPECT_EQ(200.0, h["maxRange"].asDouble());
	ASSERT_EQ(16u, h["bins"].size());
	EXPECT_EQ(6u, h["bins"][0u].asUInt());	// bin width 12.5: 0,.5,1,10,10.5,11
	EXPECT_EQ(3u, h["bins"][1u].asUInt());	// 20,20.5,21
	EXPECT_EQ(0u, h["outOfRangeLow"].asUInt());
	EXPECT_EQ(0u, h["outOfRangeHigh"].asUInt());
}

TEST_F(SelectionInspectCommandTest, EmptySelectionSaysSoAndReadsNothing)
{
	Rig rig;
	rig.Settle();
	const uint32_t before = rig.Reads();
	const Json::Value r = Exec("cells.inspect", "{\"all\":true}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_TRUE(r["data"]["ok"].asBool());
	EXPECT_EQ(0u, r["data"]["selected"].asUInt());
	EXPECT_TRUE(r["data"]["layers"].isArray());
	EXPECT_EQ(0u, r["data"]["layers"].size());
	EXPECT_FALSE(r["data"].isMember("histogram"));
	EXPECT_FALSE(r["data"].isMember("cell"));
	EXPECT_EQ(before, rig.Reads());
}

TEST_F(SelectionInspectCommandTest, ResultIsFreshWithoutAnIntermediateUpdate)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	Exec("cells.select.cell", "{\"q\":3,\"r\":0}");	// no Update afterwards
	const Json::Value r = Exec("cells.inspect", "{}");
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(1u, r["data"]["selected"].asUInt());
	EXPECT_EQ(1.5, r["data"]["layers"][0u]["mean"].asDouble());
}

// ---------------------------------------------------------------- command: pinned

TEST_F(SelectionInspectCommandTest, LivePinnedBlockHasAAndDelta)
{
	Rig rig;
	SelectBox();
	ASSERT_TRUE(Exec("cells.select.pin", "{}")["success"].asBool());
	Exec("cells.select.cell", "{\"q\":3,\"r\":0}");	// idx 3, temp 1.5
	rig.Settle();
	const Json::Value r = Exec("cells.inspect", "{}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	const Json::Value& d = r["data"];
	EXPECT_EQ(9u, d["pinned"]["count"].asUInt());
	EXPECT_FALSE(d["pinned"]["frozen"].asBool());
	const Json::Value& t = d["layers"][0u];
	EXPECT_EQ(1u, t["count"].asUInt());
	EXPECT_EQ(9u, t["a"]["count"].asUInt());
	EXPECT_DOUBLE_EQ(10.5, t["a"]["mean"].asDouble());
	EXPECT_EQ(-8, t["delta"]["count"].asInt());
	EXPECT_DOUBLE_EQ(1.5, t["delta"]["min"].asDouble());	// 1.5 - 0
	EXPECT_DOUBLE_EQ(-9.0, t["delta"]["mean"].asDouble());	// 1.5 - 10.5
	EXPECT_DOUBLE_EQ(-19.5, t["delta"]["max"].asDouble());	// 1.5 - 21
}

TEST_F(SelectionInspectCommandTest, LiveANotComputedOutsideTheShownSet)
{
	Rig rig;
	SelectBox();
	Exec("cells.select.pin", "{}");
	rig.Settle();
	const Json::Value r = Exec("cells.inspect", "{\"layers\":[\"rain\"]}");
	ASSERT_TRUE(r["success"].asBool());
	const Json::Value& a = r["data"]["layers"][0u]["a"];
	EXPECT_TRUE(a["notComputed"].asBool());
	EXPECT_FALSE(r["data"]["layers"][0u].isMember("delta"));
}

TEST_F(SelectionInspectCommandTest, FrozenPinnedHasAForAnyLayerAndNotFrozenForLateOnes)
{
	Rig rig;
	SelectBox();
	ASSERT_TRUE(Exec("cells.select.pin", "{\"freeze\":true}")["success"].asBool());
	rig.Add(rig.late, "late", "Weather", CellValueKind::Float);	// registered after the freeze
	Exec("cells.select.cell", "{\"q\":3,\"r\":0}");
	rig.Settle();
	const Json::Value r = Exec("cells.inspect", "{\"layers\":[\"rain\",\"late\"]}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	const Json::Value& d = r["data"];
	EXPECT_TRUE(d["pinned"]["frozen"].asBool());
	const Json::Value& rain = d["layers"][0u];
	EXPECT_EQ(9u, rain["a"]["count"].asUInt());
	EXPECT_EQ(-8, rain["delta"]["count"].asInt());
	EXPECT_DOUBLE_EQ(0.0, rain["delta"]["mean"].asDouble());
	const Json::Value& late = d["layers"][1u];
	EXPECT_TRUE(late["a"]["notFrozen"].asBool());
	EXPECT_FALSE(late.isMember("delta"));
	EXPECT_EQ(1u, late["count"].asUInt());
}

// ---------------------------------------------------------------- command: single cell

TEST_F(SelectionInspectCommandTest, SingleCellBlockHasRowsAndDetails)
{
	Rig rig;
	rig.detail.sectionTitle = "Ecology";
	rig.detail.valueKey = StringCRC("cells/temp");
	rig.detail.valueToEmit = 0.25f;
	rig.detail.rowsToEmit = 1;
	rig.registry.Details().Register(&rig.grid.desc, "Eco", rig.detail.Delegate());
	Exec("cells.select.cell", "{\"q\":1,\"r\":0}");	// idx 1: temp 0.5, terrain 1 (water)
	rig.Settle();
	const Json::Value r = Exec("cells.inspect", "{}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	const Json::Value& c = r["data"]["cell"];
	ASSERT_TRUE(c.isObject());
	EXPECT_EQ(1u, c["index"].asUInt());
	EXPECT_EQ(1u, c["q"].asUInt());
	EXPECT_EQ(0u, c["r"].asUInt());
	ASSERT_EQ(2u, c["rows"].size());
	EXPECT_EQ("temp", c["rows"][0u]["layer"].asString());
	EXPECT_EQ(0.5, c["rows"][0u]["value"].asDouble());
	EXPECT_EQ("C", c["rows"][0u]["unit"].asString());
	EXPECT_TRUE(c["rows"][0u]["hasData"].asBool());
	EXPECT_FALSE(c["rows"][0u]["isNaN"].asBool());
	EXPECT_EQ("water", c["rows"][1u]["enumId"].asString());
	EXPECT_EQ("Water", c["rows"][1u]["enumLabel"].asString());
	EXPECT_FALSE(c["rows"][1u]["otherValue"].asBool());
	// details: provider Section (name), builder Section, value row, text row
	ASSERT_EQ(4u, c["details"].size());
	EXPECT_EQ("section", c["details"][0u]["kind"].asString());
	EXPECT_EQ("Eco", c["details"][0u]["label"].asString());
	EXPECT_EQ("section", c["details"][1u]["kind"].asString());
	EXPECT_EQ("Ecology", c["details"][1u]["label"].asString());
	EXPECT_EQ("value", c["details"][2u]["kind"].asString());
	EXPECT_EQ("Temperature", c["details"][2u]["label"].asString());
	EXPECT_EQ("C", c["details"][2u]["unit"].asString());
	EXPECT_EQ(0.25, c["details"][2u]["value"].asDouble());
	EXPECT_TRUE(c["details"][2u].isMember("tooltip"));
	EXPECT_EQ("text", c["details"][3u]["kind"].asString());
	EXPECT_EQ("row0", c["details"][3u]["label"].asString());
	EXPECT_EQ("v0", c["details"][3u]["text"].asString());
}

TEST_F(SelectionInspectCommandTest, NoCellBlockForMultiSelection)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	const Json::Value r = Exec("cells.inspect", "{}");
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_FALSE(r["data"].isMember("cell"));
}

// ---------------------------------------------------------------- command: validation, read-only, lifetime

TEST_F(SelectionInspectCommandTest, ArgumentValidationErrors)
{
	Rig rig;
	SelectBox();
	for (const char* bad : {
		"{\"group\":\"Terrain\",\"all\":true}",
		"{\"group\":\"Terrain\",\"layers\":[\"temp\"]}",
		"{\"all\":true,\"layers\":[\"temp\"]}",
		"{\"layerId\":\"temp\",\"layers\":[\"temp\"]}",
		"{\"layers\":\"temp\"}",
		"{\"layers\":[]}",
		"{\"layers\":[1]}",
		"{\"layers\":[\"\"]}",
		"{\"layerId\":5}",
		"{\"group\":5}",
		"{\"group\":\"\"}",
		"{\"all\":\"yes\"}",
		"[1]" })
	{
		const Json::Value r = Exec("cells.inspect", bad);
		EXPECT_FALSE(r["success"].asBool()) << bad;
	}
	EXPECT_TRUE(Exec("cells.inspect", "{\"all\":false}")["success"].asBool());	// false = not requested
}

TEST_F(SelectionInspectCommandTest, QueryIsReadOnly)
{
	Rig rig;
	SelectBox();
	Exec("cells.select.pin", "{}");
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	rig.Settle();
	const uint32_t version = rig.domain.ActiveSelectionView()->Version();
	const uint32_t count = rig.domain.ActiveSelectionView()->Count();
	const uint32_t passes = rig.domain.Inspector().PassCount();
	const uint32_t pinned = rig.domain.Inspector().PinnedCount();
	for (const char* args : { "{}", "{\"all\":true}", "{\"group\":\"Weather\"}", "{\"layers\":[\"temp\"]}" })
	{
		ASSERT_TRUE(Exec("cells.inspect", args)["success"].asBool());
	}
	EXPECT_EQ(version, rig.domain.ActiveSelectionView()->Version());
	EXPECT_EQ(count, rig.domain.ActiveSelectionView()->Count());
	EXPECT_EQ(passes, rig.domain.Inspector().PassCount());
	EXPECT_TRUE(rig.domain.Inspector().IsPinned());
	EXPECT_FALSE(rig.domain.Inspector().IsFrozen());
	EXPECT_EQ(pinned, rig.domain.Inspector().PinnedCount());
}

TEST_F(SelectionInspectCommandTest, CommandUnregistersWithDomain)
{
	Rig rig;
	EXPECT_TRUE(Exec("cells.inspect", "{}")["success"].asBool());
	rig.commands.Unregister();
	EXPECT_FALSE(Exec("cells.inspect", "{}")["success"].asBool());
}

// ---------------------------------------------------------------- model without a card / domain

TEST_F(SelectionInspectJsonModelTest, WorksWithoutDomainOrCard)
{
	sel.Set(0, true);
	sel.Set(1, true);
	state.Tick(registry, *reader, service, &sel);	// the viewer ticks before any query; this builds the shown set
	InspectRequest req;
	Json::Value out;
	ASSERT_TRUE(Build(req, out)) << out.toStyledString();
	EXPECT_TRUE(out["ok"].asBool());
	EXPECT_EQ(2u, out["selected"].asUInt());
	ASSERT_GE(out["layers"].size(), 1u);
	EXPECT_EQ("temp", out["layers"][0u]["layer"].asString());
	EXPECT_EQ(1.5, out["layers"][0u]["mean"].asDouble());
}

TEST_F(SelectionInspectJsonModelTest, EmptyOrNullSelectionComputesNothing)
{
	CellSelection empty;
	empty.Reset(&grid);
	InspectRequest req;
	req.mode = InspectRequest::Mode::All;
	Json::Value out;
	ASSERT_TRUE(Build(req, out, &empty));
	EXPECT_EQ(0u, out["selected"].asUInt());
	EXPECT_EQ(0u, out["layers"].size());
	ASSERT_TRUE(InspectorModel::BuildStatsJson(registry, *reader, service, state, nullptr, req, out));
	EXPECT_EQ(0u, out["selected"].asUInt());
	EXPECT_EQ(0u, pTemp.readCalls + pBiome.readCalls);
}

TEST_F(SelectionInspectJsonModelTest, UnknownLayerFailsWithErrorText)
{
	sel.Set(0, true);
	InspectRequest req;
	req.mode = InspectRequest::Mode::Layers;
	req.AddLayer(StringCRC("missing"));
	Json::Value out;
	EXPECT_FALSE(Build(req, out));
	EXPECT_NE(out["error"].asString().find("missing"), std::string::npos);
}

TEST_F(SelectionInspectJsonModelTest, DoesNotDisturbInspectorStateCache)
{
	sel.Set(0, true);
	state.Tick(registry, *reader, service, &sel);
	state.Tick(registry, *reader, service, &sel);
	const uint32_t passes = state.PassCount();
	const LayerStats* cached = state.StatsFor(StringCRC("temp"));
	ASSERT_NE(nullptr, cached);
	const uint32_t cachedCount = cached->count;
	sel.Set(1, true);	// the model must answer for the live selection, the state is not ticked
	InspectRequest req;
	req.mode = InspectRequest::Mode::All;
	Json::Value out;
	ASSERT_TRUE(Build(req, out));
	EXPECT_EQ(2u, out["selected"].asUInt());
	EXPECT_EQ(2u, out["layers"][0u]["count"].asUInt());
	EXPECT_EQ(passes, state.PassCount());
	EXPECT_EQ(cachedCount, state.StatsFor(StringCRC("temp"))->count);
}

TEST_F(SelectionInspectCommandTest, HistogramJsonCountsOutOfRangeValuesInTheEndBinsAndFooter)
{
	Rig rig;
	rig.temp.values[0] = -5.0f;		// schema range 0..200
	rig.temp.values[1] = 250.0f;
	rig.temp.values[2] = 300.0f;
	rig.temp.versionValue++;
	SelectBox();	// idx {0,1,2,20,21,22,40,41,42}
	rig.Settle();
	const Json::Value h = Exec("cells.inspect", "{}")["data"]["histogram"];
	ASSERT_TRUE(h.isObject());
	EXPECT_EQ(1u, h["outOfRangeLow"].asUInt());
	EXPECT_EQ(2u, h["outOfRangeHigh"].asUInt());
	EXPECT_EQ(9u, h["count"].asUInt());
	EXPECT_GE(h["bins"][0u].asUInt(), 1u);
	EXPECT_GE(h["bins"][15u].asUInt(), 2u);
	uint32_t sum = 0;
	for (const Json::Value& b : h["bins"]) { sum += b.asUInt(); }
	EXPECT_EQ(9u, sum);
	Json::Value state(Json::objectValue);
	rig.domain.GetJSONState(state);
	EXPECT_EQ(1u, state["inspector"]["histogram"]["outOfRangeLow"].asUInt());
	EXPECT_EQ(2u, state["inspector"]["histogram"]["outOfRangeHigh"].asUInt());
}

TEST_F(SelectionInspectCommandTest, DuplicateLayerIdsInTheListAreAnsweredPerRequestEntry)
{
	Rig rig;
	SelectBox();
	const Json::Value r = Exec("cells.inspect", "{\"layers\":[\"temp\",\"temp\"]}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	ASSERT_EQ(2u, r["data"]["layers"].size());
	EXPECT_EQ(r["data"]["layers"][0u]["mean"], r["data"]["layers"][1u]["mean"]);
}

TEST_F(SelectionInspectCommandTest, PinnedDeltaIsNegativeWhenBIsSmallerThanA)
{
	Rig rig;
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":4,\"r1\":0}");	// idx 0..4 -> temp 0,.5,1,1.5,2 (mean 1)
	ASSERT_TRUE(Exec("cells.select.pin", "{}")["success"].asBool());
	Exec("cells.select.cell", "{\"q\":0,\"r\":0}");	// B = idx 0 -> temp 0
	rig.Settle();
	const Json::Value r = Exec("cells.inspect", "{\"layers\":[\"temp\"]}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	const Json::Value& temp = r["data"]["layers"][0u];
	EXPECT_EQ(1u, temp["count"].asUInt());
	EXPECT_EQ(5u, temp["a"]["count"].asUInt());
	EXPECT_EQ(-4, temp["delta"]["count"].asInt());
	EXPECT_EQ(-1.0, temp["delta"]["mean"].asDouble());
	EXPECT_EQ(-2.0, temp["delta"]["max"].asDouble());
	EXPECT_EQ(0.0, temp["delta"]["min"].asDouble());
}

TEST_F(SelectionInspectCommandTest, UnpinRemovesTheABlockFromTheResult)
{
	Rig rig;
	SelectBox();
	Exec("cells.select.pin", "{}");
	rig.Settle();
	ASSERT_TRUE(Exec("cells.inspect", "{}")["data"].isMember("pinned"));
	Exec("cells.select.unpin", "{}");
	rig.Settle();
	const Json::Value d = Exec("cells.inspect", "{}")["data"];
	EXPECT_FALSE(d.isMember("pinned"));
	EXPECT_FALSE(d["layers"][0u].isMember("a"));
	EXPECT_FALSE(d["layers"][0u].isMember("delta"));
}

TEST_F(SelectionInspectCommandTest, ClearingTheSelectionKeepsAAndInspectReportsEmpty)
{
	Rig rig;
	SelectBox();
	Exec("cells.select.pin", "{}");
	Exec("cells.select.clear", "{}");
	rig.Settle();
	const Json::Value r = Exec("cells.inspect", "{}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_EQ(0u, r["data"]["selected"].asUInt());
	EXPECT_EQ(0u, r["data"]["layers"].size());
	EXPECT_EQ(9u, rig.domain.Inspector().PinnedCount());
}

// ---- Task 10a exhaustive pass (Feature 4): late registration, reentrancy ----

TEST_F(SelectionInspectCommandTest, LayerRegisteredLaterAppearsInTheDefaultFormAndTheCard)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	ASSERT_EQ(nullptr, FindLayer(Exec("cells.inspect", "{}")["data"], "late"));
	rig.Add(rig.late, "late", "Terrain", CellValueKind::Float);	// active group is expanded
	rig.Settle();
	const Json::Value r = Exec("cells.inspect", "{}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	const Json::Value* late = FindLayer(r["data"], "late");
	ASSERT_NE(nullptr, late);
	EXPECT_EQ(9u, (*late)["count"].asUInt());
	Json::Value state(Json::objectValue);
	rig.domain.GetJSONState(state);
	bool onCard = false;
	for (const Json::Value& l : state["inspector"]["layers"]) { onCard = onCard || l["id"].asString() == "late"; }
	EXPECT_TRUE(onCard);
}

TEST_F(SelectionInspectCommandTest, LayerRegisteredLaterInACollapsedGroupIsNotShownButIsInAll)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	rig.Add(rig.late, "late", "Other", CellValueKind::Float);
	rig.Settle();
	EXPECT_EQ(nullptr, FindLayer(Exec("cells.inspect", "{}")["data"], "late"));
	EXPECT_NE(nullptr, FindLayer(Exec("cells.inspect", "{\"all\":true}")["data"], "late"));
}

namespace
{
	// A layer read delegate that calls back into the domain from inside an inspector pass / freeze. The viewer has no mutex
	// now, so the inspector and the layer reader must stay consistent (and not crash) when this happens.
	enum class ReenterWith { None, InspectAll, CardState, Unpin, PinLive, PinFreeze, Expand };
	CellLayerViewerDomain* gInspDomain = nullptr;
	ReenterWith gReenterWith = ReenterWith::None;
	bool gInspArmed = false;
	uint32_t gInspCalls = 0;
	uint64_t gRdVersion = 1;
	Json::Value gNestedReply;

	uint64_t RdVersion() { return gRdVersion; }
	void ReadInspectReentering(float* out, uint32_t n)
	{
		for (uint32_t i = 0; i < n; ++i) { out[i] = 1.0f; }
		if (!gInspArmed) { return; }
		gInspArmed = false;
		++gInspCalls;
		switch (gReenterWith)
		{
		case ReenterWith::InspectAll:	gNestedReply = Exec("cells.inspect", "{\"all\":true}"); break;
		case ReenterWith::CardState:	{ Json::Value s(Json::objectValue); gInspDomain->GetJSONState(s); gNestedReply = s; } break;
		case ReenterWith::Unpin:		gNestedReply = Exec("cells.select.unpin", "{}"); break;
		case ReenterWith::PinLive:		gNestedReply = Exec("cells.select.pin", "{}"); break;
		case ReenterWith::PinFreeze:	gNestedReply = Exec("cells.select.pin", "{\"freeze\":true}"); break;
		case ReenterWith::Expand:		gNestedReply = Exec("cells.inspector.expand", "{\"group\":\"Weather\",\"expanded\":true}"); break;
		case ReenterWith::None:			break;
		}
	}

	struct InspectReentryTest : SelectionInspectCommandTest
	{
		void SetUp() override
		{
			SelectionInspectCommandTest::SetUp();
			gInspDomain = nullptr; gReenterWith = ReenterWith::None; gInspArmed = false; gInspCalls = 0; gRdVersion = 1;
			gNestedReply = Json::Value();
		}
		void TearDown() override { gInspDomain = nullptr; SelectionInspectCommandTest::TearDown(); }

		// Registers "rd" (Terrain group, so it is in the shown set) with the reentrant delegate. Not read until the first pass.
		static void AddReentrant(Rig& rig, ReenterWith mode)
		{
			CellLayerDesc d = rig.temp.MakeDesc("rd", &rig.grid.desc, CellValueKind::Float, "cells/temp");
			d.group.Clear();
			d.group.Append("Terrain");
			d.read = fastdelegate::FastDelegate2<float*, uint32_t>(&ReadInspectReentering);
			d.version = fastdelegate::FastDelegate0<uint64_t>(&RdVersion);
			ASSERT_EQ(CellRegisterResult::Ok, rig.registry.Register(d));
			gInspDomain = &rig.domain;
			gReenterWith = mode;
			gInspArmed = true;
		}

		static const Json::Value* CardLayer(const Json::Value& state, const char* id)
		{
			for (const Json::Value& l : state["inspector"]["layers"]) { if (l["id"].asString() == id) { return &l; } }
			return nullptr;
		}
	};
}

TEST_F(InspectReentryTest, NestedInspectAllInsideAPassDoesNotCorruptTheLayerBeingRead)
{
	Rig rig;
	AddReentrant(rig, ReenterWith::InspectAll);
	SelectBox();
	rig.Settle();
	ASSERT_EQ(1u, gInspCalls);
	ASSERT_TRUE(gNestedReply["success"].asBool()) << gNestedReply.toStyledString();
	const Json::Value* nestedRain = FindLayer(gNestedReply["data"], "rain");
	ASSERT_NE(nullptr, nestedRain);
	EXPECT_EQ(10.0, (*nestedRain)["mean"].asDouble());
	// rd is 1.0 everywhere; the nested read of "rain" (10.0) must not have overwritten rd's buffer
	Json::Value state(Json::objectValue);
	rig.domain.GetJSONState(state);
	const Json::Value* rd = CardLayer(state, "rd");
	ASSERT_NE(nullptr, rd);
	EXPECT_EQ(9u, (*rd)["count"].asUInt());
	EXPECT_EQ(1.0, (*rd)["mean"].asDouble());
	EXPECT_EQ(1.0, (*rd)["min"].asDouble());
	EXPECT_EQ(1.0, (*rd)["max"].asDouble());
	const Json::Value after = Exec("cells.inspect", "{\"layers\":[\"rd\"]}");
	EXPECT_EQ(1.0, after["data"]["layers"][0u]["mean"].asDouble());
}

TEST_F(InspectReentryTest, NestedCardStateInsideAPassCompletes)
{
	Rig rig;
	AddReentrant(rig, ReenterWith::CardState);
	Exec("cells.select.cell", "{\"q\":1,\"r\":0}");	// one cell: the card reads the single-cell table through the reader
	rig.Settle();
	ASSERT_EQ(1u, gInspCalls);
	ASSERT_TRUE(gNestedReply.isMember("inspector"));
	Json::Value state(Json::objectValue);
	rig.domain.GetJSONState(state);
	const Json::Value* rd = CardLayer(state, "rd");
	ASSERT_NE(nullptr, rd);
	EXPECT_EQ(1.0, (*rd)["mean"].asDouble());
	bool rdRow = false;
	for (const Json::Value& row : state["inspector"]["cell"]["rows"])
	{
		if (row["layer"].asString() == "rd") { rdRow = true; EXPECT_EQ(1.0, row["value"].asDouble()); }
	}
	EXPECT_TRUE(rdRow);
}

TEST_F(InspectReentryTest, UnpinInsideALiveAPassIsRejectedAndAIsIntact)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	AddReentrant(rig, ReenterWith::Unpin);
	ASSERT_TRUE(Exec("cells.select.pin", "{}")["success"].asBool());
	rig.Settle();	// the first pass reads rd for B and A; the delegate unpins
	ASSERT_EQ(1u, gInspCalls);
	EXPECT_FALSE(gNestedReply["success"].asBool()) << gNestedReply.toStyledString();
	EXPECT_TRUE(rig.domain.Inspector().IsPinned());
	EXPECT_EQ(9u, rig.domain.Inspector().PinnedCount());
	const LayerStats* a = rig.domain.Inspector().StatsForA(StringCRC("rd"));
	ASSERT_NE(nullptr, a);
	EXPECT_EQ(9u, a->count);
}

TEST_F(InspectReentryTest, PinInsideALiveAPassIsRejectedAndTheOldAIsKept)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	AddReentrant(rig, ReenterWith::PinLive);
	ASSERT_TRUE(Exec("cells.select.pin", "{}")["success"].asBool());
	rig.Settle();
	ASSERT_EQ(1u, gInspCalls);
	EXPECT_FALSE(gNestedReply["success"].asBool()) << gNestedReply.toStyledString();
	EXPECT_TRUE(rig.domain.Inspector().IsPinned());
	EXPECT_EQ(9u, rig.domain.Inspector().PinnedCount());
}

TEST_F(InspectReentryTest, UnpinInsideAFreezeIsRejectedAndTheFreezeCompletes)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	AddReentrant(rig, ReenterWith::Unpin);	// rd is first read by the freeze itself
	const Json::Value r = Exec("cells.select.pin", "{\"freeze\":true}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	ASSERT_EQ(1u, gInspCalls);
	EXPECT_FALSE(gNestedReply["success"].asBool()) << gNestedReply.toStyledString();
	EXPECT_TRUE(rig.domain.Inspector().IsFrozen());
	EXPECT_EQ(5u, rig.domain.Inspector().FrozenLayerCount());	// temp, terrain, rain, wind, rd
	ASSERT_NE(nullptr, rig.domain.Inspector().StatsForA(StringCRC("rd")));
	EXPECT_EQ(1.0, rig.domain.Inspector().StatsForA(StringCRC("rd"))->mean);
}

TEST_F(InspectReentryTest, PinFreezeInsideAFreezeIsRejectedAndTheOuterFreezeCompletes)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	AddReentrant(rig, ReenterWith::PinFreeze);
	const Json::Value r = Exec("cells.select.pin", "{\"freeze\":true}");
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_FALSE(gNestedReply["success"].asBool()) << gNestedReply.toStyledString();
	EXPECT_EQ(5u, rig.domain.Inspector().FrozenLayerCount());
	EXPECT_EQ(5u, r["data"]["layers"].asUInt());
}

TEST_F(InspectReentryTest, ExpandInsideAPassTakesEffectOnALaterPass)
{
	Rig rig;
	AddReentrant(rig, ReenterWith::Expand);
	SelectBox();
	rig.Settle();
	ASSERT_EQ(1u, gInspCalls);
	EXPECT_TRUE(gNestedReply["success"].asBool()) << gNestedReply.toStyledString();
	EXPECT_TRUE(rig.domain.Inspector().IsGroupExpanded("Weather"));
	Json::Value state(Json::objectValue);
	rig.domain.GetJSONState(state);
	ASSERT_NE(nullptr, CardLayer(state, "rain"));
	EXPECT_EQ(10.0, (*CardLayer(state, "rain"))["mean"].asDouble());
	ASSERT_NE(nullptr, CardLayer(state, "rd"));
	EXPECT_EQ(1.0, (*CardLayer(state, "rd"))["mean"].asDouble());
}

TEST_F(InspectReentryTest, TheDomainStillWorksAfterRejectedNestedCommands)
{
	Rig rig;
	SelectBox();
	rig.Settle();
	AddReentrant(rig, ReenterWith::Unpin);
	Exec("cells.select.pin", "{}");
	rig.Settle();
	ASSERT_TRUE(Exec("cells.select.unpin", "{}")["success"].asBool());	// no longer busy
	EXPECT_FALSE(rig.domain.Inspector().IsPinned());
	ASSERT_TRUE(Exec("cells.select.pin", "{\"freeze\":true}")["success"].asBool());
	EXPECT_TRUE(rig.domain.Inspector().IsFrozen());
}

#endif // DIA_DEBUG
