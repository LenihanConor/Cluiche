#include <gtest/gtest.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/SelectionFingerprint.h>
#include <DiaCellLayers/ViewStatePublisher.h>
#include <DiaCellLayers/ViewerStateSnapshot.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/ViewStateTestSupport.h>
#include <DiaDebugServer/SimHostedTypes.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

// Editor Mirror (Feature 5) exhaustive pass: gaps left by the per-task tests. Reply-size limits against the 16 KB sim-hosted cap,
// indices past 16 bits, query determinism, grid-id helpers, registrar edge cases, publisher payload shape / ordering / version
// semantics, snapshot window and identity edges, and re-entry now that the viewer has no mutex.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	bool IsError(const Json::Value& v) { return v.isObject() && v.isMember("error") && v["error"].isString(); }

	std::string Compact(const Json::Value& v)
	{
		Json::StreamWriterBuilder b;
		b["indentation"] = "";
		return Json::writeString(b, v);
	}

	Json::Value ValuesArgs(const char* layer)
	{
		Json::Value a(Json::objectValue);
		a["layer"] = layer;
		return a;
	}

	Json::Value OneArg(const char* key, const Json::Value& value)
	{
		Json::Value a(Json::objectValue);
		a[key] = value;
		return a;
	}

	// Two layers on one grid (alpha, beta), a second grid (gamma), two overlays; mirror queries and a publisher on a fake clock.
	struct MirrorRig
	{
		TestGrid gridA;
		TestGrid gridB{ 2, 2 };
		Dia::Graphics::CellFieldRegistry cellField;
		CountingLayerProvider alpha, beta, gamma;
		FakeDebugDraw draw;
		FakeClock clock;
		TopicRecorder topic;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		LayerUploader uploader{ cellField, nullptr };
		CellLayerViewerDomain domain{ registry, uploader, nullptr };
		MirrorQueries queries{ registry, domain, nullptr };
		ViewStatePublisher publisher{ registry, domain, topic, clock };

		MirrorRig(uint16_t w = 4, uint16_t h = 3) : gridA(w, h)
		{
			FakePuId() = 1;
			const uint32_t n = gridA.CellCount();
			alpha.values.resize(n);
			beta.values.resize(n);
			for (uint32_t i = 0; i < n; ++i) { alpha.values[i] = static_cast<float>(i); beta.values[i] = static_cast<float>(i) * 2.0f; }
			gamma.values = { 5, 6, 7, 8 };
			EXPECT_EQ(registry.Register(MakeCountingLayer("alpha", &gridA.desc, alpha)), CellRegisterResult::Ok);
			EXPECT_EQ(registry.Register(MakeCountingLayer("beta", &gridA.desc, beta)), CellRegisterResult::Ok);
			EXPECT_EQ(registry.Register(MakeCountingLayer("gamma", &gridB.desc, gamma)), CellRegisterResult::Ok);
			EXPECT_EQ(registry.RegisterOverlay(MakeOverlay("wind")), CellRegisterResult::Ok);
			EXPECT_EQ(registry.RegisterOverlay(MakeOverlay("rivers")), CellRegisterResult::Ok);
			domain.Update(draw);
		}

		void Frame() { domain.Update(draw); publisher.Update(); }

		void Select(const std::vector<uint32_t>& cells)
		{
			CellSelection* sel = domain.ActiveSelection();
			ASSERT_NE(sel, nullptr);
			sel->BeginEdit();
			sel->Clear();
			for (uint32_t c : cells) { sel->Set(c, true); }
			sel->EndEdit();
		}
	};

	std::vector<std::string> MemberNames(const Json::Value& v)
	{
		std::vector<std::string> names = v.getMemberNames();
		std::sort(names.begin(), names.end());
		return names;
	}
}

// ---- reply sizes against the 16 KB sim-hosted cap ----

TEST(MirrorExhaustiveSizes, LayerValuesMaxChunkWithLongestIdsStaysUnderTheSimCap)
{
	const std::string longLayer(60, 'l');
	const std::string longGrid(60, 'g');
	TestGrid grid{ 40, 40 };	// 1,600 cells: a full 1,000 chunk plus a remainder
	grid.desc.id = StringCRC(longGrid.c_str());
	Dia::Graphics::CellFieldRegistry cellField;
	CountingLayerProvider prov;
	prov.values.resize(grid.CellCount());
	for (uint32_t i = 0; i < prov.values.size(); ++i) { prov.values[i] = -1.0e30f * static_cast<float>(i + 1); }	// widest textual-ish payload bits
	FakeDebugDraw draw;
	FakePuId() = 1;
	CellLayerRegistry registry{ nullptr, &FakePuIdFn };
	LayerUploader uploader{ cellField, nullptr };
	CellLayerViewerDomain domain{ registry, uploader, nullptr };
	MirrorQueries queries{ registry, domain, nullptr };
	ASSERT_EQ(registry.Register(MakeCountingLayer(longLayer.c_str(), &grid.desc, prov)), CellRegisterResult::Ok);
	domain.Update(draw);

	Json::Value a = ValuesArgs(longLayer.c_str());
	a["count"] = 100000;	// over the limit: clamped
	const Json::Value r = queries.LayerValues(a);
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["count"].asUInt(), 1000u);
	EXPECT_EQ(r["gridId"].asString(), longGrid);
	const size_t bytes = Compact(r).size();
	EXPECT_LT(bytes, static_cast<size_t>(Dia::DebugServer::kSimPayloadBytes)) << "reply was " << bytes << " bytes";
	EXPECT_GT(bytes, 5000u) << "sanity: a full chunk is about 5.3 KB of base64";
}

TEST(MirrorExhaustiveSizes, SelectGetFullChunkOfFiveDigitIndicesPastSixteenBitsStaysUnderTheSimCap)
{
	MirrorRig rig(300, 300);	// 90,000 cells: indices well past 65,535
	std::vector<uint32_t> cells;
	for (uint32_t i = 0; i < 1000; ++i) { cells.push_back(89000 + i); }
	rig.Select(cells);
	const Json::Value r = rig.queries.SelectionGet(Json::Value(Json::objectValue));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["total"].asUInt(), 1000u);
	ASSERT_EQ(r["indices"].size(), 1000u);
	EXPECT_EQ(r["indices"][0u].asUInt(), 89000u);
	EXPECT_EQ(r["indices"][999u].asUInt(), 89999u);
	EXPECT_LT(Compact(r).size(), static_cast<size_t>(Dia::DebugServer::kSimPayloadBytes));
}

TEST(MirrorExhaustiveSizes, SelectGetPagesPastSixteenBitIndicesAreExactAndAscending)
{
	MirrorRig rig(300, 300);
	rig.Select({ 70000, 65536, 65535, 89999, 0, 4294967u % 90000u });
	std::vector<uint32_t> got;
	uint32_t offset = 0;
	for (;;)
	{
		const Json::Value r = rig.queries.SelectionGet(OneArg("offset", offset));
		ASSERT_FALSE(IsError(r));
		for (const Json::Value& i : r["indices"]) { got.push_back(i.asUInt()); }
		if (r["indices"].empty()) { break; }
		offset += r["indices"].size();
		if (offset >= r["total"].asUInt()) { break; }
	}
	std::vector<uint32_t> expect = { 0, 4294967u % 90000u, 65535, 65536, 70000, 89999 };
	std::sort(expect.begin(), expect.end());
	EXPECT_EQ(got, expect);
}

// ---- determinism ----

TEST(MirrorExhaustiveDeterminism, IdenticalRigsAnswerEveryQueryWithIdenticalBytes)
{
	MirrorRig a, b;
	a.Select({ 1, 5, 7 });
	b.Select({ 7, 1, 5 });	// different insertion order, same set
	for (MirrorRig* r : { &a, &b }) { r->queries.NotifyFrame(); }
	EXPECT_EQ(Compact(a.queries.LayerList()), Compact(b.queries.LayerList()));
	EXPECT_EQ(Compact(a.queries.LayerValues(ValuesArgs("alpha"))), Compact(b.queries.LayerValues(ValuesArgs("alpha"))));
	EXPECT_EQ(Compact(a.queries.SelectionGet(Json::Value(Json::objectValue))), Compact(b.queries.SelectionGet(Json::Value(Json::objectValue))));
	EXPECT_EQ(Compact(a.queries.SelectionStats(Json::Value(Json::objectValue))), Compact(b.queries.SelectionStats(Json::Value(Json::objectValue))));
}

TEST(MirrorExhaustiveDeterminism, RepeatedQueriesAreStableAcrossFramesWhenNothingChanges)
{
	MirrorRig rig;
	rig.Select({ 2, 3 });
	for (int i = 0; i < 2; ++i) { rig.domain.Update(rig.draw); rig.queries.NotifyFrame(); }	// let the inspector settle (its "updating" flag clears)
	const std::string list = Compact(rig.queries.LayerList());
	const std::string values = Compact(rig.queries.LayerValues(ValuesArgs("beta")));
	const std::string get = Compact(rig.queries.SelectionGet(Json::Value(Json::objectValue)));
	const std::string stats = Compact(rig.queries.SelectionStats(Json::Value(Json::objectValue)));
	for (int i = 0; i < 3; ++i)
	{
		rig.queries.NotifyFrame();
		rig.domain.Update(rig.draw);
		EXPECT_EQ(Compact(rig.queries.LayerList()), list);
		EXPECT_EQ(Compact(rig.queries.LayerValues(ValuesArgs("beta"))), values);
		EXPECT_EQ(Compact(rig.queries.SelectionGet(Json::Value(Json::objectValue))), get);
		EXPECT_EQ(Compact(rig.queries.SelectionStats(Json::Value(Json::objectValue))), stats) << "a new frame recomputes to the same answer";
	}
}

// ---- layer / selection query edges ----

TEST(MirrorExhaustiveValues, OverlayEmptyAndWrongCaseIdsAreUnknownLayers)
{
	MirrorRig rig;
	for (const char* id : { "wind", "rivers", "", "ALPHA", "Alpha", "alpha ", "grid.0" })
	{
		const Json::Value r = rig.queries.LayerValues(ValuesArgs(id));
		ASSERT_TRUE(IsError(r)) << "'" << id << "'";
		EXPECT_EQ(r["error"].asString(), "unknown layer") << "'" << id << "'";
	}
	EXPECT_FALSE(IsError(rig.queries.LayerValues(ValuesArgs("alpha"))));
}

TEST(MirrorExhaustiveValues, ErrorRepliesCarryNoPartialPayload)
{
	MirrorRig rig;
	const Json::Value bad = rig.queries.LayerValues(OneArg("layer", "nope"));
	EXPECT_EQ(MemberNames(bad), (std::vector<std::string>{ "error" }));
	const Json::Value badGet = rig.queries.SelectionGet(OneArg("offset", 99));
	EXPECT_EQ(MemberNames(badGet), (std::vector<std::string>{ "error" }));
}

TEST(MirrorExhaustiveValues, ReplyKeySetIsExactlyTheSpecShape)
{
	MirrorRig rig;
	rig.Select({ 1 });
	EXPECT_EQ(MemberNames(rig.queries.LayerValues(ValuesArgs("alpha"))),
		(std::vector<std::string>{ "count", "data", "encoding", "gridId", "height", "layer", "offset", "total", "version", "width" }));
	EXPECT_EQ(MemberNames(rig.queries.SelectionGet(Json::Value(Json::objectValue))),
		(std::vector<std::string>{ "hash", "indices", "offset", "total", "version" }));
}

TEST(MirrorExhaustiveSelectGet, FollowsTheActiveGridAndSwitchingGridsClearsTheSelection)
{
	MirrorRig rig;
	rig.Select({ 1, 2, 3 });
	EXPECT_EQ(rig.queries.SelectionGet(Json::Value(Json::objectValue))["total"].asUInt(), 3u);
	ASSERT_TRUE(rig.domain.SetBase(StringCRC("gamma")));	// second grid: its selection is its own (empty)
	rig.domain.Update(rig.draw);
	const Json::Value other = rig.queries.SelectionGet(Json::Value(Json::objectValue));
	EXPECT_EQ(other["total"].asUInt(), 0u);
	EXPECT_EQ(other["hash"].asString(), "0000000000000000");
	rig.Select({ 0 });
	EXPECT_EQ(rig.queries.SelectionGet(Json::Value(Json::objectValue))["total"].asUInt(), 1u);
	ASSERT_TRUE(rig.domain.SetBase(StringCRC("alpha")));
	rig.domain.Update(rig.draw);
	EXPECT_EQ(rig.queries.SelectionGet(Json::Value(Json::objectValue))["total"].asUInt(), 0u) << "Feature 2: switching the active grid clears the selection";
}

TEST(MirrorExhaustiveSelectGet, EmptyHashIsAllZerosAndCountZeroAtOffsetZeroIsValid)
{
	MirrorRig rig;
	Json::Value a(Json::objectValue);
	a["offset"] = 0;
	a["count"] = 0;
	const Json::Value r = rig.queries.SelectionGet(a);
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["hash"].asString(), "0000000000000000");
	EXPECT_EQ(r["total"].asUInt(), 0u);
	EXPECT_TRUE(r["indices"].isArray());
	EXPECT_EQ(r["indices"].size(), 0u);
}

// ---- stats edges ----

TEST(MirrorExhaustiveStats, UpperCaseAllAndNullLayersAreErrorsAndComputeNothing)
{
	MirrorRig rig;
	rig.Select({ 1, 2 });
	const uint32_t before = rig.queries.StatsComputeCount();
	EXPECT_TRUE(IsError(rig.queries.SelectionStats(OneArg("layers", "ALL"))));
	EXPECT_TRUE(IsError(rig.queries.SelectionStats(OneArg("layers", Json::Value(Json::nullValue)))));
	EXPECT_TRUE(IsError(rig.queries.SelectionStats(OneArg("layers", "wind"))));	// an overlay id, as a bare string
	Json::Value overlayInArray(Json::arrayValue);
	overlayInArray.append("wind");
	EXPECT_TRUE(IsError(rig.queries.SelectionStats(OneArg("layers", overlayInArray)))) << "overlays are not layers";
	EXPECT_EQ(rig.queries.StatsComputeCount(), before);
}

TEST(MirrorExhaustiveStats, SameCountDifferentCellsInsideOneFrameRecomputesWithTheNewHashAndVersion)
{
	MirrorRig rig;
	rig.Select({ 1, 2 });
	const Json::Value first = rig.queries.SelectionStats(Json::Value(Json::objectValue));
	ASSERT_FALSE(IsError(first)) << first.toStyledString();
	const uint32_t computes = rig.queries.StatsComputeCount();
	rig.Select({ 5, 6 });	// same count, different cells, same frame (no NotifyFrame)
	const Json::Value second = rig.queries.SelectionStats(Json::Value(Json::objectValue));
	ASSERT_FALSE(IsError(second)) << second.toStyledString();
	EXPECT_EQ(rig.queries.StatsComputeCount(), computes + 1) << "the selection version is part of the cache key";
	EXPECT_NE(second["hash"].asString(), first["hash"].asString());
	EXPECT_NE(second["version"].asUInt(), first["version"].asUInt());
	EXPECT_EQ(second["count"].asUInt(), 2u);
	EXPECT_EQ(second["hash"].asString(), rig.queries.SelectionGet(Json::Value(Json::objectValue))["hash"].asString());
}

TEST(MirrorExhaustiveStats, ErrorCallsBetweenValidOnesDoNotDisturbTheFrameCache)
{
	MirrorRig rig;
	rig.Select({ 1, 2 });
	const Json::Value first = rig.queries.SelectionStats(Json::Value(Json::objectValue));
	if (IsError(first)) { GTEST_SKIP() << "schema-less rig cannot aggregate: " << first["error"].asString(); }
	const uint32_t computes = rig.queries.StatsComputeCount();
	EXPECT_TRUE(IsError(rig.queries.SelectionStats(OneArg("layers", "nope"))));
	EXPECT_EQ(rig.queries.SelectionStats(Json::Value(Json::objectValue)), first);
	EXPECT_EQ(rig.queries.StatsComputeCount(), computes) << "still served from the cache";
}

// ---- MirrorGridId ----

TEST(MirrorExhaustiveGridId, IndexOfAndFormatEdges)
{
	MirrorRig rig;
	EXPECT_EQ(MirrorGridId::IndexOf(rig.registry, nullptr), 0u);
	TestGrid foreign{ 2, 2 };
	EXPECT_EQ(MirrorGridId::IndexOf(rig.registry, &foreign.desc), 0u) << "not registered: 0";
	EXPECT_EQ(MirrorGridId::IndexOf(rig.registry, &rig.gridA.desc), 0u);
	EXPECT_EQ(MirrorGridId::IndexOf(rig.registry, &rig.gridB.desc), 1u);

	char id[MirrorGridId::kBufferSize];
	MirrorGridId::Format(7, id);
	EXPECT_STREQ(id, "grid.7");
	MirrorGridId::Format(rig.registry, nullptr, id);
	EXPECT_STREQ(id, "grid.0");
	MirrorGridId::Format(rig.registry, &rig.gridB.desc, id);
	EXPECT_STREQ(id, "grid.1");
	MirrorGridId::Format(rig.registry, &foreign.desc, id);
	EXPECT_STREQ(id, "grid.0");

	const std::string longId(60, 'x');
	foreign.desc.id = StringCRC(longId.c_str());
	MirrorGridId::Format(rig.registry, &foreign.desc, id);
	EXPECT_STREQ(id, longId.c_str()) << "a named grid always uses its own id, registered or not";
}

// ---- registrar edges ----

TEST(MirrorExhaustiveRegistrar, UnregisterRemovesAllFourAndIsHarmlessTwiceOrWhenEmpty)
{
	MirrorRig rig;
	FakeMirrorRegistrar registrar;
	UnregisterMirrorQueries(registrar);	// nothing registered yet
	EXPECT_EQ(registrar.Count(), 0u);
	ASSERT_TRUE(RegisterMirrorQueries(registrar, rig.queries));
	EXPECT_EQ(registrar.Count(), 4u);
	UnregisterMirrorQueries(registrar);
	EXPECT_EQ(registrar.Count(), 0u);
	UnregisterMirrorQueries(registrar);
	EXPECT_EQ(registrar.Count(), 0u);
	ASSERT_TRUE(RegisterMirrorQueries(registrar, rig.queries)) << "the names are free again";
	EXPECT_EQ(registrar.Count(), 4u);
}

TEST(MirrorExhaustiveRegistrar, NoneOfTheFourIsRegisteredAsACommand)
{
	MirrorRig rig;
	FakeMirrorRegistrar registrar;
	ASSERT_TRUE(RegisterMirrorQueries(registrar, rig.queries));
	for (const char* name : { "cells.layer.list", "cells.layer.values", "cells.select.get", "cells.select.stats" })
	{
		EXPECT_TRUE(registrar.Has(name)) << name;
		EXPECT_FALSE(registrar.IsCommand(name)) << name << " is read-only: a query";
	}
}

TEST(MirrorExhaustiveRegistrar, HandlersSurviveNullAndWrongTypedArgsWithoutCrashing)
{
	MirrorRig rig;
	FakeMirrorRegistrar registrar;
	ASSERT_TRUE(RegisterMirrorQueries(registrar, rig.queries));
	const Json::Value inputs[] = { Json::Value(), Json::Value(7), Json::Value("text"), Json::Value(Json::arrayValue), Json::Value(true) };
	for (const char* name : { "cells.layer.list", "cells.layer.values", "cells.select.get", "cells.select.stats" })
	{
		for (const Json::Value& in : inputs)
		{
			const Json::Value r = registrar.Execute(name, in);
			EXPECT_TRUE(r.isObject()) << name << " must always answer with an object, for " << in.toStyledString();
		}
	}
	EXPECT_TRUE(IsError(registrar.Execute("cells.layer.values", Json::Value())));
	EXPECT_FALSE(IsError(registrar.Execute("cells.layer.list", Json::Value("ignored"))));
}

// ---- ViewStatePublisher ----

TEST(MirrorExhaustivePublisher, PayloadKeySetsAreExactlyTheSpecShape)
{
	MirrorRig rig;
	rig.Frame();
	ASSERT_EQ(rig.topic.Count(), 1u);
	const Json::Value& p = rig.topic.Last();
	EXPECT_EQ(MemberNames(p), (std::vector<std::string>{ "base", "grid", "history", "layersVersion", "overlays", "selection", "viewVersion" }));
	EXPECT_EQ(MemberNames(p["grid"]), (std::vector<std::string>{ "height", "id", "topology", "width" }));
	EXPECT_EQ(MemberNames(p["selection"]), (std::vector<std::string>{ "count", "hash", "version" }));
	EXPECT_TRUE(p["viewVersion"].isUInt());
	EXPECT_TRUE(p["layersVersion"].isUInt());
	EXPECT_TRUE(p["selection"]["hash"].isString());
}

TEST(MirrorExhaustivePublisher, EnablingOverlaysInADifferentOrderGivesTheSamePayloadAndNoExtraPublish)
{
	MirrorRig a, b;
	a.Frame(); b.Frame();
	a.clock.Advance(200); b.clock.Advance(200);
	ASSERT_TRUE(a.domain.ToggleOverlay(StringCRC("wind")));
	ASSERT_TRUE(a.domain.ToggleOverlay(StringCRC("rivers")));
	ASSERT_TRUE(b.domain.ToggleOverlay(StringCRC("rivers")));
	ASSERT_TRUE(b.domain.ToggleOverlay(StringCRC("wind")));
	a.Frame(); b.Frame();
	ASSERT_EQ(a.topic.Count(), 2u);
	ASSERT_EQ(b.topic.Count(), 2u);
	EXPECT_EQ(Compact(a.topic.Last()), Compact(b.topic.Last())) << "overlays list in registry order, not toggle order";
	ASSERT_EQ(a.topic.Last()["overlays"].size(), 2u);
	EXPECT_EQ(a.topic.Last()["overlays"][0u].asString(), "wind");
	EXPECT_EQ(a.topic.Last()["overlays"][1u].asString(), "rivers");
}

TEST(MirrorExhaustivePublisher, EveryOneOfTheMaximumOverlaysIsListedInRegistryOrder)
{
	TestGrid grid{ 2, 2 };
	Dia::Graphics::CellFieldRegistry cellField;
	CountingLayerProvider prov;
	prov.values = { 1, 2, 3, 4 };
	FakeDebugDraw draw;
	FakeClock clock;
	TopicRecorder topic;
	FakePuId() = 1;
	CellLayerRegistry registry{ nullptr, &FakePuIdFn };
	LayerUploader uploader{ cellField, nullptr };
	CellLayerViewerDomain domain{ registry, uploader, nullptr };
	ViewStatePublisher publisher{ registry, domain, topic, clock };
	ASSERT_EQ(registry.Register(MakeCountingLayer("only", &grid.desc, prov)), CellRegisterResult::Ok);
	std::vector<std::string> names;
	for (uint32_t i = 0; i < CellLayerRegistry::kMaxOverlays; ++i) { names.push_back("ov" + std::to_string(i)); }
	for (const std::string& n : names) { ASSERT_EQ(registry.RegisterOverlay(MakeOverlay(n.c_str())), CellRegisterResult::Ok) << n; }
	EXPECT_NE(registry.RegisterOverlay(MakeOverlay("one.too.many")), CellRegisterResult::Ok);
	domain.Update(draw);
	for (const std::string& n : names) { ASSERT_TRUE(domain.ToggleOverlay(StringCRC(n.c_str()))) << n; }
	publisher.Update();
	ASSERT_EQ(topic.Count(), 1u);
	const Json::Value& overlays = topic.Last()["overlays"];
	ASSERT_EQ(overlays.size(), CellLayerRegistry::kMaxOverlays);
	for (uint32_t i = 0; i < overlays.size(); ++i) { EXPECT_EQ(overlays[i].asString(), names[i]); }
}

TEST(MirrorExhaustivePublisher, UnregisteringEveryLayerRepublishesTheEmptyBaseWithANewLayersVersion)
{
	MirrorRig rig;
	rig.Frame();
	const uint32_t layersVersion = rig.topic.Last()["layersVersion"].asUInt();
	rig.clock.Advance(200);
	for (const char* id : { "alpha", "beta", "gamma" }) { ASSERT_TRUE(rig.registry.Unregister(StringCRC(id))); }
	rig.Frame();
	ASSERT_EQ(rig.topic.Count(), 2u);
	const Json::Value& p = rig.topic.Last();
	EXPECT_EQ(p["base"].asString(), "") << "empty state: no base";
	EXPECT_TRUE(p["grid"].isObject()) << "pins current behaviour: the viewer keeps its last active grid in the empty state";
	EXPECT_NE(p["layersVersion"].asUInt(), layersVersion);
	EXPECT_EQ(p["selection"]["count"].asUInt(), 0u);
	EXPECT_EQ(p["selection"]["hash"].asString(), "0000000000000000");
	EXPECT_EQ(p["viewVersion"].asUInt(), 2u);
}

TEST(MirrorExhaustivePublisher, SelectionVersionParticipatesButANetZeroEditDoesNot)
{
	MirrorRig rig;
	rig.Select({ 4 });
	rig.Frame();
	ASSERT_EQ(rig.topic.Count(), 1u);
	const std::string hashWithCell = rig.topic.Last()["selection"]["hash"].asString();
	const uint32_t version1 = rig.topic.Last()["selection"]["version"].asUInt();

	rig.clock.Advance(200);
	{	// a bulk edit that ends where it began: no version bump, nothing to publish
		CellSelection* sel = rig.domain.ActiveSelection();
		sel->BeginEdit(); sel->Set(4, false); sel->Set(4, true); sel->EndEdit();
	}
	rig.Frame();
	EXPECT_EQ(rig.topic.Count(), 1u) << "net-zero edit publishes nothing";

	rig.clock.Advance(200);
	rig.Select({});
	rig.Frame();
	ASSERT_EQ(rig.topic.Count(), 2u);
	rig.clock.Advance(200);
	rig.Select({ 4 });
	rig.Frame();
	ASSERT_EQ(rig.topic.Count(), 3u);
	EXPECT_EQ(rig.topic.Last()["selection"]["hash"].asString(), hashWithCell) << "same set, same hash";
	EXPECT_GT(rig.topic.Last()["selection"]["version"].asUInt(), version1) << "but a later version";
}

TEST(MirrorExhaustivePublisher, SameHistoryOfChangesGivesTheSamePayloadSequence)
{
	MirrorRig a, b;
	for (MirrorRig* r : { &a, &b })
	{
		r->Frame();
		r->clock.Advance(150);
		r->domain.SetBase(StringCRC("beta"));
		r->domain.ToggleOverlay(StringCRC("wind"));
		r->Select({ 3, 9 });
		r->Frame();
		r->clock.Advance(150);
		r->domain.SetBase(StringCRC("gamma"));
		r->Frame();
	}
	ASSERT_EQ(a.topic.Count(), b.topic.Count());
	ASSERT_EQ(a.topic.Count(), 3u);
	for (size_t i = 0; i < a.topic.Count(); ++i) { EXPECT_EQ(Compact(a.topic.entries[i].payload), Compact(b.topic.entries[i].payload)) << i; }
}

TEST(MirrorExhaustivePublisher, ViewVersionOnlyAdvancesOnRealPublishesNotOnHeldUpdates)
{
	MirrorRig rig;
	rig.Frame();	// publish 1 at t=0
	for (int i = 0; i < 5; ++i)
	{
		rig.clock.Advance(10);
		ASSERT_TRUE(rig.domain.SetBase(StringCRC((i % 2) ? "alpha" : "beta")));
		rig.Frame();	// all inside the interval
	}
	EXPECT_EQ(rig.publisher.PublishCount(), 1u);
	EXPECT_EQ(rig.publisher.ViewVersion(), 1u);
	EXPECT_EQ(rig.publisher.CoalescedCount(), 3u) << "only the updates whose state differs from the published one are held (beta, beta, beta)";
	rig.clock.Advance(100);
	rig.Frame();
	EXPECT_EQ(rig.publisher.PublishCount(), 2u);
	EXPECT_EQ(rig.publisher.ViewVersion(), 2u) << "the whole burst collapses into a single version bump";
	EXPECT_EQ(rig.topic.Last()["viewVersion"].asUInt(), 2u);
}

TEST(MirrorExhaustivePublisher, TopicIdIsTheSpecName)
{
	EXPECT_EQ(ViewStatePublisher::TopicId(), StringCRC("cells.view.state"));
	EXPECT_EQ(ViewStatePublisher::kMinPublishIntervalMs, 100u);
}

// ---- snapshot publisher / adapter ----

namespace
{
	class ExSnapDomain : public Dia::VisualDebugger::IDebugDomain
	{
	public:
		StringCRC GetDomainId() const override { return StringCRC("ex.domain"); }
		const char* GetDisplayName() const override { return "Ex"; }
		const char* GetDescription() const override { return "Ex domain"; }
		StringCRC GetGroup() const override { return StringCRC("Spatial"); }
		Dia::Core::RGBA GetAccentColour() const override { return Dia::Core::RGBA(9, 8, 7, 255); }
		void GetJSONState(Json::Value& out) override { ++calls; out["simFrame"] = "domain wrote this"; out["a"] = 1; }
		void OnCommand(StringCRC, const Json::Value&) override {}
		uint32_t calls = 0;
	};

	class ExSink : public IViewerStateSink
	{
	public:
		void Write(const char* json, uint32_t length) override { writes.emplace_back(json, length); }
		std::vector<std::string> writes;
	};

	class ExSource : public IViewerStateSource
	{
	public:
		bool ReadLatest(std::string& json) override { json = text; return has; }
		bool has = false;
		std::string text;
	};

	class ExDemand : public IViewerStateDemand
	{
	public:
		void RequestState() override { ++requests; }
		uint32_t requests = 0;
	};
}

TEST(MirrorExhaustiveSnapshot, ArmAloneWritesNothingUntilTheNextUpdate)
{
	ExSnapDomain domain;
	ExSink sink;
	ViewerStateSnapshotPublisher pub(domain, sink);
	pub.Arm(5);
	EXPECT_TRUE(pub.IsArmed());
	EXPECT_EQ(pub.PublishCount(), 0u);
	EXPECT_TRUE(sink.writes.empty());
	EXPECT_EQ(domain.calls, 0u);
	pub.Update(5);
	EXPECT_EQ(sink.writes.size(), 1u);
}

TEST(MirrorExhaustiveSnapshot, WindowEdgeIsInclusiveAndTheFramePastItDisarms)
{
	ExSnapDomain domain;
	ExSink sink;
	ViewerStateSnapshotPublisher pub(domain, sink, 3);
	pub.Arm(10);
	for (uint64_t f = 10; f <= 13; ++f) { pub.Update(f); }
	EXPECT_EQ(sink.writes.size(), 4u) << "armFrames after the arm frame still publishes";
	pub.Update(14);
	EXPECT_EQ(sink.writes.size(), 4u);
	EXPECT_FALSE(pub.IsArmed());
	pub.Update(15);
	EXPECT_EQ(sink.writes.size(), 4u);
	EXPECT_EQ(domain.calls, 4u) << "no GetJSONState once disarmed";
}

TEST(MirrorExhaustiveSnapshot, SimFrameIsStampedByThePublisherOverAnythingTheDomainWrote)
{
	ExSnapDomain domain;
	ExSink sink;
	ViewerStateSnapshotPublisher pub(domain, sink);
	pub.Arm(40);
	pub.Update(42);
	ASSERT_EQ(sink.writes.size(), 1u);
	Json::Value v;
	Json::CharReaderBuilder b;
	std::string errs;
	std::unique_ptr<Json::CharReader> reader(b.newCharReader());
	ASSERT_TRUE(reader->parse(sink.writes[0].data(), sink.writes[0].data() + sink.writes[0].size(), &v, &errs));
	EXPECT_EQ(v["simFrame"].asUInt64(), 42u);
	EXPECT_EQ(v["a"].asInt(), 1);
}

TEST(MirrorExhaustiveSnapshot, SameFrameTwiceWritesIdenticalBytes)
{
	ExSnapDomain domain;
	ExSink sink;
	ViewerStateSnapshotPublisher pub(domain, sink);
	pub.Arm(1);
	pub.Update(7);
	pub.Update(7);
	ASSERT_EQ(sink.writes.size(), 2u);
	EXPECT_EQ(sink.writes[0], sink.writes[1]);
}

TEST(MirrorExhaustiveSnapshot, AdapterKeepsWorkingAfterTheIdentityDomainIsGone)
{
	ExSource source;
	ExDemand demand;
	std::unique_ptr<ViewerStateAdapter> adapter;
	{
		ExSnapDomain identity;
		adapter.reset(new ViewerStateAdapter(identity, source, demand));
	}	// identity destroyed: the adapter copied what it needs
	EXPECT_EQ(adapter->GetDomainId(), StringCRC("ex.domain"));
	EXPECT_STREQ(adapter->GetDisplayName(), "Ex");
	EXPECT_STREQ(adapter->GetDescription(), "Ex domain");
	EXPECT_EQ(adapter->GetGroup(), StringCRC("Spatial"));
	EXPECT_FALSE(adapter->HasWorldDrawers());
	source.has = true;
	source.text = "{\"a\":2,\"simFrame\":3}";
	Json::Value out;
	adapter->GetJSONState(out);
	EXPECT_EQ(out["a"].asInt(), 2);
	EXPECT_EQ(demand.requests, 1u);
}

// ---- re-entry: the viewer has no mutex, so callbacks may call straight back into it ----

namespace
{
	struct ReentryProbe
	{
		MirrorQueries*		queries = nullptr;
		CellLayerViewerDomain*	domain = nullptr;
		ViewStatePublisher*	publisher = nullptr;
		bool				armed = false;
		Json::Value			list, get, stats, state;
		Json::Value			inner;
	};
	ReentryProbe gProbe;

	void ReadThatCallsBackIn(float* out, uint32_t n)
	{
		if (gProbe.armed)
		{
			gProbe.armed = false;
			gProbe.list = gProbe.queries->LayerList();
			gProbe.get = gProbe.queries->SelectionGet(Json::Value(Json::objectValue));
			gProbe.stats = gProbe.queries->SelectionStats(Json::Value(Json::objectValue));
			gProbe.inner = gProbe.queries->LayerValues(OneArg("layer", "beta"));
			gProbe.domain->GetJSONState(gProbe.state);
			gProbe.publisher->Update();
			gProbe.queries->NotifyFrame();
		}
		for (uint32_t i = 0; i < n; ++i) { out[i] = 9.0f; }
	}
	uint64_t VersionOfReentrant() { return 11; }
}

TEST(MirrorExhaustiveReentry, ALayerReadCallbackThatCallsEveryMirrorEntryPointJustWorks)
{
	TestGrid grid{ 4, 3 };
	Dia::Graphics::CellFieldRegistry cellField;
	CountingLayerProvider beta;
	beta.values = std::vector<float>(12, 3.0f);
	FakeDebugDraw draw;
	FakeClock clock;
	TopicRecorder topic;
	FakePuId() = 1;
	CellLayerRegistry registry{ nullptr, &FakePuIdFn };
	LayerUploader uploader{ cellField, nullptr };
	CellLayerViewerDomain domain{ registry, uploader, nullptr };
	MirrorQueries queries{ registry, domain, nullptr };
	ViewStatePublisher publisher{ registry, domain, topic, clock };

	CellLayerDesc outer = MakeLayer("outer", &grid.desc);
	outer.read = fastdelegate::FastDelegate2<float*, uint32_t>(&ReadThatCallsBackIn);
	outer.version = fastdelegate::FastDelegate0<uint64_t>(&VersionOfReentrant);
	ASSERT_EQ(registry.Register(outer), CellRegisterResult::Ok);
	ASSERT_EQ(registry.Register(MakeCountingLayer("beta", &grid.desc, beta)), CellRegisterResult::Ok);
	domain.Update(draw);
	CellSelection* sel = domain.ActiveSelection();
	ASSERT_NE(sel, nullptr);
	sel->BeginEdit(); sel->Set(2, true); sel->EndEdit();

	gProbe = ReentryProbe();
	gProbe.queries = &queries; gProbe.domain = &domain; gProbe.publisher = &publisher;
	gProbe.armed = true;
	const Json::Value r = queries.LayerValues(OneArg("layer", "outer"));
	const bool entered = !gProbe.armed;
	gProbe.queries = nullptr; gProbe.domain = nullptr; gProbe.publisher = nullptr;
	ASSERT_TRUE(entered) << "the reader never called the outer delegate";
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["count"].asUInt(), 12u);
	EXPECT_EQ(r["version"].asUInt64(), 11u);

	EXPECT_FALSE(IsError(gProbe.list));
	EXPECT_EQ(gProbe.list["grids"][0u]["layers"].size(), 2u);
	ASSERT_FALSE(IsError(gProbe.get));
	EXPECT_EQ(gProbe.get["total"].asUInt(), 1u);
	EXPECT_EQ(gProbe.get["indices"][0u].asUInt(), 2u);
	ASSERT_FALSE(IsError(gProbe.inner));
	EXPECT_EQ(gProbe.inner["total"].asUInt(), 12u);
	EXPECT_TRUE(gProbe.state.isObject());
	EXPECT_TRUE(gProbe.stats.isObject());
	EXPECT_EQ(topic.Count(), 1u) << "the publisher ran inside the callback and published once";

	// everything still answers afterwards
	EXPECT_FALSE(IsError(queries.LayerValues(OneArg("layer", "beta"))));
	EXPECT_FALSE(IsError(queries.LayerValues(OneArg("layer", "outer"))));
	EXPECT_FALSE(IsError(queries.SelectionGet(Json::Value(Json::objectValue))));
}

#endif // DIA_DEBUG
