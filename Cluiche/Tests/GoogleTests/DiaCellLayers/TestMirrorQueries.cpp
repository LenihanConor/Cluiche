#include <gtest/gtest.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/SelectionFingerprint.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>

#include <algorithm>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	struct MirrorQueriesTest : ::testing::Test
	{
		TestGrid gridA{ 4, 3 };
		TestGrid gridB{ 2, 2 };
		Dia::Graphics::CellFieldRegistry cellField;
		StubProvider temp, rain;
		FakeDebugDraw draw;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		LayerUploader uploader{ cellField, nullptr };
		CellLayerViewerDomain domain{ registry, uploader, nullptr };
		MirrorQueries queries{ registry, domain, nullptr };

		void SetUp() override
		{
			FakePuId() = 1;
			temp.values = std::vector<float>(12, 1.0f);
			rain.values = std::vector<float>(12, 2.0f);
			ASSERT_EQ(registry.Register(temp.MakeDesc("temp", &gridA.desc)), CellRegisterResult::Ok);
			ASSERT_EQ(registry.Register(MakeLayer("biome", &gridA.desc, "cells/biome", CellValueKind::Category, "Terrain")), CellRegisterResult::Ok);
			ASSERT_EQ(registry.Register(rain.MakeDesc("rain", &gridB.desc)), CellRegisterResult::Ok);
			ASSERT_EQ(registry.RegisterOverlay(MakeOverlay("wind", "Wind", "Air")), CellRegisterResult::Ok);
			ASSERT_EQ(registry.RegisterOverlay(MakeOverlay("rivers", "Rivers", "Water")), CellRegisterResult::Ok);
			domain.Update(draw);
		}

		void Select(const std::vector<uint32_t>& cells)
		{
			CellSelection* sel = domain.ActiveSelection();
			ASSERT_NE(sel, nullptr);
			sel->BeginEdit();
			for (uint32_t c : cells) { sel->Set(c, true); }
			sel->EndEdit();
		}
	};

	// One grid big enough to need two chunks. Layer is a stub; only the selection matters.
	struct MirrorChunkTest : ::testing::Test
	{
		static const uint32_t kCells = MirrorQueries::kMaxSelectionChunk + 1;	// 1,001
		TestGrid grid{ static_cast<uint16_t>(kCells), 1 };
		Dia::Graphics::CellFieldRegistry cellField;
		StubProvider prov;
		FakeDebugDraw draw;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		LayerUploader uploader{ cellField, nullptr };
		CellLayerViewerDomain domain{ registry, uploader, nullptr };
		MirrorQueries queries{ registry, domain, nullptr };

		void SetUp() override
		{
			FakePuId() = 1;
			ASSERT_EQ(registry.Register(prov.MakeDesc("big", &grid.desc)), CellRegisterResult::Ok);
			domain.Update(draw);
		}

		void SelectFirst(uint32_t n)
		{
			CellSelection* sel = domain.ActiveSelection();
			ASSERT_NE(sel, nullptr);
			sel->BeginEdit();
			for (uint32_t i = 0; i < n; ++i) { sel->Set(i, true); }
			sel->EndEdit();
		}
	};

	bool IsError(const Json::Value& v) { return v.isObject() && v.isMember("error") && v["error"].isString(); }

	std::vector<uint32_t> Indices(const Json::Value& v)
	{
		std::vector<uint32_t> out;
		if (!v.isObject() || !v["indices"].isArray()) { return out; }
		for (const Json::Value& i : v["indices"]) { out.push_back(i.asUInt()); }
		return out;
	}

	Json::Value Args(const char* key, const Json::Value& value)
	{
		Json::Value a(Json::objectValue);
		a[key] = value;
		return a;
	}
}

// ---- cells.layer.list ----

TEST_F(MirrorQueriesTest, ListGroupsLayersAndOverlaysByGrid)
{
	const Json::Value r = queries.LayerList();
	ASSERT_TRUE(r["grids"].isArray());
	ASSERT_EQ(r["grids"].size(), 2u);
	const Json::Value& g0 = r["grids"][0u];
	EXPECT_EQ(g0["id"].asString(), "grid.0");
	EXPECT_EQ(g0["width"].asUInt(), 4u);
	EXPECT_EQ(g0["height"].asUInt(), 3u);
	ASSERT_EQ(g0["layers"].size(), 2u);
	if (g0["layers"].size() == 2u)
	{
		EXPECT_EQ(g0["layers"][0u]["id"].asString(), "temp");
		EXPECT_EQ(g0["layers"][0u]["kind"].asString(), "float");
		EXPECT_EQ(g0["layers"][1u]["id"].asString(), "biome");
		EXPECT_EQ(g0["layers"][1u]["kind"].asString(), "category");
		EXPECT_EQ(g0["layers"][1u]["schemaKey"].asString(), "cells/biome");
		EXPECT_EQ(g0["layers"][1u]["group"].asString(), "Terrain");
		EXPECT_TRUE(g0["layers"][1u].isMember("hotkey"));
	}
	const Json::Value& g1 = r["grids"][1u];
	EXPECT_EQ(g1["id"].asString(), "grid.1");
	ASSERT_EQ(g1["layers"].size(), 1u);
	if (g1["layers"].size() == 1u) { EXPECT_EQ(g1["layers"][0u]["id"].asString(), "rain"); }
	ASSERT_TRUE(r["overlays"].isArray());
	ASSERT_EQ(r["overlays"].size(), 2u);
	if (r["overlays"].size() == 2u)
	{
		EXPECT_EQ(r["overlays"][0u]["id"].asString(), "wind");
		EXPECT_EQ(r["overlays"][0u]["label"].asString(), "Wind");
		EXPECT_EQ(r["overlays"][1u]["group"].asString(), "Water");
	}
}

TEST_F(MirrorQueriesTest, ListCarriesLayersVersionAndItBumpsOnRegister)
{
	Json::Value r = queries.LayerList();
	ASSERT_TRUE(r.isMember("layersVersion"));
	EXPECT_EQ(r["layersVersion"].asUInt(), registry.LayersVersion());
	const uint32_t before = r["layersVersion"].asUInt();
	StubProvider extra;
	ASSERT_EQ(registry.Register(extra.MakeDesc("extra", &gridA.desc)), CellRegisterResult::Ok);
	r = queries.LayerList();
	EXPECT_NE(r["layersVersion"].asUInt(), before);
}

TEST_F(MirrorQueriesTest, ListAgreesWithDiaApiCommandData)
{
	Json::Value api(Json::objectValue);
	domain.ListLayers(api);
	const Json::Value q = queries.LayerList();
	ASSERT_EQ(q["grids"].size(), api["grids"].size());
	for (Json::ArrayIndex gi = 0; gi < q["grids"].size() && gi < api["grids"].size(); ++gi)
	{
		const Json::Value& qg = q["grids"][gi];
		const Json::Value& ag = api["grids"][gi];
		EXPECT_EQ(qg["index"].asUInt(), ag["index"].asUInt());
		EXPECT_EQ(qg["width"].asUInt(), ag["width"].asUInt());
		EXPECT_EQ(qg["height"].asUInt(), ag["height"].asUInt());
		ASSERT_EQ(qg["layers"].size(), ag["layers"].size());
		for (Json::ArrayIndex i = 0; i < qg["layers"].size() && i < ag["layers"].size(); ++i)
		{
			EXPECT_EQ(qg["layers"][i]["id"].asString(), ag["layers"][i]["id"].asString());
			EXPECT_EQ(qg["layers"][i]["group"].asString(), ag["layers"][i]["group"].asString());
			EXPECT_EQ(qg["layers"][i]["kind"].asString(), ag["layers"][i]["kind"].asString());
		}
	}
	ASSERT_EQ(q["overlays"].size(), api["overlays"].size());
	for (Json::ArrayIndex i = 0; i < q["overlays"].size() && i < api["overlays"].size(); ++i)
	{
		EXPECT_EQ(q["overlays"][i]["id"].asString(), api["overlays"][i]["id"].asString());
		EXPECT_EQ(q["overlays"][i]["label"].asString(), api["overlays"][i]["label"].asString());
	}
}

TEST_F(MirrorQueriesTest, GridIdHelperMatchesQueryIds)
{
	char id[MirrorGridId::kBufferSize];
	MirrorGridId::Format(MirrorGridId::IndexOf(registry, &gridB.desc), id);
	EXPECT_STREQ(id, "grid.1");
	EXPECT_EQ(MirrorGridId::IndexOf(registry, &gridA.desc), 0u);
}

TEST_F(MirrorQueriesTest, GridIdUsesDescIdWhenSetInListAndValues)
{
	gridA.desc.id = StringCRC("world.hex");
	const Json::Value r = queries.LayerList();
	ASSERT_EQ(r["grids"].size(), 2u);
	EXPECT_EQ(r["grids"][0u]["id"].asString(), "world.hex");
	EXPECT_EQ(r["grids"][1u]["id"].asString(), "grid.1");	// empty id keeps the fallback
	Json::Value a(Json::objectValue);
	a["layer"] = "temp";
	const Json::Value v = queries.LayerValues(a);
	ASSERT_FALSE(IsError(v));
	EXPECT_EQ(v["gridId"].asString(), "world.hex");
}

TEST_F(MirrorQueriesTest, DistinctGridIdsStayDistinct)
{
	gridA.desc.id = StringCRC("test.a");
	gridB.desc.id = StringCRC("test.b");
	const Json::Value r = queries.LayerList();
	ASSERT_EQ(r["grids"].size(), 2u);
	EXPECT_EQ(r["grids"][0u]["id"].asString(), "test.a");
	EXPECT_EQ(r["grids"][1u]["id"].asString(), "test.b");
}

TEST_F(MirrorQueriesTest, GridIdHelperPrefersDescIdElseFallback)
{
	char id[MirrorGridId::kBufferSize];
	MirrorGridId::Format(registry, &gridB.desc, id);
	EXPECT_STREQ(id, "grid.1");
	gridB.desc.id = StringCRC("test.b");
	MirrorGridId::Format(registry, &gridB.desc, id);
	EXPECT_STREQ(id, "test.b");
}

TEST_F(MirrorQueriesTest, RegisteredHandlersAnswer)
{
	FakeMirrorRegistrar qr;
	ASSERT_TRUE(RegisterMirrorQueries(qr, queries));
	ASSERT_TRUE(qr.Has("cells.layer.list"));
	ASSERT_TRUE(qr.Has("cells.select.get"));
	EXPECT_TRUE(qr.Execute("cells.layer.list", Json::Value(Json::nullValue)).isMember("layersVersion"));
	EXPECT_TRUE(qr.Execute("cells.select.get", Json::Value(Json::objectValue)).isMember("indices"));
	UnregisterMirrorQueries(qr);
	EXPECT_FALSE(qr.Has("cells.layer.list"));
}

TEST_F(MirrorQueriesTest, RegistrarReceivesExactlyTheFourQueryNames)
{
	FakeMirrorRegistrar qr;
	ASSERT_TRUE(RegisterMirrorQueries(qr, queries));
	std::vector<std::string> names = qr.Names();
	std::sort(names.begin(), names.end());
	EXPECT_EQ(names, (std::vector<std::string>{ "cells.layer.list", "cells.layer.values", "cells.select.get", "cells.select.stats" }));
	EXPECT_EQ(qr.RegisterCount(), 4u);
	UnregisterMirrorQueries(qr);
	EXPECT_EQ(qr.Count(), 0u);
}

TEST_F(MirrorQueriesTest, RegisterFailureIsReportedAndRolledBack)
{
	FakeMirrorRegistrar qr;
	ASSERT_TRUE(qr.Register("cells.select.get", Dia::DebugServer::SimHelpInfo{"test help"}, [](const Json::Value&) { return Json::Value(Json::objectValue); }));	// name already taken
	EXPECT_FALSE(RegisterMirrorQueries(qr, queries));
	EXPECT_EQ(qr.Count(), 1u);	// only the pre-existing entry remains: the others were rolled back
}

// ---- cells.select.get ----

TEST_F(MirrorQueriesTest, SelectGetReturnsAscendingIndicesFromShuffledInsert)
{
	Select({ 9, 2, 11, 0, 5 });
	const Json::Value r = queries.SelectionGet(Json::Value(Json::objectValue));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(Indices(r), (std::vector<uint32_t>{ 0, 2, 5, 9, 11 }));
	EXPECT_EQ(r["total"].asUInt(), 5u);
	EXPECT_EQ(r["offset"].asUInt(), 0u);
}

TEST_F(MirrorQueriesTest, SelectGetVersionAndHashMatchFingerprint)
{
	Select({ 3, 1, 7 });
	const Json::Value r = queries.SelectionGet(Json::Value(Json::objectValue));
	const ICellSelectionView* view = domain.ActiveSelectionView();
	ASSERT_NE(view, nullptr);
	const SelectionFingerprint fp = SelectionFingerprintFn::Compute(*view);
	char hex[SelectionFingerprintFn::kHexLength + 1];
	SelectionFingerprintFn::ToHex(fp.mHash, hex);
	EXPECT_EQ(r["version"].asUInt(), fp.mVersion);
	EXPECT_EQ(r["hash"].asString(), std::string(hex));
	EXPECT_EQ(r["hash"].asString().size(), 16u);
}

TEST_F(MirrorQueriesTest, SelectGetEmptySelection)
{
	const Json::Value r = queries.SelectionGet(Json::Value(Json::objectValue));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["total"].asUInt(), 0u);
	EXPECT_TRUE(r["indices"].isArray());
	EXPECT_EQ(r["indices"].size(), 0u);
	EXPECT_EQ(r["hash"].asString(), "0000000000000000");
}

TEST_F(MirrorQueriesTest, SelectGetOffsetAndCountPage)
{
	Select({ 0, 1, 2, 3, 4, 5 });
	Json::Value a(Json::objectValue);
	a["offset"] = 2;
	a["count"] = 3;
	const Json::Value r = queries.SelectionGet(a);
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(Indices(r), (std::vector<uint32_t>{ 2, 3, 4 }));
	EXPECT_EQ(r["offset"].asUInt(), 2u);
	EXPECT_EQ(r["total"].asUInt(), 6u);
}

TEST_F(MirrorQueriesTest, SelectGetOffsetEqualTotalIsEmptyList)
{
	Select({ 1, 2, 3 });
	const Json::Value r = queries.SelectionGet(Args("offset", 3));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["indices"].size(), 0u);
	EXPECT_EQ(r["total"].asUInt(), 3u);
}

TEST_F(MirrorQueriesTest, SelectGetOffsetBeyondTotalIsError)
{
	Select({ 1, 2, 3 });
	EXPECT_TRUE(IsError(queries.SelectionGet(Args("offset", 4))));
}

TEST_F(MirrorQueriesTest, SelectGetBadArgsAreErrorsAndChangeNothing)
{
	Select({ 1, 2, 3 });
	const ICellSelectionView* view = domain.ActiveSelectionView();
	ASSERT_NE(view, nullptr);
	const SelectionFingerprint before = SelectionFingerprintFn::Compute(*view);
	EXPECT_TRUE(IsError(queries.SelectionGet(Args("offset", "abc"))));
	EXPECT_TRUE(IsError(queries.SelectionGet(Args("count", "abc"))));
	EXPECT_TRUE(IsError(queries.SelectionGet(Args("offset", -1))));
	EXPECT_TRUE(IsError(queries.SelectionGet(Args("count", -5))));
	EXPECT_TRUE(IsError(queries.SelectionGet(Args("offset", 1.5))));
	EXPECT_TRUE(IsError(queries.SelectionGet(Args("offset", true))));
	EXPECT_TRUE(IsError(queries.SelectionGet(Args("count", Json::Value(Json::nullValue)))));
	EXPECT_EQ(SelectionFingerprintFn::Compute(*domain.ActiveSelectionView()), before);
}

// ---- chunking ----

TEST_F(MirrorChunkTest, DefaultChunkIsOneThousand)
{
	EXPECT_EQ(MirrorQueries::kMaxSelectionChunk, 1000u);
}

TEST_F(MirrorChunkTest, ExactlyOneFullChunk)
{
	SelectFirst(MirrorQueries::kMaxSelectionChunk);
	const Json::Value r = queries.SelectionGet(Json::Value(Json::objectValue));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["total"].asUInt(), MirrorQueries::kMaxSelectionChunk);
	EXPECT_EQ(r["indices"].size(), MirrorQueries::kMaxSelectionChunk);
	const Json::Value next = queries.SelectionGet(Args("offset", MirrorQueries::kMaxSelectionChunk));
	ASSERT_FALSE(IsError(next));
	EXPECT_EQ(next["indices"].size(), 0u);
}

TEST_F(MirrorChunkTest, TwoChunksConcatenateToFullSet)
{
	SelectFirst(kCells);
	const Json::Value first = queries.SelectionGet(Json::Value(Json::objectValue));
	ASSERT_FALSE(IsError(first));
	EXPECT_EQ(first["total"].asUInt(), kCells);
	EXPECT_EQ(first["indices"].size(), MirrorQueries::kMaxSelectionChunk);
	const Json::Value second = queries.SelectionGet(Args("offset", MirrorQueries::kMaxSelectionChunk));
	ASSERT_FALSE(IsError(second));
	EXPECT_EQ(second["indices"].size(), 1u);
	std::vector<uint32_t> all = Indices(first);
	const std::vector<uint32_t> tail = Indices(second);
	all.insert(all.end(), tail.begin(), tail.end());
	ASSERT_EQ(all.size(), kCells);
	if (all.size() == kCells)
	{
		for (uint32_t i = 0; i < kCells; ++i)
		{
			if (all[i] != i) { ADD_FAILURE() << "index mismatch at " << i; break; }
		}
	}
	EXPECT_EQ(first["hash"].asString(), second["hash"].asString());
}

TEST_F(MirrorChunkTest, CountAboveChunkIsClampedWithWarning)
{
	SelectFirst(kCells);
	LogCapture log("CEL");
	const Json::Value r = queries.SelectionGet(Args("count", 100000));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["indices"].size(), MirrorQueries::kMaxSelectionChunk);
	EXPECT_EQ(log.Warnings("clamped"), 1u);
}

TEST_F(MirrorChunkTest, CountZeroReturnsNoIndices)
{
	SelectFirst(5);
	const Json::Value r = queries.SelectionGet(Args("count", 0));
	ASSERT_FALSE(IsError(r));
	EXPECT_EQ(r["indices"].size(), 0u);
	EXPECT_EQ(r["total"].asUInt(), 5u);
}

// ---- Task 10a exhaustive pass ----

namespace
{
	// A 50 x 50 grid (2,500 cells): three select.get pages at the 1,000 limit.
	struct PagingRig
	{
		TestGrid grid{ 50, 50 };
		Dia::Graphics::CellFieldRegistry cellField;
		StubProvider prov;
		FakeDebugDraw draw;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		LayerUploader uploader{ cellField, nullptr };
		CellLayerViewerDomain domain{ registry, uploader, nullptr };
		MirrorQueries queries{ registry, domain, nullptr };

		PagingRig()
		{
			FakePuId() = 1;
			EXPECT_EQ(registry.Register(prov.MakeDesc("big", &grid.desc)), CellRegisterResult::Ok);
			domain.Update(draw);
		}
		void SelectAll(uint32_t n)
		{
			CellSelection* sel = domain.ActiveSelection();
			ASSERT_NE(sel, nullptr);
			sel->BeginEdit();
			for (uint32_t i = 0; i < n; ++i) { sel->Set(i, true); }
			sel->EndEdit();
		}
		Json::Value Page(Json::Int64 offset, Json::Int64 count)
		{
			Json::Value a(Json::objectValue);
			a["offset"] = offset;
			a["count"] = count;
			return queries.SelectionGet(a);
		}
	};
}

TEST(MirrorSelectGetEdge, NoActiveGridIsEmptyAndOnlyOffsetZeroIsValid)
{
	CellLayerRegistry registry{ nullptr, &FakePuIdFn };
	Dia::Graphics::CellFieldRegistry cellField;
	LayerUploader uploader{ cellField, nullptr };
	CellLayerViewerDomain domain{ registry, uploader, nullptr };
	MirrorQueries queries{ registry, domain, nullptr };
	const Json::Value r = queries.SelectionGet(Json::Value(Json::objectValue));
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["total"].asUInt(), 0u);
	EXPECT_EQ(r["hash"].asString(), "0000000000000000");
	EXPECT_EQ(r["indices"].size(), 0u);
	EXPECT_TRUE(IsError(queries.SelectionGet(Args("offset", 1))));
}

TEST(MirrorSelectGetEdge, NonObjectArgsUseDefaults)
{
	PagingRig rig;
	rig.SelectAll(5);
	for (const Json::Value& args : { Json::Value(), Json::Value("x"), Json::Value(7), Json::Value(Json::arrayValue) })
	{
		const Json::Value r = rig.queries.SelectionGet(args);
		ASSERT_FALSE(IsError(r)) << args.toStyledString();
		EXPECT_EQ(r["total"].asUInt(), 5u);
		EXPECT_EQ(r["offset"].asUInt(), 0u);
	}
}

TEST(MirrorSelectGetEdge, ThreePagesAtTheClampConcatenateToTheSortedSet)
{
	PagingRig rig;
	rig.SelectAll(2500);
	std::vector<uint32_t> all;
	uint32_t offset = 0;
	std::vector<size_t> sizes;
	for (int guard = 0; guard < 10; ++guard)
	{
		const Json::Value r = rig.Page(offset, 1000);
		ASSERT_FALSE(IsError(r));
		EXPECT_EQ(r["total"].asUInt(), 2500u);
		EXPECT_EQ(r["offset"].asUInt(), offset);
		const std::vector<uint32_t> idx = Indices(r);
		sizes.push_back(idx.size());
		if (idx.empty()) { break; }
		all.insert(all.end(), idx.begin(), idx.end());
		offset += static_cast<uint32_t>(idx.size());
	}
	EXPECT_EQ(sizes, (std::vector<size_t>{ 1000, 1000, 500, 0 }));
	ASSERT_EQ(all.size(), 2500u);
	for (uint32_t i = 0; i < all.size(); ++i)
	{
		if (all[i] != i) { ADD_FAILURE() << "index mismatch at " << i << ": " << all[i]; break; }
	}
}

TEST(MirrorSelectGetEdge, PageStartingAtTheClampBoundaryAndLastIndex)
{
	PagingRig rig;
	rig.SelectAll(2500);
	const Json::Value mid = rig.Page(999, 2);
	EXPECT_EQ(Indices(mid), (std::vector<uint32_t>{ 999, 1000 }));
	const Json::Value last = rig.Page(2499, 1000);
	EXPECT_EQ(Indices(last), (std::vector<uint32_t>{ 2499 }));
	const Json::Value atTotal = rig.Page(2500, 1000);
	ASSERT_FALSE(IsError(atTotal));
	EXPECT_EQ(Indices(atTotal).size(), 0u);
	EXPECT_TRUE(IsError(rig.Page(2501, 1)));
	const Json::Value huge = rig.Page(0, 4294967295ll);
	ASSERT_FALSE(IsError(huge));
	EXPECT_EQ(Indices(huge).size(), 1000u) << "count is clamped to the chunk limit";
	EXPECT_TRUE(IsError(rig.Page(0, 4294967296ll))) << "2^32 is not a uint32";
	EXPECT_TRUE(IsError(rig.Page(4294967296ll, 1)));
}

TEST(MirrorSelectGetEdge, ClampWarnsOnlyWhenOverTheLimit)
{
	PagingRig rig;
	rig.SelectAll(1200);
	LogCapture log("CEL");
	rig.Page(0, 1000);
	EXPECT_EQ(log.Warnings("clamped"), 0u);
	rig.Page(0, 1001);
	EXPECT_EQ(log.Warnings("clamped"), 1u);
}

TEST(MirrorSelectGetEdge, VersionAndHashTrackTheSelectionBetweenPages)
{
	PagingRig rig;
	rig.SelectAll(1500);
	const Json::Value first = rig.Page(0, 1000);
	rig.domain.ActiveSelection()->BeginEdit();
	rig.domain.ActiveSelection()->Set(2000, true);
	rig.domain.ActiveSelection()->EndEdit();
	const Json::Value second = rig.Page(1000, 1000);
	EXPECT_NE(first["version"].asUInt(), second["version"].asUInt());
	EXPECT_NE(first["hash"].asString(), second["hash"].asString());
	EXPECT_EQ(first["total"].asUInt(), 1500u);
	EXPECT_EQ(second["total"].asUInt(), 1501u);
	EXPECT_EQ(Indices(second).back(), 2000u);
}

TEST(MirrorSelectGetEdge, SelectingTheSameCellTwiceDoesNotChangeCountOrHash)
{
	PagingRig rig;
	rig.SelectAll(3);
	const Json::Value a = rig.queries.SelectionGet(Json::Value(Json::objectValue));
	rig.SelectAll(3);
	const Json::Value b = rig.queries.SelectionGet(Json::Value(Json::objectValue));
	EXPECT_EQ(a["total"], b["total"]);
	EXPECT_EQ(a["hash"], b["hash"]);
}

TEST(MirrorListEdge, EmptyRegistryListsNoGridsAndNoOverlays)
{
	CellLayerRegistry registry{ nullptr, &FakePuIdFn };
	Dia::Graphics::CellFieldRegistry cellField;
	LayerUploader uploader{ cellField, nullptr };
	CellLayerViewerDomain domain{ registry, uploader, nullptr };
	MirrorQueries queries{ registry, domain, nullptr };
	const Json::Value r = queries.LayerList();
	ASSERT_TRUE(r["grids"].isArray());
	EXPECT_EQ(r["grids"].size(), 0u);
	ASSERT_TRUE(r["overlays"].isArray());
	EXPECT_EQ(r["overlays"].size(), 0u);
	EXPECT_TRUE(r["layersVersion"].isNumeric());
}

TEST_F(MirrorQueriesTest, LayersVersionMovesOnUnregisterAndOverlayChangesButNotOnQueriesOrRejectedRegistrations)
{
	const uint32_t v0 = queries.LayerList()["layersVersion"].asUInt();
	queries.LayerList();
	queries.SelectionGet(Json::Value(Json::objectValue));
	EXPECT_EQ(queries.LayerList()["layersVersion"].asUInt(), v0) << "queries never bump it";

	StubProvider dup;
	EXPECT_NE(registry.Register(dup.MakeDesc("temp", &gridA.desc)), CellRegisterResult::Ok) << "duplicate id rejected";
	EXPECT_EQ(queries.LayerList()["layersVersion"].asUInt(), v0) << "a rejected registration does not bump it";

	ASSERT_EQ(registry.RegisterOverlay(MakeOverlay("extra", "Extra", "Air")), CellRegisterResult::Ok);
	const uint32_t v1 = queries.LayerList()["layersVersion"].asUInt();
	EXPECT_NE(v1, v0);
	ASSERT_TRUE(registry.UnregisterOverlay(StringCRC("extra")));
	const uint32_t v2 = queries.LayerList()["layersVersion"].asUInt();
	EXPECT_NE(v2, v1);
	ASSERT_TRUE(registry.Unregister(StringCRC("rain")));
	const uint32_t v3 = queries.LayerList()["layersVersion"].asUInt();
	EXPECT_NE(v3, v2);
	EXPECT_FALSE(registry.Unregister(StringCRC("rain"))) << "already gone";
	EXPECT_EQ(queries.LayerList()["layersVersion"].asUInt(), v3) << "a no-op unregister does not bump it";
}

TEST(MirrorListEdge, SchemaBackedHotkeysAreListedPerGridWithClashesAndUnboundEmpty)
{
	using Dia::ValueSchema::ValueInfo;
	Dia::ValueSchema::ValueSchemaRegistry schema;
	auto addKey = [&](const char* name, const char* hotkey)
	{
		ValueInfo v = Dia::ValueSchema::Testing::MakeValueInfo("cow.cell", name);
		if (hotkey != nullptr) { v.mUi["hotkey"] = hotkey; }
		schema.Register(v);
	};
	addKey("a", "t");
	addKey("b", "t");	// clashes with a on the same grid
	addKey("c", nullptr);
	addKey("d", "m");
	addKey("e", "t");	// same key on another grid: no clash
	schema.Publish();
	Dia::ValueSchema::ValueSchemaService svc(schema);

	TestGrid gridA{ 2, 2 };
	TestGrid gridB{ 2, 2 };
	Dia::Graphics::CellFieldRegistry cellField;
	CellLayerRegistry registry{ &svc, &FakePuIdFn };
	LayerUploader uploader{ cellField, nullptr };
	CellLayerViewerDomain domain{ registry, uploader, &svc };
	MirrorQueries queries{ registry, domain, &svc };
	FakePuId() = 1;
	ASSERT_EQ(registry.Register(MakeLayer("a", &gridA.desc, "cow.cell/a")), CellRegisterResult::Ok);
	ASSERT_EQ(registry.Register(MakeLayer("b", &gridA.desc, "cow.cell/b")), CellRegisterResult::Ok);
	ASSERT_EQ(registry.Register(MakeLayer("c", &gridA.desc, "cow.cell/c")), CellRegisterResult::Ok);
	ASSERT_EQ(registry.Register(MakeLayer("d", &gridA.desc, "cow.cell/d")), CellRegisterResult::Ok);
	ASSERT_EQ(registry.Register(MakeLayer("e", &gridB.desc, "cow.cell/e")), CellRegisterResult::Ok);

	const Json::Value r = queries.LayerList();
	ASSERT_EQ(r["grids"].size(), 2u);
	const Json::Value& la = r["grids"][0u]["layers"];
	ASSERT_EQ(la.size(), 4u);
	EXPECT_EQ(la[0u]["hotkey"].asString(), "t");
	EXPECT_EQ(la[1u]["hotkey"].asString(), "") << "later layer of a clash gets no hotkey";
	EXPECT_EQ(la[2u]["hotkey"].asString(), "") << "no ui.hotkey in the schema";
	EXPECT_EQ(la[3u]["hotkey"].asString(), "m");
	EXPECT_EQ(la[0u]["schemaKey"].asString(), "cow.cell/a");
	const Json::Value& lb = r["grids"][1u]["layers"];
	ASSERT_EQ(lb.size(), 1u);
	EXPECT_EQ(lb[0u]["hotkey"].asString(), "t");
}

#endif // DIA_DEBUG
