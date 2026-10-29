#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/CellPickerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>
#include <DiaAPI/DiaAPI.h>
#include <DiaPathfinding/HexTopology.h>

#include <atomic>
#include <random>
#include <set>
#include <string>
#include <thread>

#ifdef DIA_DEBUG

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;
using Dia::Observation::Health::HealthStatus;
using Dia::Observation::Metric::MetricRegistry;

namespace
{
	CellPickerView TestView() { CellPickerView v; v.viewportWidth = 1280; v.viewportHeight = 720; return v; }
	CellPickerView ViewFn0() { return TestView(); }
	CellPickerView NaNView() { CellPickerView v = TestView(); v.cameraX = std::numeric_limits<float>::quiet_NaN(); return v; }

	struct ViewerEdgeTest : ::testing::Test
	{
		TestGrid gridA{ 4, 3 };
		TestGrid gridB{ 5, 2 };
		CellFieldRegistry cellField;
		UploaderSchemaFixture fx;
		StubProvider temp, rain, soil, biome;
		FakeDebugDraw draw;

		void SetUp() override
		{
			FakePuId() = 1;
			temp.values = { 5, -2, 7, 3, 1, 0, 4, 9, 2, 2, 2, 2 };
			rain.values = std::vector<float>(12, 1.0f);
			soil.values = std::vector<float>(10, 3.0f);
			biome.values = { 0, 1, 2, 5, -1, 0, 1, 1, 0, 0, 0, 0 };
		}
	};

	struct Rig
	{
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;
		Rig(CellFieldRegistry& cf, const Dia::ValueSchema::ValueSchemaService* s, CellLayerViewerDomain::ViewFn view = CellLayerViewerDomain::ViewFn())
			: registry(s, &FakePuIdFn), uploader(cf, s), domain(registry, uploader, s, view) {}
		void Add(const CellLayerDesc& d) { ASSERT_EQ(registry.Register(d), CellRegisterResult::Ok); }
	};

	double CounterNow(const char* name) { auto* c = MetricRegistry::Instance().FindCounter(StringCRC(name)); return c ? static_cast<double>(c->Value()) : 0.0; }

	void PointAt(FakeDebugDraw& draw, int q, int r, float dx = 0.0f, float dy = 0.0f)
	{
		const Dia::Maths::Vector2D c = Dia::Pathfinding::HexTopology::CellCenter(Dia::Pathfinding::CellCoord{ q, r }, 1.0f);
		float sx, sy;
		WorldToScreen(TestView(), c.X() + dx, c.Y() + dy, sx, sy);
		draw.mouse = Dia::Maths::Vector2D(sx, sy);
	}

	std::set<std::string> Keys(const Json::Value& v) { std::set<std::string> k; for (const auto& n : v.getMemberNames()) { k.insert(n); } return k; }
}

TEST_F(ViewerEdgeTest, PanelBaseCommandWithBadArgsChangesNothing)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	rig.domain.Update(draw);
	Json::Value none(Json::objectValue), num(Json::objectValue), unknown(Json::objectValue);
	num["id"] = 5;
	unknown["id"] = "nope";
	rig.domain.OnCommand(StringCRC("base"), none);
	rig.domain.OnCommand(StringCRC("base"), num);
	rig.domain.OnCommand(StringCRC("base"), unknown);
	rig.domain.OnCommand(StringCRC("base"), Json::Value());
	rig.domain.OnCommand(StringCRC("mystery"), unknown);
	rig.domain.OnCommand(StringCRC("setScale"), unknown);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("temp"));
}

TEST_F(ViewerEdgeTest, PanelBaseCommandIgnoresExtraFields)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	Json::Value a(Json::objectValue);
	a["id"] = "rain";
	a["junk"] = 12;
	rig.domain.OnCommand(StringCRC("base"), a);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("rain"));
}

TEST_F(ViewerEdgeTest, PanelGridCommandBadArgsChangeNothing)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	rig.domain.Update(draw);
	Json::Value none(Json::objectValue), str(Json::objectValue), big(Json::objectValue);
	str["index"] = "1";
	big["index"] = 99;
	rig.domain.OnCommand(StringCRC("grid"), none);
	rig.domain.OnCommand(StringCRC("grid"), str);
	rig.domain.OnCommand(StringCRC("grid"), big);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("temp"));
	Json::Value ok(Json::objectValue);
	ok["index"] = 1;
	rig.domain.OnCommand(StringCRC("grid"), ok);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("soil"));
}

TEST_F(ViewerEdgeTest, PanelGridCommandNegativeIndexDoesNotThrow)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.domain.Update(draw);
	Json::Value neg(Json::objectValue);
	neg["index"] = -1;
	EXPECT_NO_THROW(rig.domain.OnCommand(StringCRC("grid"), neg));
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("temp"));
}

TEST_F(ViewerEdgeTest, PanelToggleCommandBadArgsChangeNothing)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	ASSERT_EQ(rig.registry.RegisterOverlay(MakeOverlay("ov")), CellRegisterResult::Ok);
	Json::Value none(Json::objectValue), num(Json::objectValue), unknown(Json::objectValue);
	num["drawer"] = 3;
	unknown["drawer"] = "nope";
	rig.domain.OnCommand(StringCRC("toggle"), none);
	rig.domain.OnCommand(StringCRC("toggle"), num);
	rig.domain.OnCommand(StringCRC("toggle"), unknown);
	EXPECT_FALSE(rig.domain.IsOverlayEnabled(StringCRC("ov")));
	EXPECT_FALSE(rig.domain.IsOverlayEnabled(StringCRC("nope")));
}

TEST_F(ViewerEdgeTest, ToggleTwiceReturnsToDisabledAndStopsDrawing)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	static uint32_t draws;
	draws = 0;
	CellOverlayDesc o = MakeOverlay("ov");
	o.draw = fastdelegate::FastDelegate2<Dia::Core::IDebugDraw&, const void*>([](Dia::Core::IDebugDraw&, const void* sel) { if (sel == nullptr) { ++draws; } });
	ASSERT_EQ(rig.registry.RegisterOverlay(o), CellRegisterResult::Ok);
	EXPECT_TRUE(rig.domain.ToggleOverlay(StringCRC("ov")));
	rig.domain.Update(draw);
	EXPECT_EQ(draws, 1u);
	EXPECT_TRUE(rig.domain.ToggleOverlay(StringCRC("ov")));
	EXPECT_FALSE(rig.domain.IsOverlayEnabled(StringCRC("ov")));
	rig.domain.Update(draw);
	EXPECT_EQ(draws, 1u);
}

TEST_F(ViewerEdgeTest, EveryOverlayCanBeToggledUpToCapacity)
{
	Rig rig(cellField, nullptr);
	for (uint32_t i = 0; i < CellLayerRegistry::kMaxOverlays; ++i)
	{
		char id[16]; snprintf(id, sizeof(id), "o%u", i);
		ASSERT_EQ(rig.registry.RegisterOverlay(MakeOverlay(id)), CellRegisterResult::Ok);
		EXPECT_TRUE(rig.domain.ToggleOverlay(StringCRC(id)));
		EXPECT_TRUE(rig.domain.IsOverlayEnabled(StringCRC(id)));
	}
}

TEST_F(ViewerEdgeTest, SetGridBoundaries)
{
	Rig rig(cellField, nullptr);
	EXPECT_FALSE(rig.domain.SetGrid(0));	// no layers yet
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	EXPECT_FALSE(rig.domain.SetGrid(2));
	EXPECT_FALSE(rig.domain.SetGrid(0xFFFFFFFFu));
	EXPECT_TRUE(rig.domain.SetGrid(1));
	EXPECT_EQ(rig.domain.ActiveGrid(), &gridB.desc);
	EXPECT_TRUE(rig.domain.SetGrid(1));
}

TEST_F(ViewerEdgeTest, SetBaseOfAlreadyActiveBaseSucceedsWithoutExtraReads)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.domain.Update(draw);
	EXPECT_TRUE(rig.domain.SetBase(StringCRC("temp")));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.uploader.Reads(), 1u);
}

TEST_F(ViewerEdgeTest, RecoversFromEmptyStateWhenLayerReturnsOnSameGrid)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.domain.Update(draw);
	rig.registry.Unregister(StringCRC("temp"));
	rig.domain.Update(draw);
	ASSERT_TRUE(rig.domain.IsEmptyState());
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.domain.Update(draw);
	EXPECT_FALSE(rig.domain.IsEmptyState());
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("temp"));
}

TEST_F(ViewerEdgeTest, EmptyFromTheStartThenFirstLayerBecomesActive)
{
	Rig rig(cellField, nullptr);
	rig.domain.Update(draw);
	rig.domain.Update(draw);
	EXPECT_TRUE(rig.domain.IsEmptyState());
	EXPECT_EQ(draw.cellFieldRequests, 0u);
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("soil"));
	EXPECT_EQ(draw.cellFieldRequests, 1u);
}

// BUG (production): after the active grid empties, mActiveGrid stays on it, so ResolveBase never looks at other grids.
// Layers on another grid are then unreachable from the panel (layers[] is empty; gridSelector hidden with a single grid).
// Repro: register temp on gridA, Update, Unregister temp, Update (empty), register soil on gridB, Update -> still empty.
TEST_F(ViewerEdgeTest, EmptyStateThenLayerOnOtherGridIsAdopted)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.domain.Update(draw);
	rig.registry.Unregister(StringCRC("temp"));
	rig.domain.Update(draw);
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	rig.domain.Update(draw);
	EXPECT_FALSE(rig.domain.IsEmptyState());
}

TEST_F(ViewerEdgeTest, AdoptedGridShowsItsLayersInPanelJson)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.domain.Update(draw);
	rig.registry.Unregister(StringCRC("temp"));
	rig.domain.Update(draw);
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("soil"));
	EXPECT_EQ(rig.domain.ActiveGrid(), &gridB.desc);
	Json::Value j;
	rig.domain.GetJSONState(j);
	EXPECT_FALSE(j["empty"].asBool());
	ASSERT_EQ(j["layers"].size(), 1u);
	EXPECT_EQ(j["layers"][0u]["id"].asString(), "soil");
}

TEST_F(ViewerEdgeTest, EmptiedActiveGridAdoptsFirstOfTwoCandidateGrids)
{
	TestGrid gridC{ 3, 3 };
	StubProvider other;
	other.values = std::vector<float>(9, 2.0f);
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	rig.Add(other.MakeDesc("other", &gridC.desc));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("temp"));
	rig.registry.Unregister(StringCRC("temp"));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("soil"));
	EXPECT_EQ(rig.domain.ActiveGrid(), &gridB.desc);
}

TEST_F(ViewerEdgeTest, UserChosenGridWithLayersIsNeverOverridden)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	rig.domain.Update(draw);
	ASSERT_TRUE(rig.domain.SetBase(StringCRC("rain")));
	rig.registry.Unregister(StringCRC("rain"));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("temp"));
	EXPECT_EQ(rig.domain.ActiveGrid(), &gridA.desc);
	ASSERT_TRUE(rig.domain.SetGrid(1));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("soil"));
}

TEST_F(ViewerEdgeTest, AllGridsEmptyKeepsEmptyState)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.domain.Update(draw);
	rig.registry.Unregister(StringCRC("temp"));
	rig.domain.Update(draw);
	rig.domain.Update(draw);
	EXPECT_TRUE(rig.domain.IsEmptyState());
}

TEST_F(ViewerEdgeTest, RepeatedUpdateWithNoChangesDoesNoReadOrUpload)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.domain.Update(draw);
	for (int i = 0; i < 300; ++i) { rig.domain.Update(draw); }
	EXPECT_EQ(rig.uploader.Reads(), 1u);
	EXPECT_EQ(rig.uploader.Uploads(), 1u);
	EXPECT_EQ(rig.uploader.SkippedFrames(), 300u);
	EXPECT_EQ(draw.cellFieldRequests, 301u);
}

TEST_F(ViewerEdgeTest, MetricDeltasMatchUploaderOverRandomFrames)
{
	Rig rig(cellField, nullptr);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	const double r0 = CounterNow("dia.celllayers.viewer.reads"), u0 = CounterNow("dia.celllayers.viewer.uploads"), s0 = CounterNow("dia.celllayers.viewer.skipped_frames");
	std::mt19937 rng(5);
	for (int i = 0; i < 500; ++i)
	{
		if (rng() % 5 == 0) { temp.versionValue++; }
		if (rng() % 7 == 0) { rig.domain.SetBase(StringCRC(rng() % 2 ? "rain" : "temp")); }
		rig.domain.Update(draw);
	}
	EXPECT_EQ(CounterNow("dia.celllayers.viewer.reads") - r0, static_cast<double>(rig.uploader.Reads()));
	EXPECT_EQ(CounterNow("dia.celllayers.viewer.uploads") - u0, static_cast<double>(rig.uploader.Uploads()));
	EXPECT_EQ(CounterNow("dia.celllayers.viewer.skipped_frames") - s0, static_cast<double>(rig.uploader.SkippedFrames()));
}

TEST_F(ViewerEdgeTest, HoverReportsEveryCellCentreWithItsValue)
{
	Rig rig(cellField, nullptr, CellLayerViewerDomain::ViewFn(&ViewFn0));
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	for (int r = 0; r < 3; ++r)
	{
		for (int q = 0; q < 4; ++q)
		{
			PointAt(draw, q, r);
			rig.domain.Update(draw);
			const auto h = rig.domain.Hover();
			ASSERT_TRUE(h.valid) << q << "," << r;
			EXPECT_EQ(h.q, q);
			EXPECT_EQ(h.r, r);
			EXPECT_EQ(h.value, temp.values[r * 4 + q]);
			EXPECT_EQ(h.layer, StringCRC("temp"));
		}
	}
}

TEST_F(ViewerEdgeTest, HoverJustInsideCellEdgesPicksThatCellAndOutsideGridClears)
{
	Rig rig(cellField, nullptr, CellLayerViewerDomain::ViewFn(&ViewFn0));
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	PointAt(draw, 2, 1, 0.9f, 0.0f);
	rig.domain.Update(draw);
	EXPECT_TRUE(rig.domain.Hover().valid);
	EXPECT_EQ(rig.domain.Hover().q, 2);
	PointAt(draw, 2, 1, 0.0f, -0.85f);
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.Hover().r, 1);
	PointAt(draw, 10, 10);
	rig.domain.Update(draw);
	EXPECT_FALSE(rig.domain.Hover().valid);
	draw.mouse = Dia::Maths::Vector2D(-5000.0f, -5000.0f);
	rig.domain.Update(draw);
	EXPECT_FALSE(rig.domain.Hover().valid);
}

TEST_F(ViewerEdgeTest, HoverWithoutViewFnOrWithNonFiniteViewIsNeverValid)
{
	Rig noView(cellField, nullptr);
	noView.Add(temp.MakeDesc("temp", &gridA.desc));
	PointAt(draw, 1, 1);
	noView.domain.Update(draw);
	EXPECT_FALSE(noView.domain.Hover().valid);
	Json::Value j;
	noView.domain.GetJSONState(j);
	EXPECT_FALSE(j["hover"]["valid"].asBool());

	CellFieldRegistry cf2;
	Rig nanView(cf2, nullptr, CellLayerViewerDomain::ViewFn(&NaNView));
	StubProvider t2;
	t2.values = temp.values;
	nanView.Add(t2.MakeDesc("temp", &gridA.desc));
	nanView.domain.Update(draw);
	EXPECT_FALSE(nanView.domain.Hover().valid);
}

TEST_F(ViewerEdgeTest, HoverFollowsBaseSwitchAndClearsWhenEmpty)
{
	Rig rig(cellField, nullptr, CellLayerViewerDomain::ViewFn(&ViewFn0));
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	PointAt(draw, 1, 0);
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.Hover().value, -2.0f);
	rig.domain.SetBase(StringCRC("rain"));
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.Hover().value, 1.0f);
	EXPECT_EQ(rig.domain.Hover().layer, StringCRC("rain"));
	rig.registry.Unregister(StringCRC("rain"));
	rig.registry.Unregister(StringCRC("temp"));
	rig.domain.Update(draw);
	EXPECT_FALSE(rig.domain.Hover().valid);
}

TEST_F(ViewerEdgeTest, CategoryHoverTextFallsBackToIdThenQuestionMark)
{
	fx.Add(UploaderSchemaFixture::EnumEntry("biome", 3));
	const auto& svc = fx.Publish();
	Rig rig(cellField, &svc, CellLayerViewerDomain::ViewFn(&ViewFn0));
	rig.Add(biome.MakeDesc("biome", &gridA.desc, CellValueKind::Category, "cells/biome"));
	const struct { int q, r; const char* text; } cases[] = { {0, 0, "v0"}, {1, 0, "v1"}, {2, 0, "v2"}, {3, 0, "?"}, {0, 1, "?"} };
	for (const auto& c : cases)
	{
		PointAt(draw, c.q, c.r);
		rig.domain.Update(draw);
		Json::Value j;
		rig.domain.GetJSONState(j);
		ASSERT_TRUE(j["hover"]["valid"].asBool()) << c.q << "," << c.r;
		EXPECT_STREQ(j["hover"]["text"].asCString(), c.text) << c.q << "," << c.r;
	}
}

TEST_F(ViewerEdgeTest, CategoryHoverWithoutSchemaShowsQuestionMark)
{
	Rig rig(cellField, nullptr, CellLayerViewerDomain::ViewFn(&ViewFn0));
	rig.Add(biome.MakeDesc("biome", &gridA.desc, CellValueKind::Category));
	PointAt(draw, 1, 0);
	rig.domain.Update(draw);
	Json::Value j;
	rig.domain.GetJSONState(j);
	EXPECT_STREQ(j["hover"]["text"].asCString(), "?");
}

TEST_F(ViewerEdgeTest, JSONTopLevelKeySetIsStableAcrossStates)
{
	Rig rig(cellField, nullptr, CellLayerViewerDomain::ViewFn(&ViewFn0));
	Json::Value empty;
	rig.domain.Update(draw);
	rig.domain.GetJSONState(empty);
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	rig.domain.Update(draw);
	Json::Value full;
	rig.domain.GetJSONState(full);
	std::set<std::string> base = Keys(empty);
	for (const char* k : { "empty", "activeBase", "grids", "layers", "groups", "overlays", "drawers", "stats", "legend", "hover" })
	{
		EXPECT_TRUE(base.count(k)) << k << " missing in empty state";
		EXPECT_TRUE(Keys(full).count(k)) << k << " missing in populated state";
	}
	EXPECT_TRUE(empty["empty"].asBool());
	EXPECT_FALSE(full["empty"].asBool());
	EXPECT_FALSE(empty.isMember("gridSelector"));
	EXPECT_TRUE(full.isMember("gridSelector"));
	EXPECT_FALSE(empty.isMember("noData"));
	EXPECT_EQ(empty["layers"].size(), 0u);
	EXPECT_FALSE(empty["legend"]["available"].asBool());
}

TEST_F(ViewerEdgeTest, JSONRebuildsHotkeysWhenRegistryChanges)
{
	fx.Add([] { auto e = UploaderSchemaFixture::FloatEntry("a", 0, 1); e.mUi["hotkey"] = "q"; return e; }());
	fx.Add([] { auto e = UploaderSchemaFixture::FloatEntry("b", 0, 1); e.mUi["hotkey"] = "w"; return e; }());
	const auto& svc = fx.Publish();
	Rig rig(cellField, &svc);
	rig.Add(temp.MakeDesc("a", &gridA.desc, CellValueKind::Float, "cells/a"));
	rig.domain.Update(draw);
	Json::Value j;
	rig.domain.GetJSONState(j);
	EXPECT_STREQ(j["layers"][0]["hotkey"].asCString(), "q");
	rig.Add(rain.MakeDesc("b", &gridA.desc, CellValueKind::Float, "cells/b"));
	rig.domain.GetJSONState(j);
	ASSERT_EQ(j["layers"].size(), 2u);
	EXPECT_STREQ(j["layers"][1]["hotkey"].asCString(), "w");
}

TEST_F(ViewerEdgeTest, HealthDegradesRecoversAndIgnoresCreatePending)
{
	Rig rig(cellField, nullptr);
	CellLayerDesc broken = temp.MakeDesc("temp", &gridA.desc);
	broken.read = fastdelegate::FastDelegate2<float*, uint32_t>();
	rig.Add(broken);
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.HealthReporter().Report().status, HealthStatus::kDegraded);
	rig.registry.Unregister(StringCRC("temp"));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	Dia::Core::CellFieldLayerHandle fillers[CellFieldRegistry::kMaxLayers];
	for (auto& f : fillers) { ASSERT_TRUE(cellField.CreateLayer(gridA.desc, Dia::Graphics::CellFieldValueKind::Float, f).ok); }
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.HealthReporter().Report().status, HealthStatus::kDegraded);
	cellField.Destroy(fillers[0]);
	RecordingSink sink;
	cellField.Drain(sink);
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.HealthReporter().Report().status, HealthStatus::kOK);
}

TEST_F(ViewerEdgeTest, HealthStartsOKWithNoLayers)
{
	Rig rig(cellField, nullptr);
	rig.domain.Update(draw);
	EXPECT_EQ(rig.domain.HealthReporter().Report().status, HealthStatus::kOK);
}

TEST_F(ViewerEdgeTest, RegisterCommandsIsIdempotentAndDiaAPICommandsHandleBadArgs)
{
	Dia::API::Initialize();
	{
		Rig rig(cellField, nullptr);
		rig.Add(temp.MakeDesc("temp", &gridA.desc));
		rig.Add(rain.MakeDesc("rain", &gridA.desc));
		ASSERT_EQ(rig.registry.RegisterOverlay(MakeOverlay("ov")), CellRegisterResult::Ok);
		rig.domain.RegisterCommands();
		rig.domain.RegisterCommands();
		rig.domain.Update(draw);
		Dia::API::CommandArgs none;
		EXPECT_EQ(Dia::API::ExecuteCommand(StringCRC("cells.layer.base"), none), 2);
		EXPECT_EQ(Dia::API::ExecuteCommand(StringCRC("cells.overlay.toggle"), none), 2);
		Dia::API::CommandArgs extra;
		extra.positionalArgs.Add("rain");
		extra.positionalArgs.Add("ignored");
		EXPECT_EQ(Dia::API::ExecuteCommand(StringCRC("cells.layer.base"), extra), 0);
		EXPECT_EQ(rig.domain.ActiveBase(), StringCRC("rain"));
		Dia::API::CommandArgs ov;
		ov.positionalArgs.Add("ov");
		EXPECT_EQ(Dia::API::ExecuteCommand(StringCRC("cells.overlay.toggle"), ov), 0);
		EXPECT_EQ(Dia::API::ExecuteCommand(StringCRC("cells.overlay.toggle"), ov), 0);
		EXPECT_FALSE(rig.domain.IsOverlayEnabled(StringCRC("ov")));
		rig.domain.UnregisterCommands();
		rig.domain.UnregisterCommands();
		EXPECT_EQ(Dia::API::GetCommand(StringCRC("cells.layer.list")), nullptr);
	}
	Dia::API::Shutdown();
}

TEST_F(ViewerEdgeTest, ListLayersOnEmptyRegistryHasEmptyArrays)
{
	Rig rig(cellField, nullptr);
	Json::Value out;
	rig.domain.ListLayers(out);
	EXPECT_EQ(out["grids"].size(), 0u);
	EXPECT_EQ(out["overlays"].size(), 0u);
	EXPECT_EQ(out["activeBase"].asString(), "");
	EXPECT_EQ(out["activeGridIndex"].asInt(), -1);
}

TEST_F(ViewerEdgeTest, ConcurrentCommandsJsonAndUpdatesStayConsistent)
{
	Rig rig(cellField, nullptr, CellLayerViewerDomain::ViewFn(&ViewFn0));
	rig.Add(temp.MakeDesc("temp", &gridA.desc));
	rig.Add(rain.MakeDesc("rain", &gridA.desc));
	rig.Add(soil.MakeDesc("soil", &gridB.desc));
	ASSERT_EQ(rig.registry.RegisterOverlay(MakeOverlay("ov")), CellRegisterResult::Ok);
	rig.domain.Update(draw);
	std::atomic<bool> stop{ false };
	std::atomic<uint32_t> bad{ 0 };
	std::thread updater([&] { FakeDebugDraw d; uint32_t n = 0; while (!stop) { rig.domain.Update(d); if ((++n % 50) == 0) { temp.versionValue++; } } });
	auto commander = [&](uint32_t seed)
	{
		std::mt19937 rng(seed);
		const char* ids[] = { "temp", "rain", "soil", "nope" };
		for (int i = 0; i < 1500; ++i)
		{
			switch (rng() % 5)
			{
			case 0: rig.domain.SetBase(StringCRC(ids[rng() % 4])); break;
			case 1: rig.domain.SetGrid(rng() % 3); break;
			case 2: rig.domain.ToggleOverlay(StringCRC("ov")); break;
			case 3: { Json::Value j; rig.domain.GetJSONState(j); if (!j.isMember("layers") || !j.isMember("legend")) { ++bad; } break; }
			default: { Json::Value l; rig.domain.ListLayers(l); if (l["grids"].size() != 2u) { ++bad; } break; }
			}
		}
	};
	std::thread c1(commander, 1), c2(commander, 2), c3(commander, 3);
	c1.join(); c2.join(); c3.join();
	stop = true;
	updater.join();
	EXPECT_EQ(bad.load(), 0u);
	const StringCRC active = rig.domain.ActiveBase();
	ASSERT_NE(active, StringCRC::kZero);
	const CellLayerDesc* l = rig.registry.Find(active);
	ASSERT_NE(l, nullptr);
	EXPECT_EQ(rig.domain.ActiveGrid(), l->grid);
	EXPECT_EQ(rig.uploader.Uploads(), rig.uploader.Reads());
}

#endif
