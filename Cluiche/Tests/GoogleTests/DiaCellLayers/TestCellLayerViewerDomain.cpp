#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/ViewerCommands.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/CellPickerTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>
#include <DiaAPI/DiaAPI.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaPathfinding/HexTopology.h>

#include <string.h>

#ifdef DIA_DEBUG

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;
using Dia::Observation::Health::HealthStatus;
using Dia::Observation::Metric::MetricRegistry;

namespace
{
	struct FakeDraw : public Dia::Core::IDebugDraw
	{
		uint32_t cellFieldRequests = 0;
		Dia::Core::CellFieldDrawRequest lastRequest;
		Dia::Maths::Vector2D mouse;

		void RequestDraw(const Dia::Maths::Vector2D&, float, Dia::Core::RGBA, Dia::Core::RGBA) override {}
		void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, Dia::Core::RGBA) override {}
		void RequestDrawPoint(const Dia::Maths::Vector2D&, Dia::Core::RGBA) override {}
		void RequestDrawRect(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, Dia::Core::RGBA, Dia::Core::RGBA) override {}
		void RequestDrawArc(const Dia::Maths::Vector2D&, float, float, float, Dia::Core::RGBA) override {}
		void RequestDrawRay(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, float, Dia::Core::RGBA) override {}
		void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, Dia::Core::RGBA, Dia::Core::RGBA) override {}
		void RequestDrawText(const Dia::Maths::Vector2D&, const char*, float, Dia::Core::RGBA) override {}
		void RequestDrawLine3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, Dia::Core::RGBA) override {}
		void RequestDrawRay3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, float, Dia::Core::RGBA) override {}
		void RequestDrawBox3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, Dia::Core::RGBA) override {}
		void RequestDrawSphere3D(const Dia::Maths::Vector3D&, float, Dia::Core::RGBA) override {}
		void RequestDrawArrow3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, float, float, Dia::Core::RGBA) override {}
		void RequestDrawCellField(const Dia::Core::CellFieldDrawRequest& r) override { ++cellFieldRequests; lastRequest = r; }
		uint32_t DroppedCount() const override { return 0; }
		const Dia::Maths::Vector2D& GetMousePixel() const override { return mouse; }
	};

	uint32_t gOverlayDraws = 0;
	void CountingDraw(Dia::Core::IDebugDraw&, const void*) { ++gOverlayDraws; }

	CellPickerView TestView() { CellPickerView v; v.viewportWidth = 1280; v.viewportHeight = 720; return v; }
	CellPickerView ViewFn0() { return TestView(); }

	struct ViewerTest : ::testing::Test
	{
		TestGrid gridA{ 4, 3 };
		TestGrid gridB{ 5, 2 };
		CellFieldRegistry cellField;
		UploaderSchemaFixture fx;
		StubProvider temp, rain, soil, biome;
		FakeDraw draw;

		void SetUp() override
		{
			FakePuId() = 1;
			gOverlayDraws = 0;
			temp.values = { 5, -2, 7, 3, 1, 0, 4, 9, 2, 2, 2, 2 };
			rain.values = std::vector<float>(12, 1.0f);
			soil.values = std::vector<float>(10, 3.0f);
			biome.values = { 0, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
		}

		static Dia::ValueSchema::ValueInfo Float(const char* key, bool defaultBase, const char* unit, const char* desc)
		{
			auto i = UploaderSchemaFixture::FloatEntry(key, -10.0, 10.0);
			if (defaultBase) { i.mUi["default_base"] = true; }
			Dia::ValueSchema::SetText(i.mUnit, unit);
			Dia::ValueSchema::SetText(i.mDescription, desc);
			Dia::ValueSchema::SetText(i.mLabel, (std::string("L-") + key).c_str());
			return i;
		}
	};

	// Registry + uploader + domain over a published schema (or none).
	struct Rig
	{
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;
		Rig(CellFieldRegistry& cf, const Dia::ValueSchema::ValueSchemaService* s, CellLayerViewerDomain::ViewFn view = CellLayerViewerDomain::ViewFn())
			: registry(s, &FakePuIdFn), uploader(cf, s), domain(registry, uploader, s, view) {}
		void Add(const CellLayerDesc& d) { ASSERT_EQ(registry.Register(d), CellRegisterResult::Ok); }
	};

	double CounterNow(const char* name)
	{
		auto* c = MetricRegistry::Instance().FindCounter(StringCRC(name));
		return c ? static_cast<double>(c->Value()) : 0.0;
	}
	double GaugeNow(const char* name)
	{
		auto* g = MetricRegistry::Instance().FindGauge(StringCRC(name));
		return g ? g->Value() : -1.0;
	}

	void PointAtCell(FakeDraw& draw, int q, int r)
	{
		const Dia::Maths::Vector2D c = Dia::Pathfinding::HexTopology::CellCenter(Dia::Pathfinding::CellCoord{ q, r }, 1.0f);
		float sx, sy;
		WorldToScreen(TestView(), c.X(), c.Y(), sx, sy);
		draw.mouse = Dia::Maths::Vector2D(sx, sy);
	}
}

TEST_F(ViewerTest, BaseDefaultsToFirstLayerWithoutSchema)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("temp"));
	EXPECT_EQ(rig.domain.ActiveGrid(), &gridA.desc);
	EXPECT_EQ(draw.cellFieldRequests, 1u);
	EXPECT_TRUE(draw.lastRequest.layer.IsValid());
}

TEST_F(ViewerTest, BaseDefaultsToUiDefaultBase)
{
	fx.Add(Float("temp", false, "C", "t"));
	fx.Add(Float("rain", true, "mm", "r"));
	const auto& svc = fx.Publish();
	Rig rig(cellField, &svc);
	rig.Add(temp.MakeDesc("temp", &gridA.desc, CellValueKind::Float, "cells/temp"));
	rig.Add(rain.MakeDesc("rain", &gridA.desc, CellValueKind::Float, "cells/rain"));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("rain"));
}

TEST_F(ViewerTest, SetBaseSwitchesAndChangesActiveGrid)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.ActiveGrid(), &gridA.desc);
	EXPECT_TRUE(rig.domain.SetBase(StringCRC("soil")));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("soil"));
	EXPECT_EQ(rig.domain.ActiveGrid(), &gridB.desc);
	EXPECT_EQ(soil.readCalls, 1u);
}

TEST_F(ViewerTest, UnknownBaseIdChangesNothing)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.domain.Update(draw);
	EXPECT_FALSE(rig.domain.SetBase(StringCRC("nope")));
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("temp"));
	EXPECT_FALSE(rig.domain.ToggleOverlay(StringCRC("nope")));
	EXPECT_FALSE(rig.domain.SetGrid(7));
	EXPECT_EQ(rig.domain.ActiveGrid(), &gridA.desc);
}

TEST_F(ViewerTest, UnregisterActiveFallsBackToGridDefault)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	rig.domain.Update(draw);
	ASSERT_TRUE(rig.domain.SetBase(StringCRC("rain")));
	rig.domain.Update(draw);
	rig.registry.Unregister(StringCRC("rain"));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("temp"));
	EXPECT_FALSE(rig.domain.IsEmptyState());
}

TEST_F(ViewerTest, UnregisterLastLayerOfGridGivesEmptyState)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.domain.Update(draw);
	ASSERT_EQ(draw.cellFieldRequests, 1u);
	rig.registry.Unregister(StringCRC("temp"));
	rig.domain.Update(draw);
	EXPECT_TRUE(rig.domain.IsEmptyState());
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC::kZero);
	EXPECT_EQ(draw.cellFieldRequests, 1u);	// nothing submitted for the empty state
	Json::Value state;
	rig.domain.GetJSONState(state);
	EXPECT_TRUE(state["empty"].asBool());
	EXPECT_EQ(state["layers"].size(), 0u);
}

TEST_F(ViewerTest, OverlayDrawsOnlyWhenEnabled)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	CellOverlayDesc o = MakeOverlay("ov", "Overlay");
	o.draw = fastdelegate::FastDelegate2<Dia::Core::IDebugDraw&, const void*>(&CountingDraw);
	ASSERT_EQ(rig.registry.RegisterOverlay(o), CellRegisterResult::Ok);
	rig.domain.Update(draw);
	EXPECT_EQ(gOverlayDraws, 0u);
	EXPECT_FALSE(rig.domain.IsOverlayEnabled(StringCRC("ov")));
	EXPECT_TRUE(rig.domain.ToggleOverlay(StringCRC("ov")));
	rig.domain.Update(draw);
	EXPECT_EQ(gOverlayDraws, 1u);
	EXPECT_TRUE(rig.domain.ToggleOverlay(StringCRC("ov")));
	rig.domain.Update(draw);
	EXPECT_EQ(gOverlayDraws, 1u);
}

TEST_F(ViewerTest, GridMismatchAdoptsNewGrid)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.domain.Update(draw);
	rig.registry.Unregister(StringCRC("temp"));
	rig.Add(temp.MakeDesc("temp", &gridB.desc));	// same id, other grid
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("temp"));
	EXPECT_EQ(rig.domain.ActiveGrid(), &gridB.desc);
}

TEST_F(ViewerTest, JSONStateShape)
{
	fx.Add(Float("temp", true, "C", "Air temperature"));
	fx.Add(Float("rain", false, "mm", "Rainfall"));
	const auto& svc = fx.Publish();
	Rig rig(cellField, &svc);
	rig.Add(temp.MakeDesc("temp", &gridA.desc, CellValueKind::Float, "cells/temp"));
	rig.Add(rain.MakeDesc("rain", &gridA.desc, CellValueKind::Float, "cells/rain"));
	ASSERT_EQ(rig.registry.RegisterOverlay(MakeOverlay("ov", "My Overlay")), CellRegisterResult::Ok);
	rig.domain.Update(draw);

	Json::Value s;
	rig.domain.GetJSONState(s);
	ASSERT_TRUE(s["layers"].isArray());
	ASSERT_EQ(s["layers"].size(), 2u);
	EXPECT_STREQ(s["layers"][0]["id"].asCString(), "temp");
	EXPECT_STREQ(s["layers"][0]["label"].asCString(), "L-temp");
	EXPECT_STREQ(s["layers"][0]["unit"].asCString(), "C");
	EXPECT_TRUE(s["layers"][0]["selected"].asBool());
	EXPECT_FALSE(s["layers"][1]["selected"].asBool());
	EXPECT_EQ(s["layers"][0]["groupNumber"].asUInt(), 1u);
	EXPECT_EQ(s["layers"][1]["groupNumber"].asUInt(), 2u);
	EXPECT_TRUE(s["layers"][0].isMember("hotkey"));
	ASSERT_EQ(s["overlays"].size(), 1u);
	EXPECT_STREQ(s["overlays"][0]["id"].asCString(), "ov");
	EXPECT_STREQ(s["overlays"][0]["label"].asCString(), "My Overlay");
	EXPECT_FALSE(s["overlays"][0]["enabled"].asBool());
	EXPECT_TRUE(s["groups"].isArray());
	EXPECT_EQ(s["grids"].size(), 1u);
	EXPECT_FALSE(s.isMember("gridSelector"));	// single grid
	ASSERT_TRUE(s["legend"].isObject());
	EXPECT_STREQ(s["legend"]["unit"].asCString(), "C");
	EXPECT_FALSE(s["legend"]["category"].asBool());
	EXPECT_GE(s["legend"]["ticks"].size(), 2u);
	EXPECT_FALSE(s["hover"]["valid"].asBool());
}

TEST_F(ViewerTest, JSONStateGridSelectorOnlyWhenMultiGrid)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	rig.domain.Update(draw);
	Json::Value s;
	rig.domain.GetJSONState(s);
	ASSERT_TRUE(s.isMember("gridSelector"));
	EXPECT_EQ(s["gridSelector"].size(), 2u);
	EXPECT_EQ(s["layers"].size(), 1u);	// layers of the active grid only
	EXPECT_TRUE(s["gridSelector"][0]["active"].asBool());
	EXPECT_TRUE(rig.domain.SetGrid(1));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("soil"));
}

TEST_F(ViewerTest, ListLayersGroupsByGrid)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	ASSERT_EQ(rig.registry.RegisterOverlay(MakeOverlay("ov")), CellRegisterResult::Ok);
	Json::Value l;
	rig.domain.ListLayers(l);
	ASSERT_EQ(l["grids"].size(), 2u);
	EXPECT_STREQ(l["grids"][0]["layers"][0]["id"].asCString(), "temp");
	EXPECT_STREQ(l["grids"][1]["layers"][0]["id"].asCString(), "soil");
	EXPECT_STREQ(l["overlays"][0]["id"].asCString(), "ov");
}

TEST_F(ViewerTest, ListLayersReportsActiveBaseAndSelected)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	rig.domain.Update(draw);
	Json::Value l;
	rig.domain.ListLayers(l);
	EXPECT_EQ(l["activeBase"].asString(), "temp");
	EXPECT_EQ(l["activeGridIndex"].asInt(), 0);
	EXPECT_TRUE(l["grids"][0]["layers"][0]["selected"].asBool());
	EXPECT_FALSE(l["grids"][1]["layers"][0]["selected"].asBool());

	ASSERT_TRUE(rig.domain.SetBase(StringCRC("soil")));
	EXPECT_FALSE(rig.domain.SetBase(StringCRC("nope")));
	Json::Value l2;
	rig.domain.ListLayers(l2);
	EXPECT_EQ(l2["activeBase"].asString(), "soil");
	EXPECT_EQ(l2["activeGridIndex"].asInt(), 1);
	EXPECT_FALSE(l2["grids"][0]["layers"][0]["selected"].asBool());
	EXPECT_TRUE(l2["grids"][1]["layers"][0]["selected"].asBool());
}

TEST_F(ViewerTest, CommandsViaSimHostedRegistrar)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	ASSERT_EQ(rig.registry.RegisterOverlay(MakeOverlay("ov")), CellRegisterResult::Ok);
	ViewerCommands commands(rig.domain);
	ASSERT_TRUE(commands.Registered());
	rig.domain.Update(draw);

	Json::Value id(Json::objectValue);
	id["id"] = "rain";
	EXPECT_TRUE(ViewerCommands::Exec("cells.layer.base_by_id", id)["success"].asBool());
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("rain"));
	id["id"] = "nope";
	EXPECT_FALSE(ViewerCommands::Exec("cells.layer.base_by_id", id)["success"].asBool());
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("rain"));
	id["id"] = "ov";
	EXPECT_TRUE(ViewerCommands::Exec("cells.overlay.toggle_by_id", id)["success"].asBool());
	EXPECT_TRUE(rig.domain.IsOverlayEnabled(StringCRC("ov")));
	id["id"] = "nope";
	EXPECT_FALSE(ViewerCommands::Exec("cells.overlay.toggle_by_id", id)["success"].asBool());

	Json::Value list;
	rig.domain.ListLayers(list);
	EXPECT_EQ(list["grids"][0]["layers"].size(), 2u);
	commands.Unregister();
	EXPECT_FALSE(commands.Registrar().Has("cells.layer.base_by_id"));
}

TEST_F(ViewerTest, PanelCommands)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	ASSERT_EQ(rig.registry.RegisterOverlay(MakeOverlay("ov")), CellRegisterResult::Ok);
	Json::Value a(Json::objectValue);
	a["id"] = "rain";
	rig.domain.OnCommand(StringCRC("base"), a);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("rain"));
	Json::Value t(Json::objectValue);
	t["drawer"] = "ov";
	rig.domain.OnCommand(StringCRC("toggle"), t);
	EXPECT_TRUE(rig.domain.IsOverlayEnabled(StringCRC("ov")));
}

TEST_F(ViewerTest, HoverInsideGridShowsValueUnitAndDescription)
{
	fx.Add(Float("temp", true, "C", "Air temperature"));
	const auto& svc = fx.Publish();
	Rig rig(cellField, &svc, CellLayerViewerDomain::ViewFn(&ViewFn0));
	rig.Add(temp.MakeDesc("temp", &gridA.desc, CellValueKind::Float, "cells/temp"));
	PointAtCell(draw, 2, 1);
	rig.domain.Update(draw);
	const auto h = rig.domain.Hover();
	ASSERT_TRUE(h.valid);
	EXPECT_EQ(h.q, 2);
	EXPECT_EQ(h.r, 1);
	EXPECT_FLOAT_EQ(h.value, temp.values[1 * 4 + 2]);
	Json::Value s;
	rig.domain.GetJSONState(s);
	EXPECT_TRUE(s["hover"]["valid"].asBool());
	EXPECT_EQ(s["hover"]["q"].asInt(), 2);
	EXPECT_EQ(s["hover"]["r"].asInt(), 1);
	EXPECT_STREQ(s["hover"]["unit"].asCString(), "C");
	EXPECT_STREQ(s["hover"]["description"].asCString(), "Air temperature");
	EXPECT_DOUBLE_EQ(s["hover"]["value"].asDouble(), static_cast<double>(temp.values[6]));
}

TEST_F(ViewerTest, HoverOutsideGridShowsNothing)
{
	Rig rig(cellField, nullptr, CellLayerViewerDomain::ViewFn(&ViewFn0));
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	draw.mouse = Dia::Maths::Vector2D(5.0f, 5.0f);	// far from the 4x3 grid at the canvas centre
	rig.domain.Update(draw);
	EXPECT_FALSE(rig.domain.Hover().valid);
	Json::Value s;
	rig.domain.GetJSONState(s);
	EXPECT_FALSE(s["hover"]["valid"].asBool());
}

TEST_F(ViewerTest, HoverOnCategoryShowsEnumLabel)
{
	fx.Add(UploaderSchemaFixture::EnumEntry("biome", 3));
	const auto& svc = fx.Publish();
	Rig rig(cellField, &svc, CellLayerViewerDomain::ViewFn(&ViewFn0));
	rig.Add(biome.MakeDesc("biome", &gridA.desc, CellValueKind::Category, "cells/biome"));
	PointAtCell(draw, 2, 0);
	rig.domain.Update(draw);
	Json::Value s;
	rig.domain.GetJSONState(s);
	ASSERT_TRUE(s["hover"]["valid"].asBool());
	EXPECT_STREQ(s["hover"]["text"].asCString(), "v2");	// enum value 2: label empty -> id
	EXPECT_TRUE(s["legend"]["category"].asBool());
	EXPECT_EQ(s["legend"]["swatches"].size(), 3u);
}

TEST_F(ViewerTest, HealthDegradedOnReadFailureThenRecovers)
{
	Rig rig(cellField, nullptr);
	CellLayerDesc broken = temp.MakeDesc("temp", &gridA.desc);
	broken.read = fastdelegate::FastDelegate2<float*, uint32_t>();	// no read delegate
	rig.Add(broken);
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.HealthReporter().Report().status, HealthStatus::kDegraded);
	rig.registry.Unregister(StringCRC("temp"));
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.HealthReporter().Report().status, HealthStatus::kOK);
}

TEST_F(ViewerTest, MetricsCountReadsUploadsSkipsAndLayers)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	rig.domain.Update(draw);	// first upload
	const double reads0 = CounterNow("dia.celllayers.viewer.reads");
	const double uploads0 = CounterNow("dia.celllayers.viewer.uploads");
	const double skipped0 = CounterNow("dia.celllayers.viewer.skipped_frames");
	for (int i = 0; i < 5; ++i) { rig.domain.Update(draw); }
	temp.versionValue++;
	rig.domain.Update(draw);
	EXPECT_EQ(CounterNow("dia.celllayers.viewer.reads") - reads0, 1.0);
	EXPECT_EQ(CounterNow("dia.celllayers.viewer.uploads") - uploads0, 1.0);
	EXPECT_EQ(CounterNow("dia.celllayers.viewer.skipped_frames") - skipped0, 5.0);
	EXPECT_EQ(GaugeNow("dia.celllayers.viewer.registered_layers"), 2.0);
	EXPECT_EQ(GaugeNow("dia.celllayers.viewer.visible_layers"), 2.0);
	EXPECT_GE(GaugeNow("dia.celllayers.viewer.read_ms"), 0.0);
}

TEST_F(ViewerTest, IdentityIsAPanelOnlyDomain)
{
	Rig rig(cellField, nullptr);
	EXPECT_FALSE(rig.domain.HasWorldDrawers());
	EXPECT_NE(rig.domain.GetDomainId(), StringCRC::kZero);
	EXPECT_GT(strlen(rig.domain.GetDisplayName()), 0u);
	EXPECT_LE(strlen(rig.domain.GetDescription()), 80u);
}

TEST_F(ViewerTest, NoDataFlagOnlyWhenSourceNotLiveWithoutData)
{
	fx.Add(Float("temp", true, "C", "t"));
	const auto& svc = fx.Publish();
	Rig rig(cellField, &svc);
	rig.Add(temp.MakeDesc("temp", &gridA.desc, CellValueKind::Float, "cells/temp"));
	StubViewValueSource source;
	source.live = true;
	ASSERT_TRUE(rig.registry.SetViewValueSource(&source));
	rig.domain.Update(draw);
	Json::Value s;
	rig.domain.GetJSONState(s);
	EXPECT_FALSE(s.isMember("noData"));
	EXPECT_FALSE(s["legend"].isMember("unitPrefix"));

	source.live = false;
	source.hasData = false;
	const uint32_t requests = draw.cellFieldRequests;
	rig.domain.Update(draw);
	EXPECT_EQ(draw.cellFieldRequests, requests);
	Json::Value n;
	rig.domain.GetJSONState(n);
	ASSERT_TRUE(n.isMember("noData"));
	EXPECT_TRUE(n["noData"].asBool());
	EXPECT_FALSE(n["empty"].asBool());
	EXPECT_FALSE(n["legend"]["available"].asBool());
}

TEST_F(ViewerTest, PresentationPrefixAppearsInLegendJson)
{
	fx.Add(Float("temp", true, "C", "t"));
	const auto& svc = fx.Publish();
	Rig rig(cellField, &svc);
	rig.Add(temp.MakeDesc("temp", &gridA.desc, CellValueKind::Float, "cells/temp"));
	StubViewValueSource source;
	source.values = std::vector<float>(12, 1.0f);
	source.hasPresentation = true;
	source.presentation.rampId = StringCRC("test.heat");
	source.presentation.rangeMin = -2.0f;
	source.presentation.rangeMax = 2.0f;
	source.SetPrefix("D");
	ASSERT_TRUE(rig.registry.SetViewValueSource(&source));
	rig.domain.Update(draw);
	Json::Value s;
	rig.domain.GetJSONState(s);
	EXPECT_STREQ(s["legend"]["unitPrefix"].asCString(), "D");
	EXPECT_STREQ(s["legend"]["unit"].asCString(), "DC");
	EXPECT_EQ(s["legend"]["min"].asFloat(), -2.0f);
	EXPECT_STREQ(s["legend"]["ramp"].asCString(), "test.heat");
}

#endif // DIA_DEBUG
