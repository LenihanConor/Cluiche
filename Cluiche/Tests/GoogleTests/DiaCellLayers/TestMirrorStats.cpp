#include <gtest/gtest.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/SelectionFingerprint.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaCellLayers/Testing/ViewerCommands.h>
#include <DiaAPI/DiaAPI.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

// cells.select.stats (editor mirror, Feature 5 task 6). Rig: 20x20 grid. temp (float, base) = index * 0.5; terrain (category,
// index % 2); rain (float) = 10; wind (float) = index % 4. Shown set = temp + terrain. Box (0,0)-(2,2) = {0,1,2,20,21,22,40,41,42}.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	struct StatsRig
	{
		TestGrid grid{ 20, 20 };
		Dia::Graphics::CellFieldRegistry cellField;
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		StubProvider temp, terrain, rain, wind;
		FakeDebugDraw draw;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		LayerUploader uploader{ cellField, nullptr };
		std::unique_ptr<CellLayerViewerDomain> domain;
		std::unique_ptr<ViewerCommands> commands;
		std::unique_ptr<MirrorQueries> queries;

		static const Dia::ValueSchema::ValueSchemaService* Build(UploaderSchemaFixture& f)
		{
			Dia::ValueSchema::ValueInfo t = Dia::ValueSchema::Testing::MakeValueInfo("cells", "terrain");
			t.mType = Dia::ValueSchema::ValueType::Enum;
			Dia::ValueSchema::SetText(t.mLabel, "Terrain");
			t.mEnumValues.Reserve(2);
			const char* ids[] = { "grass", "water" };
			for (int i = 0; i < 2; ++i)
			{
				Dia::ValueSchema::EnumValue v;
				Dia::ValueSchema::SetText(v.mId, ids[i]);
				Dia::ValueSchema::SetText(v.mLabel, ids[i]);
				t.mEnumValues.Add(v);
			}
			f.Add(t);
			f.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 200));
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

		StatsRig()
		{
			FakePuId() = 1;
			service = Build(fx);
			domain.reset(new CellLayerViewerDomain(registry, uploader, service));
			commands.reset(new ViewerCommands(*domain));
			queries.reset(new MirrorQueries(registry, *domain, service));
			const size_t n = 400;
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
			domain->Update(draw);
		}

		void Select(const std::vector<uint32_t>& cells)
		{
			CellSelection* sel = domain->ActiveSelection();
			ASSERT_NE(sel, nullptr);
			sel->BeginEdit();
			for (uint32_t c : cells) { sel->Set(c, true); }
			sel->EndEdit();
		}
		void SelectBox() { Select({ 0, 1, 2, 20, 21, 22, 40, 41, 42 }); }
	};

	struct MirrorStatsTest : ::testing::Test
	{
		void SetUp() override { FakePuId() = 1; Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};

	bool IsError(const Json::Value& v) { return v.isObject() && v.isMember("error") && v["error"].isString(); }

	Json::Value Args(const char* key, const Json::Value& value)
	{
		Json::Value a(Json::objectValue);
		a[key] = value;
		return a;
	}

	Json::Value LayerArray(std::initializer_list<const char*> ids)
	{
		Json::Value a(Json::arrayValue);
		for (const char* i : ids) { a.append(i); }
		return a;
	}

	std::string LayerNames(const Json::Value& r)
	{
		std::string s;
		if (!r.isObject() || !r["layers"].isArray()) { return "<none>"; }
		for (const Json::Value& l : r["layers"]) { s += l["layer"].asString() + ","; }
		return s;
	}

	Json::Value Inspect(const Json::Value& params)
	{
		return ViewerCommands::Exec("cells.inspect", params);
	}
}

TEST_F(MirrorStatsTest, DefaultIsTheInspectorShownSet)
{
	StatsRig rig;
	rig.SelectBox();
	const Json::Value r = rig.queries->SelectionStats(Json::Value(Json::objectValue));
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(LayerNames(r), "temp,terrain,");
	EXPECT_EQ(r["selected"].asUInt(), 9u);
}

TEST_F(MirrorStatsTest, NullArgsAreTheDefaultToo)
{
	StatsRig rig;
	rig.SelectBox();
	const Json::Value r = rig.queries->SelectionStats(Json::Value(Json::nullValue));
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(LayerNames(r), "temp,terrain,");
}

TEST_F(MirrorStatsTest, AllIsEveryLayerOfTheActiveGrid)
{
	StatsRig rig;
	rig.SelectBox();
	const Json::Value r = rig.queries->SelectionStats(Args("layers", "all"));
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(LayerNames(r), "temp,terrain,rain,wind,");
}

TEST_F(MirrorStatsTest, ExplicitListIsInRequestOrder)
{
	StatsRig rig;
	rig.SelectBox();
	const Json::Value r = rig.queries->SelectionStats(Args("layers", LayerArray({ "wind", "temp" })));
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(LayerNames(r), "wind,temp,");
	ASSERT_EQ(r["layers"].size(), 2u);
	if (r["layers"].size() == 2u)
	{
		EXPECT_EQ(r["layers"][0u]["mean"].asDouble(), 1.0);	// wind over the box
		EXPECT_EQ(r["layers"][1u]["max"].asDouble(), 21.0);
	}
}

TEST_F(MirrorStatsTest, PerLayerShapeEqualsCellsInspect)
{
	StatsRig rig;
	rig.SelectBox();
	const Json::Value mine = rig.queries->SelectionStats(Args("layers", LayerArray({ "wind", "terrain", "temp" })));
	const Json::Value theirs = Inspect(Args("layers", LayerArray({ "wind", "terrain", "temp" })));
	ASSERT_FALSE(IsError(mine)) << mine.toStyledString();
	ASSERT_TRUE(theirs["success"].asBool()) << theirs.toStyledString();
	EXPECT_EQ(mine["layers"], theirs["data"]["layers"]);
	EXPECT_EQ(mine["base"], theirs["data"]["base"]);
	EXPECT_EQ(mine["histogram"], theirs["data"]["histogram"]);
	EXPECT_EQ(mine["grid"], theirs["data"]["grid"]);
	EXPECT_EQ(mine["selected"], theirs["data"]["selected"]);

	const Json::Value mineAll = rig.queries->SelectionStats(Args("layers", "all"));
	const Json::Value theirsAll = Inspect(Args("all", true));
	ASSERT_TRUE(theirsAll["success"].asBool());
	EXPECT_EQ(mineAll["layers"], theirsAll["data"]["layers"]);

	const Json::Value mineDef = rig.queries->SelectionStats(Json::Value(Json::objectValue));
	const Json::Value theirsDef = Inspect(Json::Value(Json::objectValue));
	EXPECT_EQ(mineDef["layers"], theirsDef["data"]["layers"]);
}

TEST_F(MirrorStatsTest, CountVersionHashEqualTheSelectionFingerprint)
{
	StatsRig rig;
	rig.SelectBox();
	const ICellSelectionView* sel = rig.domain->ActiveSelectionView();
	ASSERT_NE(sel, nullptr);
	const SelectionFingerprint fp = SelectionFingerprintFn::Compute(*sel);
	char hex[SelectionFingerprintFn::kHexLength + 1];
	SelectionFingerprintFn::ToHex(fp.mHash, hex);
	const Json::Value r = rig.queries->SelectionStats(Json::Value(Json::objectValue));
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["count"].asUInt(), fp.mCount);
	EXPECT_EQ(r["version"].asUInt(), fp.mVersion);
	EXPECT_EQ(r["hash"].asString(), std::string(hex));
	EXPECT_EQ(r["hash"].asString().size(), 16u);
	EXPECT_EQ(fp.mCount, 9u);
}

TEST_F(MirrorStatsTest, EmptySelection)
{
	StatsRig rig;
	const Json::Value r = rig.queries->SelectionStats(Args("layers", "all"));
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["count"].asUInt(), 0u);
	EXPECT_EQ(r["hash"].asString(), "0000000000000000");
	ASSERT_TRUE(r["layers"].isArray());
	EXPECT_EQ(r["layers"].size(), 0u);
}

TEST_F(MirrorStatsTest, StatsAggregatedOncePerFrame)
{
	StatsRig rig;
	rig.SelectBox();
	const uint32_t before = rig.queries->StatsComputeCount();
	const Json::Value a = rig.queries->SelectionStats(Args("layers", "all"));
	const Json::Value b = rig.queries->SelectionStats(Args("layers", "all"));
	ASSERT_FALSE(IsError(a)) << a.toStyledString();
	EXPECT_EQ(rig.queries->StatsComputeCount(), before + 1);
	EXPECT_EQ(a, b);
	const uint32_t reads = rig.temp.readCalls + rig.rain.readCalls + rig.wind.readCalls + rig.terrain.readCalls;
	rig.queries->SelectionStats(Args("layers", "all"));
	EXPECT_EQ(reads, rig.temp.readCalls + rig.rain.readCalls + rig.wind.readCalls + rig.terrain.readCalls);	// no layer re-read either
}

TEST_F(MirrorStatsTest, NotifyFrameEndsTheCacheEntry)
{
	StatsRig rig;
	rig.SelectBox();
	rig.queries->SelectionStats(Args("layers", "all"));
	const uint32_t c1 = rig.queries->StatsComputeCount();
	rig.queries->NotifyFrame();
	rig.queries->SelectionStats(Args("layers", "all"));
	EXPECT_EQ(rig.queries->StatsComputeCount(), c1 + 1);
	rig.queries->SelectionStats(Args("layers", "all"));
	EXPECT_EQ(rig.queries->StatsComputeCount(), c1 + 1);
}

TEST_F(MirrorStatsTest, SelectionChangeRecomputesWithinTheFrame)
{
	StatsRig rig;
	rig.SelectBox();
	const Json::Value a = rig.queries->SelectionStats(Json::Value(Json::objectValue));
	const uint32_t c1 = rig.queries->StatsComputeCount();
	rig.Select({ 100 });
	const Json::Value b = rig.queries->SelectionStats(Json::Value(Json::objectValue));
	EXPECT_EQ(rig.queries->StatsComputeCount(), c1 + 1);
	EXPECT_NE(a["count"].asUInt(), b["count"].asUInt());
	EXPECT_NE(a["hash"].asString(), b["hash"].asString());
}

TEST_F(MirrorStatsTest, LayerVersionChangeRecomputesWithinTheFrame)
{
	StatsRig rig;
	rig.SelectBox();
	const Json::Value a = rig.queries->SelectionStats(Args("layers", "all"));
	const uint32_t c1 = rig.queries->StatsComputeCount();
	for (float& v : rig.rain.values) { v = 50.0f; }
	rig.rain.versionValue++;
	const Json::Value b = rig.queries->SelectionStats(Args("layers", "all"));
	EXPECT_EQ(rig.queries->StatsComputeCount(), c1 + 1);
	EXPECT_NE(a, b);
}

TEST_F(MirrorStatsTest, DifferentLayerSetRecomputes)
{
	StatsRig rig;
	rig.SelectBox();
	rig.queries->SelectionStats(Args("layers", "all"));
	const uint32_t c1 = rig.queries->StatsComputeCount();
	const Json::Value b = rig.queries->SelectionStats(Args("layers", LayerArray({ "wind" })));
	EXPECT_EQ(rig.queries->StatsComputeCount(), c1 + 1);
	EXPECT_EQ(LayerNames(b), "wind,");
}

TEST_F(MirrorStatsTest, AlternatingLayerSetsInOneFrameComputeOncePerSet)
{
	StatsRig rig;
	rig.SelectBox();
	const uint32_t c0 = rig.queries->StatsComputeCount();
	const Json::Value argsA = Args("layers", LayerArray({ "temp", "rain" }));
	const Json::Value argsB = Args("layers", LayerArray({ "wind" }));
	const Json::Value a1 = rig.queries->SelectionStats(argsA);
	const Json::Value b1 = rig.queries->SelectionStats(argsB);
	const Json::Value a2 = rig.queries->SelectionStats(argsA);
	const Json::Value b2 = rig.queries->SelectionStats(argsB);
	ASSERT_FALSE(IsError(a1)) << a1.toStyledString();
	ASSERT_FALSE(IsError(b1)) << b1.toStyledString();
	EXPECT_EQ(rig.queries->StatsComputeCount(), c0 + 2);
	EXPECT_EQ(a1, a2);
	EXPECT_EQ(b1, b2);
	EXPECT_EQ(LayerNames(a2), "temp,rain,");
	EXPECT_EQ(LayerNames(b2), "wind,");
}

TEST_F(MirrorStatsTest, MoreRequestsThanCapacityStillCorrectAndNotifyFrameClearsAll)
{
	StatsRig rig;
	rig.SelectBox();
	const char* names[] = { "temp", "terrain", "rain", "wind" };
	std::vector<std::vector<const char*>> sets;
	for (const char* x : names) { for (const char* y : names) { if (x != y) { sets.push_back({ x, y }); } } }	// 12 ordered pairs
	ASSERT_GT(sets.size(), static_cast<size_t>(MirrorQueries::kStatsCacheCapacity));

	auto request = [&](const std::vector<const char*>& s)
	{
		Json::Value arr(Json::arrayValue);
		for (const char* n : s) { arr.append(n); }
		return rig.queries->SelectionStats(Args("layers", arr));
	};
	auto expectedNames = [](const std::vector<const char*>& s) { std::string r; for (const char* n : s) { r += std::string(n) + ","; } return r; };

	const uint32_t c0 = rig.queries->StatsComputeCount();
	std::vector<Json::Value> first;
	for (const auto& s : sets) { first.push_back(request(s)); }
	EXPECT_EQ(rig.queries->StatsComputeCount(), c0 + static_cast<uint32_t>(sets.size()));

	const uint32_t c1 = rig.queries->StatsComputeCount();
	request(sets.back());	// most recent entry is still cached
	EXPECT_EQ(rig.queries->StatsComputeCount(), c1);

	for (size_t i = 0; i < sets.size(); ++i)	// every request, evicted or not, returns the correct result
	{
		const Json::Value again = request(sets[i]);
		EXPECT_EQ(again, first[i]) << i;
		EXPECT_EQ(LayerNames(again), expectedNames(sets[i])) << i;
	}

	rig.queries->NotifyFrame();
	const uint32_t c2 = rig.queries->StatsComputeCount();
	for (size_t i = 0; i < 3; ++i) { request(sets[i]); }
	EXPECT_EQ(rig.queries->StatsComputeCount(), c2 + 3);	// nothing survived the frame
	for (size_t i = 0; i < 3; ++i) { request(sets[i]); }
	EXPECT_EQ(rig.queries->StatsComputeCount(), c2 + 3);
}

TEST_F(MirrorStatsTest, ErrorsReturnErrorObjectsAndChangeNothing)
{
	StatsRig rig;
	rig.SelectBox();
	const Json::Value good = rig.queries->SelectionStats(Json::Value(Json::objectValue));
	const uint32_t computes = rig.queries->StatsComputeCount();

	std::vector<Json::Value> bad;
	bad.push_back(Args("layers", LayerArray({ "nope" })));
	bad.push_back(Args("layers", LayerArray({ "temp", "nope" })));
	bad.push_back(Args("layers", "everything"));
	bad.push_back(Args("layers", 5));
	bad.push_back(Args("layers", Json::Value(true)));
	bad.push_back(Args("layers", Json::Value(Json::objectValue)));
	bad.push_back(Args("layers", Json::Value(Json::arrayValue)));
	{
		Json::Value mixed(Json::arrayValue);
		mixed.append("temp");
		mixed.append(7);
		bad.push_back(Args("layers", mixed));
	}
	bad.push_back(Args("layers", LayerArray({ "" })));
	bad.push_back(Json::Value(Json::arrayValue));
	for (const Json::Value& b : bad)
	{
		const Json::Value r = rig.queries->SelectionStats(b);
		EXPECT_TRUE(IsError(r)) << b.toStyledString() << " -> " << r.toStyledString();
	}
	EXPECT_EQ(rig.queries->StatsComputeCount(), computes);
	EXPECT_EQ(rig.queries->SelectionStats(Json::Value(Json::objectValue)), good);	// state intact
}

TEST_F(MirrorStatsTest, ReadOnly)
{
	StatsRig rig;
	rig.SelectBox();
	const ICellSelectionView* sel = rig.domain->ActiveSelectionView();
	ASSERT_NE(sel, nullptr);
	const SelectionFingerprint before = SelectionFingerprintFn::Compute(*sel);
	const uint32_t passes = rig.domain->Inspector().PassCount();
	const uint32_t layersVersion = rig.registry.LayersVersion();
	rig.queries->SelectionStats(Args("layers", "all"));
	rig.queries->SelectionStats(Json::Value(Json::objectValue));
	rig.queries->SelectionStats(Args("layers", "bogus"));
	EXPECT_EQ(SelectionFingerprintFn::Compute(*sel), before);
	EXPECT_EQ(rig.domain->Inspector().PassCount(), passes);
	EXPECT_EQ(rig.registry.LayersVersion(), layersVersion);
	EXPECT_FALSE(rig.domain->Inspector().IsPinned());
}

TEST_F(MirrorStatsTest, RegisteredAndUnregistered)
{
	StatsRig rig;
	rig.SelectBox();
	FakeMirrorRegistrar qr;
	ASSERT_TRUE(RegisterMirrorQueries(qr, *rig.queries));
	ASSERT_TRUE(qr.Has("cells.select.stats"));
	const Json::Value r = qr.Execute("cells.select.stats", Args("layers", "all"));
	EXPECT_EQ(LayerNames(r), "temp,terrain,rain,wind,");
	UnregisterMirrorQueries(qr);
	EXPECT_FALSE(qr.Has("cells.select.stats"));
}

TEST_F(MirrorStatsTest, TwentyLayerResponseSize)
{
	TestGrid grid{ 20, 20 };
	Dia::Graphics::CellFieldRegistry cellField;
	UploaderSchemaFixture fx;
	std::vector<std::unique_ptr<StubProvider>> provs;
	std::vector<std::string> names;
	for (int i = 0; i < 20; ++i)
	{
		names.push_back("layer" + std::to_string(i));
		fx.Add(UploaderSchemaFixture::FloatEntry(names.back().c_str(), 0, 100));
	}
	const Dia::ValueSchema::ValueSchemaService* service = &fx.Publish();
	FakeDebugDraw draw;
	CellLayerRegistry registry{ nullptr, &FakePuIdFn };
	LayerUploader uploader{ cellField, nullptr };
	CellLayerViewerDomain domain{ registry, uploader, service };
	for (int i = 0; i < 20; ++i)
	{
		provs.emplace_back(new StubProvider());
		provs.back()->values.resize(400);
		for (size_t c = 0; c < 400; ++c) { provs.back()->values[c] = static_cast<float>((c * (i + 3)) % 97) + 0.37f; }
		CellLayerDesc d = provs.back()->MakeDesc(names[i].c_str(), &grid.desc, CellValueKind::Float, ("cells/" + names[i]).c_str());
		ASSERT_EQ(CellRegisterResult::Ok, registry.Register(d));
	}
	domain.Update(draw);
	MirrorQueries queries(registry, domain, service);
	CellSelection* sel = domain.ActiveSelection();
	ASSERT_NE(sel, nullptr);
	sel->BeginEdit();
	for (uint32_t c = 0; c < 300; ++c) { sel->Set(c, true); }
	sel->EndEdit();
	const Json::Value r = queries.SelectionStats(Args("layers", "all"));
	ASSERT_FALSE(IsError(r)) << r.toStyledString();
	EXPECT_EQ(r["layers"].size(), 20u);
	Json::StreamWriterBuilder wb;
	wb["indentation"] = "";
	const size_t bytes = Json::writeString(wb, r).size();
	RecordProperty("StatsResponseBytes20Layers", static_cast<int>(bytes));
	std::cout << "[ INFO ] 20-layer cells.select.stats response: " << bytes << " bytes\n";
	EXPECT_LT(bytes, 1024u * 1024u);
}

#endif // DIA_DEBUG
