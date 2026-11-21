#include "HistoryTestRig.h"
#include <DiaCellHistory/CellHistoryCommands.h>
#include <DiaCellHistory/CellHistoryDomain.h>
#include <DiaCellHistory/DeltaCardState.h>
#include <DiaCellHistory/DeltaCellInfo.h>
#include <DiaCellHistory/HistoryValueSource.h>
#include <DiaCellHistory/HistoryViewCommands.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/TimelineCardState.h>
#include <DiaCellHistory/ViewHistorySourceAdapter.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/InspectorModel.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/LayerUploader.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/ViewStatePublisher.h>
#include <DiaCellLayers/Testing/CellPickerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaCellLayers/Testing/ViewStateTestSupport.h>
#include <DiaCellLayers/Testing/ViewerCommands.h>
#include <DiaAPI/DiaAPI.h>
#include <DiaPathfinding/HexTopology.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

// Delta mode through the REAL DiaCellLayers consumers (Feature 4 task 4.8). Rig: 4x3 grid (12 cells), layers
//   temp  (float 0..100, tracked): sample i -> 10*i + c;           live 70 + c
//   veg   (float 0..100, tracked): sample 0 -> 50 + c; samples 1..4 -> 50 + c - (c < 4 ? 20 : 0);   live 60 + c
//   biome (category, history.group, tracked): c % 3 in every sample; live 2
//   rain  (float, NOT tracked): live 9
// Recorded values are quantised to the schema range (<= 0.002 for the 0..100 floats), so deltas are compared with a tolerance.

using namespace Dia::CellHistory;
using namespace Dia::CellHistory::Testing;
using namespace HistoryRig;
using namespace Dia::CellLayers;
using Dia::CellLayers::Testing::FakeDebugDraw;
using Dia::CellLayers::Testing::FakeMirrorRegistrar;
using Dia::CellLayers::Testing::ViewerCommands;
using Dia::Core::StringCRC;

namespace
{
	CellPickerView TestView() { CellPickerView v; v.viewportWidth = 1280; v.viewportHeight = 720; return v; }
	CellPickerView ViewFn0() { return TestView(); }

	bool DecodeFloats(const Json::Value& r, std::vector<float>& out)
	{
		out.clear();
		if (!r.isObject() || !r["data"].isString()) { return false; }
		const std::string text = r["data"].asString();
		auto val = [](char c) -> int
		{
			if (c >= 'A' && c <= 'Z') { return c - 'A'; }
			if (c >= 'a' && c <= 'z') { return c - 'a' + 26; }
			if (c >= '0' && c <= '9') { return c - '0' + 52; }
			if (c == '+') { return 62; }
			if (c == '/') { return 63; }
			return 0;
		};
		std::vector<uint8_t> bytes;
		for (size_t i = 0; i + 3 < text.size(); i += 4)
		{
			uint32_t v = 0;
			int pad = 0;
			for (int k = 0; k < 4; ++k)
			{
				const char c = text[i + k];
				if (c == '=') { ++pad; v <<= 6; }
				else { v = (v << 6) | static_cast<uint32_t>(val(c)); }
			}
			bytes.push_back(static_cast<uint8_t>(v >> 16));
			if (pad < 2) { bytes.push_back(static_cast<uint8_t>(v >> 8)); }
			if (pad < 1) { bytes.push_back(static_cast<uint8_t>(v)); }
		}
		out.resize(bytes.size() / 4);
		for (size_t i = 0; i < out.size(); ++i) { std::memcpy(&out[i], &bytes[i * 4], 4); }
		return true;
	}

	Json::Value LayerArgs(const char* layer) { Json::Value a(Json::objectValue); a["layer"] = layer; return a; }
	Json::Value ExprArgs(const char* e) { Json::Value a(Json::objectValue); a["expr"] = e; return a; }
	Json::Value StatsArgs(const char* layer)
	{
		Json::Value a(Json::objectValue);
		Json::Value l(Json::arrayValue);
		l.append(layer);
		a["layers"] = l;
		return a;
	}
	Side SampleSide(uint32_t i) { Side s; s.kind = SideKind::Sample; s.sample = i; return s; }
	Side NowSide() { return Side(); }

	struct DeltaViewIntegration : ::testing::Test
	{
		void SetUp() override { Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};

	struct DeltaRig : Rig
	{
		static constexpr uint32_t kSamples = 5;
		Dia::Graphics::CellFieldRegistry cellField;
		FakeDebugDraw draw;
		std::unique_ptr<LayerUploader> uploader;
		std::unique_ptr<CellLayerViewerDomain> domain;
		std::unique_ptr<ViewerCommands> viewerCommands;
		std::unique_ptr<MirrorQueries> queries;
		std::unique_ptr<LayerReader> reader;
		std::unique_ptr<HistoryViewState> view;
		std::unique_ptr<HistoryValueSource> source;
		std::unique_ptr<HistoryViewCommands> commands;
		FakeMirrorRegistrar registrar;

		static float TempAt(uint32_t s, uint32_t c) { return 10.0f * s + c; }
		static float VegAt(uint32_t s, uint32_t c) { return 50.0f + c - ((s >= 1 && c < 4) ? 20.0f : 0.0f); }

		void Setup(const char* base = "temp")
		{
			grid.SetTopology("hex.flat.oddq");
			grid.AddFloatLayer("temp", 0.0f);
			grid.AddFloatLayer("veg", 0.0f);
			grid.AddCategoryLayer("biome", 0);
			grid.AddFloatLayer("rain", 5.0f);
			ASSERT_TRUE(schema.Load({ FloatEntry("temp", 0, 100), FloatEntry("veg", 0, 100), EnumEntry("biome", 3, "{\"group\":true}"), FloatEntry("rain", 0, 100) }));
			cfg.mTrackCount = 0;
			cfg.mTrack[cfg.mTrackCount++] = Dia::Core::Containers::String64("%s", "temp");
			cfg.mTrack[cfg.mTrackCount++] = Dia::Core::Containers::String64("%s", "veg");
			cfg.mTrack[cfg.mTrackCount++] = Dia::Core::Containers::String64("%s", "biome");
			Start(nullptr, false);
			for (uint32_t i = 0; i < kSamples; ++i)
			{
				for (uint32_t c = 0; c < 12; ++c)
				{
					grid.Set("temp", c, TempAt(i, c)); grid.Set("veg", c, VegAt(i, c)); grid.Set("biome", c, float(c % 3));
				}
				grid.BumpVersion("temp"); grid.BumpVersion("veg"); grid.BumpVersion("biome");
				Take();
			}
			for (uint32_t c = 0; c < 12; ++c) { grid.Set("temp", c, 70.0f + c); grid.Set("veg", c, 60.0f + c); grid.Set("biome", c, 2.0f); }
			grid.SetAll("rain", 9.0f);
			grid.BumpVersion("temp"); grid.BumpVersion("veg"); grid.BumpVersion("biome"); grid.BumpVersion("rain");

			uploader = std::make_unique<LayerUploader>(cellField, schema.service.get());
			domain = std::make_unique<CellLayerViewerDomain>(registry, *uploader, schema.service.get(), CellLayerViewerDomain::ViewFn(&ViewFn0));
			viewerCommands = std::make_unique<ViewerCommands>(*domain);
			queries = std::make_unique<MirrorQueries>(registry, *domain, schema.service.get());
			reader = std::make_unique<LayerReader>(registry);
			view = std::make_unique<HistoryViewState>(*rec);
			source = std::make_unique<HistoryValueSource>(registry, *rec, *view);
			source->SetSchema(schema.service.get());
			ASSERT_TRUE(source->Install());
			commands = std::make_unique<HistoryViewCommands>(*rec, *view, &cfg);
			commands->SetLayerRegistry(&registry);
			ASSERT_TRUE(commands->Register(registrar));
			ASSERT_TRUE(domain->SetBase(StringCRC(base)));
			Frame();
		}

		void Frame() { source->Tick(); domain->Update(draw); }
		Json::Value Delta(const char* layer, const char* a, const char* b)
		{
			Json::Value args(Json::objectValue); args["layer"] = layer; args["a"] = a; args["b"] = b;
			const Json::Value r = registrar.Execute("cells.history.delta", args);
			Frame();
			return r;
		}
		void DeltaOff()
		{
			Json::Value args(Json::objectValue); args["off"] = true;
			registrar.Execute("cells.history.delta", args);
			Frame();
		}
		std::vector<float> UploaderBase(const char* layer)
		{
			uint32_t n = 0;
			const float* p = uploader->BaseValues(StringCRC(layer), n);
			return p ? std::vector<float>(p, p + n) : std::vector<float>();
		}
		std::vector<float> ReaderValues(const char* layer)
		{
			const LayerValues lv = reader->Read(StringCRC(layer));
			return lv.hasData && lv.values ? std::vector<float>(lv.values, lv.values + lv.count) : std::vector<float>();
		}
		std::vector<float> MirrorValues(const char* layer)
		{
			std::vector<float> out;
			DecodeFloats(queries->LayerValues(LayerArgs(layer)), out);
			return out;
		}
		Json::Value Query(const char* expr) { return ViewerCommands::Exec("cells.query.check", ExprArgs(expr)); }
		uint32_t QueryCount(const char* expr)
		{
			const Json::Value r = Query(expr);
			return r["success"].asBool() ? r["data"]["count"].asUInt() : 9999u;
		}
		void SelectCells(std::initializer_list<uint32_t> cells)
		{
			CellSelection* sel = domain->ActiveSelection();
			ASSERT_NE(sel, nullptr);
			sel->BeginEdit();
			sel->Clear();
			for (uint32_t c : cells) { sel->Set(c, true); }
			sel->EndEdit();
		}
		Json::Value StatsLayer(const char* layer)
		{
			const Json::Value r = queries->SelectionStats(StatsArgs(layer));
			if (!r.isObject() || !r["layers"].isArray() || r["layers"].size() != 1u) { ADD_FAILURE() << r.toStyledString(); return Json::Value(); }
			return r["layers"][0u];
		}
		const Dia::Graphics::CellGridDesc* GridDesc() { return &grid.Grid(); }
	};

	void ExpectAll(const std::vector<float>& v, float expected, float tol)
	{
		ASSERT_EQ(v.size(), 12u);
		for (uint32_t c = 0; c < 12; ++c) { EXPECT_NEAR(v[c], expected, tol) << c; }
	}
}

TEST_F(DeltaViewIntegration, UploaderReaderMirrorAndLayerValuesCarryTheDeltas)
{
	DeltaRig r; r.Setup();
	ASSERT_TRUE(r.Delta("temp", "sample:0", "sample:3")["ok"].asBool());
	ExpectAll(r.UploaderBase("temp"), 30.0f, 0.01f);
	ExpectAll(r.ReaderValues("temp"), 30.0f, 0.01f);
	ExpectAll(r.MirrorValues("temp"), 30.0f, 0.01f);	// cells.layer.values
	// Every float layer shows its own delta, not just the base.
	const std::vector<float> veg = r.ReaderValues("veg");
	ASSERT_EQ(veg.size(), 12u);
	EXPECT_NEAR(veg[0], -20.0f, 0.01f);	// veg dropped by 20 on cells 0..3 between samples 0 and 1
	EXPECT_NEAR(veg[5], 0.0f, 0.01f);
	EXPECT_EQ(r.registry.IsLive(), false);

	r.DeltaOff();
	EXPECT_TRUE(r.registry.IsLive());
	std::vector<float> live(12);
	for (uint32_t c = 0; c < 12; ++c) { live[c] = 70.0f + c; }
	EXPECT_EQ(r.UploaderBase("temp"), live);
	EXPECT_EQ(r.MirrorValues("temp"), live);
}

TEST_F(DeltaViewIntegration, NowSideDeltaUsesLiveValuesAndFollowsThem)
{
	DeltaRig r; r.Setup();
	ASSERT_TRUE(r.Delta("temp", "sample:2", "now")["ok"].asBool());
	ExpectAll(r.MirrorValues("temp"), 50.0f, 0.01f);	// (70 + c) - (20 + c)
	for (uint32_t c = 0; c < 12; ++c) { r.grid.Set("temp", c, 100.0f); }
	r.grid.BumpVersion("temp");
	r.Frame();
	const std::vector<float> v = r.MirrorValues("temp");
	ASSERT_EQ(v.size(), 12u);
	EXPECT_NEAR(v[3], 100.0f - 23.0f, 0.01f);
}

TEST_F(DeltaViewIntegration, HoverOverAnUnchangedCellShowsZero)
{
	DeltaRig r; r.Setup("veg");
	const Dia::Maths::Vector2D c = Dia::Pathfinding::HexTopology::CellCenter(Dia::Pathfinding::CellCoord{ 2, 1 }, 1.0f);
	float sx, sy;
	Dia::CellLayers::Testing::WorldToScreen(TestView(), c.X(), c.Y(), sx, sy);
	r.draw.mouse = Dia::Maths::Vector2D(sx, sy);

	ASSERT_TRUE(r.Delta("veg", "sample:0", "sample:1")["ok"].asBool());
	const CellLayerViewerDomain::HoverInfo h = r.domain->Hover();
	ASSERT_TRUE(h.valid);
	EXPECT_EQ(h.q, 2); EXPECT_EQ(h.r, 1);
	EXPECT_NEAR(h.value, 0.0f, 0.01f) << "cell 6 is c >= 4: no change";
}

TEST_F(DeltaViewIntegration, HoverOverAChangedCellShowsTheNegativeDelta)
{
	DeltaRig r; r.Setup("veg");
	const Dia::Maths::Vector2D c = Dia::Pathfinding::HexTopology::CellCenter(Dia::Pathfinding::CellCoord{ 1, 0 }, 1.0f);
	float sx, sy;
	Dia::CellLayers::Testing::WorldToScreen(TestView(), c.X(), c.Y(), sx, sy);
	r.draw.mouse = Dia::Maths::Vector2D(sx, sy);
	ASSERT_TRUE(r.Delta("veg", "sample:0", "sample:1")["ok"].asBool());
	const CellLayerViewerDomain::HoverInfo h = r.domain->Hover();
	ASSERT_TRUE(h.valid);
	EXPECT_EQ(h.q, 1); EXPECT_EQ(h.r, 0);
	EXPECT_NEAR(h.value, -20.0f, 0.01f);
	Json::Value state;
	r.domain->GetJSONState(state);
	EXPECT_NEAR(state["hover"]["value"].asDouble(), -20.0, 0.01);
}

TEST_F(DeltaViewIntegration, QueryBelowAThresholdSelectsExactlyTheCellsWhoseDeltaIsBelowIt)
{
	DeltaRig r; r.Setup();
	ASSERT_TRUE(r.Delta("veg", "sample:0", "sample:1")["ok"].asBool());
	EXPECT_EQ(r.QueryCount("veg < -0.1"), 4u);
	EXPECT_EQ(r.QueryCount("veg > -0.1"), 8u);
	EXPECT_EQ(r.QueryCount("veg < -19.9"), 4u);
	EXPECT_EQ(r.QueryCount("veg < -20.1"), 0u);
	// Other float layers are deltas too: temp changed by +10 everywhere.
	EXPECT_EQ(r.QueryCount("temp > 9.9"), 12u);
	EXPECT_EQ(r.QueryCount("temp > 10.1"), 0u);
	r.DeltaOff();
	EXPECT_EQ(r.QueryCount("veg < -0.1"), 0u) << "back to live values (60..71)";
}

TEST_F(DeltaViewIntegration, CategoryLayerInAQueryReportsNoDataInThisView)
{
	DeltaRig r; r.Setup();
	ASSERT_TRUE(r.Delta("veg", "sample:0", "sample:1")["ok"].asBool());
	const Json::Value q = r.Query("biome == b");
	EXPECT_FALSE(q["success"].asBool()) << q.toStyledString();
	EXPECT_NE(q.toStyledString().find("no data for layer"), std::string::npos) << "the query contract's no-data message: " << q.toStyledString();
	// The select-by-category command words the same condition "has no data in this view".
	Json::Value cat(Json::objectValue); cat["layer"] = "biome"; cat["value"] = "b";
	const Json::Value c = ViewerCommands::Exec("cells.select.category", cat);
	EXPECT_FALSE(c["success"].asBool()) << c.toStyledString();
	EXPECT_NE(c.toStyledString().find("no data in this view"), std::string::npos) << c.toStyledString();
	EXPECT_TRUE(r.ReaderValues("biome").empty());
}

TEST_F(DeltaViewIntegration, InspectorStatsAreOverTheDeltaAndUpdateWhenTheDeltaChanges)
{
	DeltaRig r; r.Setup("veg");
	r.SelectCells({ 0, 1, 5 });
	ASSERT_TRUE(r.Delta("veg", "sample:0", "sample:1")["ok"].asBool());
	Json::Value s = r.StatsLayer("veg");
	EXPECT_TRUE(s["hasData"].asBool());
	EXPECT_EQ(s["count"].asUInt(), 3u);
	EXPECT_NEAR(s["min"].asDouble(), -20.0, 0.01);
	EXPECT_NEAR(s["max"].asDouble(), 0.0, 0.01);
	EXPECT_NEAR(s["mean"].asDouble(), -40.0 / 3.0, 0.01);

	// The panel card (cached stats) follows: one Tick detects the change, the next runs the pass.
	r.Frame(); r.Frame();
	Json::Value state;
	r.domain->GetJSONState(state);
	const Json::Value& card = state["inspector"]["layers"][0u];
	EXPECT_EQ(card["id"].asString(), "veg");
	EXPECT_NEAR(card["mean"].asDouble(), -40.0 / 3.0, 0.01);

	// Change the delta: sample 1 vs sample 2 has no change at all.
	ASSERT_TRUE(r.Delta("veg", "sample:1", "sample:2")["ok"].asBool());
	s = r.StatsLayer("veg");
	EXPECT_NEAR(s["mean"].asDouble(), 0.0, 0.01);
	EXPECT_NEAR(s["min"].asDouble(), 0.0, 0.01);
	EXPECT_NEAR(s["max"].asDouble(), 0.0, 0.01);
	r.Frame(); r.Frame();
	Json::Value state2;
	r.domain->GetJSONState(state2);
	EXPECT_NEAR(state2["inspector"]["layers"][0u]["mean"].asDouble(), 0.0, 0.01) << "inspector SourceKey/epoch dirty check";

	// Changing only the scale (an epoch bump with identical values) must not leave a stale card either.
	r.DeltaOff();
	s = r.StatsLayer("veg");
	EXPECT_NEAR(s["mean"].asDouble(), (60.0 + 61.0 + 65.0) / 3.0, 0.01);
}

TEST_F(DeltaViewIntegration, ALayerWithNoDataOnASideIsNoDataNeverLive)
{
	DeltaRig r; r.Setup("veg");
	r.SelectCells({ 0, 1, 2 });
	ViewResult vr;
	// rain is not tracked: no recorded data on a sample side. now vs sample:1 must NOT fall back to live for the missing side.
	ASSERT_TRUE(r.view->EnterDelta(StringCRC("veg"), NowSide(), SampleSide(1), vr));
	r.Frame();
	EXPECT_TRUE(r.ReaderValues("rain").empty());
	const Json::Value rain = r.StatsLayer("rain");
	ASSERT_TRUE(rain.isObject());
	EXPECT_FALSE(rain["hasData"].asBool());
	EXPECT_TRUE(r.StatsLayer("veg")["hasData"].asBool());
	const Json::Value mirror = r.queries->LayerValues(LayerArgs("rain"));
	ASSERT_TRUE(mirror.isMember("error"));
	EXPECT_NE(mirror["error"].asString().find("no data"), std::string::npos);
	Json::Value state;
	r.domain->GetJSONState(state);
	bool found = false;
	for (const Json::Value& l : state["inspector"]["layers"])
	{
		if (l["id"].asString() == "rain") { found = true; EXPECT_FALSE(l["hasData"].asBool()); }
	}
	// rain may not be in the shown set; the stats query above is the contract. Do not require it here.
	(void)found;
}

TEST_F(DeltaViewIntegration, HistoryFieldReportsTheDeltaAndTheStampMovesOnEveryChange)
{
	DeltaRig r; r.Setup();
	ViewHistorySourceAdapter adapter(*r.rec, *r.view);
	Dia::CellLayers::Testing::FakeClock clock;
	Dia::CellLayers::Testing::TopicRecorder topic;
	ViewStatePublisher publisher(r.registry, *r.domain, topic, clock);
	publisher.SetHistorySource(&adapter);
	auto tick = [&] { clock.Advance(500); r.Frame(); publisher.Update(); };
	tick();
	const size_t base = topic.Count();
	ASSERT_GE(base, 1u);
	EXPECT_EQ(topic.Last()["history"]["mode"].asString(), "live");
	tick();
	EXPECT_EQ(topic.Count(), base) << "no change, no publish";

	uint64_t stamp = adapter.HistoryStamp();
	size_t count = topic.Count();
	auto expectChanged = [&](const char* what)
	{
		tick();
		EXPECT_NE(adapter.HistoryStamp(), stamp) << what;
		EXPECT_EQ(topic.Count(), count + 1) << what;
		stamp = adapter.HistoryStamp();
		count = topic.Count();
	};

	ASSERT_TRUE(r.Delta("temp", "sample:1", "now")["ok"].asBool());
	expectChanged("enter");
	Json::Value h = topic.Last()["history"];
	EXPECT_EQ(h["mode"].asString(), "delta");
	EXPECT_EQ(h["a"].asString(), "sample:1");
	EXPECT_EQ(h["b"].asString(), "now");
	ASSERT_TRUE(h.isMember("scale"));
	EXPECT_TRUE(h["scale"].isNull());
	EXPECT_EQ(h["layer"].asString(), "temp");
	EXPECT_EQ(h["groupside"].asString(), "B");

	Json::Value sa(Json::objectValue); sa["scale"] = 7.5;
	ASSERT_TRUE(r.registrar.Execute("cells.history.delta", sa)["ok"].asBool());
	expectChanged("scale fixed");
	h = topic.Last()["history"];
	EXPECT_NEAR(h["scale"].asDouble(), 7.5, 1e-6);

	Json::Value sb(Json::objectValue); sb["scale"] = "auto";
	ASSERT_TRUE(r.registrar.Execute("cells.history.delta", sb)["ok"].asBool());
	expectChanged("scale auto");
	EXPECT_TRUE(topic.Last()["history"]["scale"].isNull());

	Json::Value gs(Json::objectValue); gs["groupside"] = "A";
	ASSERT_TRUE(r.registrar.Execute("cells.history.delta", gs)["ok"].asBool());
	expectChanged("groupside");
	EXPECT_EQ(topic.Last()["history"]["groupside"].asString(), "A");

	r.DeltaOff();
	expectChanged("exit");
	EXPECT_EQ(topic.Last()["history"]["mode"].asString(), "live");
}

namespace
{
	struct DomainRig : DeltaRig
	{
		std::unique_ptr<CellHistoryCommands> historyCommands;
		std::unique_ptr<CellHistoryDomain> hdomain;
		int32_t hoverQ = -1, hoverR = -1;

		bool Hover(int32_t& q, int32_t& r) { q = hoverQ; r = hoverR; return hoverQ >= 0; }
		StringCRC Base() { return StringCRC("veg"); }

		void SetupDomain()
		{
			Setup("veg");
			historyCommands = std::make_unique<CellHistoryCommands>(*rec);
			hdomain = std::make_unique<CellHistoryDomain>(*rec, *view, *commands, *historyCommands,
				CellHistoryDomain::BaseLayerFn(this, &DomainRig::Base), schema.service.get(), &cfg);
			hdomain->SetLayerRegistry(&registry);
			hdomain->SetHoverFn(CellHistoryDomain::HoverCellFn(this, &DomainRig::Hover));
			hdomain->Update();
		}
		Json::Value State() { Json::Value s; hdomain->GetJSONState(s); return s; }
		const Json::Value* FindGroup(const Json::Value& groups, const char* layer, int value)
		{
			for (const Json::Value& g : groups) { if (g["layer"].asString() == layer && g["value"].asInt() == value) { return &g; } }
			return nullptr;
		}
	};
}

TEST_F(DeltaViewIntegration, TimelineCardCarriesDeltaMarkersOnlyInDeltaMode)
{
	DomainRig r; r.SetupDomain();
	Json::Value s = r.State();
	EXPECT_FALSE(s["timeline"].isMember("deltaMarkers"));
	EXPECT_FALSE(s.isMember("delta"));

	ASSERT_TRUE(r.Delta("temp", "sample:1", "sample:3")["ok"].asBool());
	r.hdomain->Update();
	s = r.State();
	ASSERT_TRUE(s["timeline"].isMember("deltaMarkers")) << s["timeline"].toStyledString();
	EXPECT_EQ(s["timeline"]["deltaMarkers"]["a"].asUInt(), 1u);
	EXPECT_EQ(s["timeline"]["deltaMarkers"]["b"].asUInt(), 3u);
	EXPECT_EQ(s["history"]["mode"].asString(), "delta");

	// now has no sample: null.
	ASSERT_TRUE(r.Delta("temp", "sample:2", "now")["ok"].asBool());
	r.hdomain->Update();
	s = r.State();
	EXPECT_EQ(s["timeline"]["deltaMarkers"]["a"].asUInt(), 2u);
	EXPECT_TRUE(s["timeline"]["deltaMarkers"]["b"].isNull());

	// A scale change rebuilds the card (history.scale is part of it).
	Json::Value sa(Json::objectValue); sa["scale"] = 9.0;
	ASSERT_TRUE(r.registrar.Execute("cells.history.delta", sa)["ok"].asBool());
	r.hdomain->Update();
	s = r.State();
	EXPECT_NEAR(s["history"]["scale"].asDouble(), 9.0, 1e-6);

	r.DeltaOff();
	r.hdomain->Update();
	s = r.State();
	EXPECT_FALSE(s["timeline"].isMember("deltaMarkers"));
	EXPECT_FALSE(s.isMember("delta"));
}

TEST_F(DeltaViewIntegration, GroupTableIsExposedAndBuiltOncePerViewOrSelectionChange)
{
	DomainRig r; r.SetupDomain();
	ASSERT_TRUE(r.Delta("temp", "sample:0", "sample:3")["ok"].asBool());
	r.hdomain->Update();
	EXPECT_EQ(r.hdomain->DeltaCard().BuildCount(), 1u);
	Json::Value d = r.State()["delta"];
	ASSERT_TRUE(d.isObject());
	EXPECT_EQ(d["groups"]["side"].asString(), "B");
	EXPECT_FALSE(d["groups"]["baseNoData"].asBool());
	EXPECT_FALSE(d["groups"]["truncated"].asBool());
	const Json::Value& rows = d["groups"]["groups"];
	ASSERT_TRUE(rows.isArray());
	ASSERT_EQ(rows.size(), 3u);	// biome 0, 1, 2 (no selection: whole grid)
	const Json::Value* g0 = r.FindGroup(rows, "biome", 0);
	ASSERT_NE(g0, nullptr);
	EXPECT_EQ((*g0)["count"].asUInt(), 4u);
	EXPECT_NEAR((*g0)["change"].asDouble(), 30.0, 0.02);
	EXPECT_NEAR((*g0)["meanA"].asDouble(), 4.5, 0.02);	// cells 0,3,6,9
	EXPECT_NEAR((*g0)["meanB"].asDouble(), 34.5, 0.02);
	EXPECT_TRUE(g0->isMember("label"));
	EXPECT_EQ(d["groups"]["selected"].asUInt(), 0u);

	// Idle frames and hover changes do not rebuild the table.
	r.hdomain->Update(); r.hdomain->Update();
	r.hoverQ = 1; r.hoverR = 1;
	r.hdomain->Update();
	EXPECT_EQ(r.hdomain->DeltaCard().BuildCount(), 1u);

	// A selection restricts the table: cells 0,1,2,3 -> biome 0 {0,3}, biome 1 {1}, biome 2 {2}.
	r.SelectCells({ 0, 1, 2, 3 });
	r.hdomain->Update();
	EXPECT_EQ(r.hdomain->DeltaCard().BuildCount(), 2u);
	d = r.State()["delta"];
	EXPECT_EQ(d["groups"]["selected"].asUInt(), 4u);
	g0 = r.FindGroup(d["groups"]["groups"], "biome", 0);
	ASSERT_NE(g0, nullptr);
	EXPECT_EQ((*g0)["count"].asUInt(), 2u);
	r.hdomain->Update();
	EXPECT_EQ(r.hdomain->DeltaCard().BuildCount(), 2u);

	// A new delta, a group-side switch and leaving delta.
	ASSERT_TRUE(r.Delta("temp", "sample:1", "sample:2")["ok"].asBool());
	r.hdomain->Update();
	EXPECT_EQ(r.hdomain->DeltaCard().BuildCount(), 3u);
	Json::Value gs(Json::objectValue); gs["groupside"] = "A";
	ASSERT_TRUE(r.registrar.Execute("cells.history.delta", gs)["ok"].asBool());
	r.hdomain->Update();
	EXPECT_EQ(r.hdomain->DeltaCard().BuildCount(), 4u);
	EXPECT_EQ(r.State()["delta"]["groups"]["side"].asString(), "A");
	r.DeltaOff();
	r.hdomain->Update();
	EXPECT_EQ(r.hdomain->DeltaCard().BuildCount(), 4u) << "no work outside delta mode";
	EXPECT_FALSE(r.State().isMember("delta"));
}

TEST_F(DeltaViewIntegration, GroupTableFollowsALiveSideWhenItsLayerChanges)
{
	DomainRig r; r.SetupDomain();
	ASSERT_TRUE(r.Delta("temp", "sample:0", "now")["ok"].asBool());
	r.hdomain->Update();
	EXPECT_EQ(r.hdomain->DeltaCard().BuildCount(), 1u);
	for (uint32_t c = 0; c < 12; ++c) { r.grid.Set("temp", c, 90.0f); }
	r.grid.BumpVersion("temp");
	r.hdomain->Update();
	EXPECT_EQ(r.hdomain->DeltaCard().BuildCount(), 2u);
	const Json::Value rows = r.State()["delta"]["groups"]["groups"];
	ASSERT_GE(rows.size(), 1u);
	EXPECT_NEAR(rows[0u]["meanB"].asDouble(), 90.0, 0.02);
}

TEST_F(DeltaViewIntegration, GroupTableMarksBaseNoDataWhenASideHasNone)
{
	DomainRig r; r.SetupDomain();
	ViewResult vr;
	ASSERT_TRUE(r.view->EnterDelta(StringCRC("rain"), SampleSide(0), SampleSide(1), vr));	// untracked
	r.hdomain->Update();
	const Json::Value d = r.State()["delta"];
	EXPECT_TRUE(d["groups"]["baseNoData"].asBool());
	EXPECT_EQ(d["groups"]["groups"].size(), 0u);
}

TEST_F(DeltaViewIntegration, HoverInfoCarriesBothSidesAndTheDelta)
{
	DomainRig r; r.SetupDomain();
	ASSERT_TRUE(r.Delta("veg", "sample:0", "sample:1")["ok"].asBool());
	r.hoverQ = 1; r.hoverR = 0;	// cell 1
	r.hdomain->Update();
	Json::Value hov = r.State()["delta"]["hover"];
	ASSERT_TRUE(hov.isObject()) << r.State().toStyledString();
	EXPECT_EQ(hov["cell"].asUInt(), 1u);
	EXPECT_EQ(hov["q"].asInt(), 1);
	EXPECT_EQ(hov["r"].asInt(), 0);
	EXPECT_NEAR(hov["a"].asDouble(), 51.0, 0.01);
	EXPECT_NEAR(hov["b"].asDouble(), 31.0, 0.01);
	EXPECT_NEAR(hov["delta"].asDouble(), -20.0, 0.02);
	EXPECT_TRUE(hov["aHasData"].asBool());
	EXPECT_TRUE(hov["bHasData"].asBool());

	r.hoverQ = -1;
	r.hdomain->Update();
	EXPECT_TRUE(r.State()["delta"]["hover"].isNull());
}

TEST_F(DeltaViewIntegration, DeltaCellReaderComputesBothSidesForOneCellFromACachedRead)
{
	DeltaRig r; r.Setup();
	DeltaCellReader reader(*r.rec);
	const CellLayerDesc* temp = r.registry.Find(StringCRC("temp"));
	ASSERT_NE(temp, nullptr);
	DeltaSpec spec; spec.layer = StringCRC("temp"); spec.a = SampleSide(1); spec.b = SampleSide(3);

	DeltaCellInfo info;
	ASSERT_TRUE(reader.Get(*temp, spec, 1, 5, info));
	EXPECT_TRUE(info.aHasData); EXPECT_TRUE(info.bHasData);
	EXPECT_NEAR(info.a, 15.0f, 0.01f);
	EXPECT_NEAR(info.b, 35.0f, 0.01f);
	EXPECT_NEAR(info.delta, 20.0f, 0.02f);
	const uint32_t reads = reader.ReadCount();
	EXPECT_EQ(reads, 2u);
	for (uint32_t c = 0; c < 12; ++c) { ASSERT_TRUE(reader.Get(*temp, spec, 1, c, info)); }
	EXPECT_EQ(reader.ReadCount(), reads) << "same epoch and version: cached";
	ASSERT_TRUE(reader.Get(*temp, spec, 2, 5, info));
	EXPECT_EQ(reader.ReadCount(), reads + 2u) << "a new epoch re-reads";

	EXPECT_FALSE(reader.Get(*temp, spec, 2, 12, info)) << "cell out of range";

	// A side with no data: reported, delta is NaN, never live.
	const CellLayerDesc* rain = r.registry.Find(StringCRC("rain"));
	ASSERT_NE(rain, nullptr);
	spec.layer = StringCRC("rain");
	ASSERT_TRUE(reader.Get(*rain, spec, 3, 0, info));
	EXPECT_FALSE(info.aHasData); EXPECT_FALSE(info.bHasData);
	EXPECT_TRUE(std::isnan(info.delta));

	// now side is the live value.
	spec.layer = StringCRC("temp"); spec.a = SampleSide(2); spec.b = NowSide();
	ASSERT_TRUE(reader.Get(*temp, spec, 4, 0, info));
	EXPECT_TRUE(info.bHasData);
	EXPECT_FLOAT_EQ(info.b, 70.0f);
	EXPECT_NEAR(info.delta, 50.0f, 0.02f);

	// A category layer has no delta.
	const CellLayerDesc* biome = r.registry.Find(StringCRC("biome"));
	spec.layer = StringCRC("biome"); spec.a = SampleSide(0); spec.b = SampleSide(1);
	ASSERT_TRUE(reader.Get(*biome, spec, 5, 4, info));
	EXPECT_FLOAT_EQ(info.a, 1.0f);
	EXPECT_TRUE(std::isnan(info.delta));
}

TEST_F(DeltaViewIntegration, SelectedCellDetailsShowABAndDeltaRowsOnlyInDeltaMode)
{
	DomainRig r; r.SetupDomain();
	r.SelectCells({ 6 });
	auto detailTexts = [&]()
	{
		std::vector<std::string> rows;
		const Json::Value cell = r.StatsLayer("veg");	// ensures the stats path runs
		(void)cell;
		const Json::Value all = r.queries->SelectionStats(StatsArgs("veg"));
		for (const Json::Value& d : all["cell"]["details"]) { rows.push_back(d["label"].asString() + "=" + d["text"].asString()); }
		return rows;
	};
	r.hdomain->Update();
	EXPECT_EQ(r.registry.Details().ProviderCountForGrid(r.GridDesc()), 0u);
	EXPECT_TRUE(detailTexts().empty());

	ASSERT_TRUE(r.Delta("veg", "sample:0", "sample:1")["ok"].asBool());
	r.hdomain->Update();
	EXPECT_EQ(r.registry.Details().ProviderCountForGrid(r.GridDesc()), 1u);
	const std::vector<std::string> rows = detailTexts();
	ASSERT_FALSE(rows.empty());
	std::string joined;
	for (const std::string& s : rows) { joined += s + "|"; }
	EXPECT_NE(joined.find("A (sample:0)"), std::string::npos) << joined;
	EXPECT_NE(joined.find("B (sample:1)"), std::string::npos) << joined;
	EXPECT_NE(joined.find("Delta"), std::string::npos) << joined;
	EXPECT_NE(joined.find("56"), std::string::npos) << "cell 6 A = 56: " << joined;

	r.DeltaOff();
	r.hdomain->Update();
	EXPECT_EQ(r.registry.Details().ProviderCountForGrid(r.GridDesc()), 0u);
	EXPECT_TRUE(detailTexts().empty());
}

namespace
{
	void PointAt(DomainRig& r, int q, int c)
	{
		const Dia::Maths::Vector2D p = Dia::Pathfinding::HexTopology::CellCenter(Dia::Pathfinding::CellCoord{ q, c }, 1.0f);
		float sx, sy;
		Dia::CellLayers::Testing::WorldToScreen(TestView(), p.X(), p.Y(), sx, sy);
		r.draw.mouse = Dia::Maths::Vector2D(sx, sy);
	}
}

TEST_F(DeltaViewIntegration, HoverExtraCarriesABAndDeltaLinesOnlyInDeltaMode)
{
	DomainRig r; r.SetupDomain();
	ASSERT_TRUE(r.domain->AddHoverExtraFn(CellLayerViewerDomain::HoverExtraFn(r.hdomain.get(), &CellHistoryDomain::HoverExtra)));
	PointAt(r, 1, 0);	// cell 1: veg A = 51, B = 31
	r.Frame();
	Json::Value s;
	r.domain->GetJSONState(s);
	ASSERT_TRUE(s["hover"]["valid"].asBool());
	EXPECT_FALSE(s["hover"].isMember("extra")) << "live view: no extra";

	ASSERT_TRUE(r.Delta("veg", "sample:0", "sample:1")["ok"].asBool());
	r.hdomain->Update();
	r.domain->GetJSONState(s);
	const Json::Value& extra = s["hover"]["extra"];
	ASSERT_TRUE(extra.isArray()) << s["hover"].toStyledString();
	ASSERT_EQ(extra.size(), 3u);
	EXPECT_EQ(extra[0u]["label"].asString(), "A (sample:0)");
	EXPECT_EQ(extra[0u]["text"].asString(), "51");
	EXPECT_EQ(extra[1u]["label"].asString(), "B (sample:1)");
	EXPECT_EQ(extra[1u]["text"].asString(), "31");
	EXPECT_EQ(extra[2u]["label"].asString(), "Delta");
	EXPECT_EQ(extra[2u]["text"].asString(), "-20");

	// Now side: the live value.
	ASSERT_TRUE(r.Delta("veg", "sample:2", "now")["ok"].asBool());
	r.hdomain->Update();
	r.domain->GetJSONState(s);
	ASSERT_EQ(s["hover"]["extra"].size(), 3u);
	EXPECT_EQ(s["hover"]["extra"][1u]["label"].asString(), "B (now)");
	EXPECT_EQ(s["hover"]["extra"][1u]["text"].asString(), "61");

	r.DeltaOff();
	r.hdomain->Update();
	r.domain->GetJSONState(s);
	EXPECT_TRUE(s["hover"]["valid"].asBool());
	EXPECT_FALSE(s["hover"].isMember("extra")) << "after off: no extra";
	r.domain->RemoveHoverExtraFn(CellLayerViewerDomain::HoverExtraFn(r.hdomain.get(), &CellHistoryDomain::HoverExtra));
}

TEST_F(DeltaViewIntegration, HoverExtraIsEmptyWhenTheHoveredLayerIsNotTheDeltaLayer)
{
	DomainRig r; r.SetupDomain();
	ASSERT_TRUE(r.domain->AddHoverExtraFn(CellLayerViewerDomain::HoverExtraFn(r.hdomain.get(), &CellHistoryDomain::HoverExtra)));
	PointAt(r, 1, 0);
	ASSERT_TRUE(r.Delta("temp", "sample:0", "sample:1")["ok"].asBool());	// base stays veg
	r.hdomain->Update();
	Json::Value s;
	r.domain->GetJSONState(s);
	ASSERT_TRUE(s["hover"]["valid"].asBool());
	EXPECT_FALSE(s["hover"].isMember("extra")) << s["hover"].toStyledString();
	r.domain->RemoveHoverExtraFn(CellLayerViewerDomain::HoverExtraFn(r.hdomain.get(), &CellHistoryDomain::HoverExtra));
}

#endif // DIA_DEBUG
