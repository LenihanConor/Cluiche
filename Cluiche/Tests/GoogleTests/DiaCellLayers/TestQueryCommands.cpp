#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/Testing/ViewerCommands.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>
#include <DiaAPI/DiaAPI.h>

#include <chrono>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef DIA_DEBUG

// cells.select.query and cells.query.check: parse + resolve + evaluate a query against the active grid.
// select replaces the selection; check never touches it. Args {"expr": "..."}.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;
typedef LogCapture::Level Level;

namespace
{
	// Enum values: grass 0, water 1, rock 2, sand 3, unused 4. terrain[i] = i % 4; temp = 1.
	struct Rig
	{
		TestGrid grid;
		CellFieldRegistry cellField;
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		StubProvider terrain, temp, la, lb;
		FakeDebugDraw draw;
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;
		ViewerCommands commands{ domain };

		static const Dia::ValueSchema::ValueSchemaService* Build(UploaderSchemaFixture& f)
		{
			Dia::ValueSchema::ValueInfo t = Dia::ValueSchema::Testing::MakeValueInfo("cells", "terrain");
			t.mType = Dia::ValueSchema::ValueType::Enum;
			t.mEnumValues.Reserve(5);
			for (const char* id : { "grass", "water", "rock", "sand", "unused" })
			{
				Dia::ValueSchema::EnumValue v;
				Dia::ValueSchema::SetText(v.mId, id);
				t.mEnumValues.Add(v);
			}
			f.Add(t);
			f.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
			f.Add(UploaderSchemaFixture::FloatEntry("a/x", 0, 1));
			f.Add(UploaderSchemaFixture::FloatEntry("b/x", 0, 1));
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
			terrain.values.resize(n);
			for (size_t i = 0; i < n; ++i) { terrain.values[i] = static_cast<float>(i % 4); }
			temp.values = std::vector<float>(n, 1.0f);
			la.values = std::vector<float>(n, 0.5f);
			lb.values = std::vector<float>(n, 0.5f);
			registry.Register(terrain.MakeDesc("terrain", &grid.desc, CellValueKind::Category, "cells/terrain"));
			registry.Register(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
			registry.Register(la.MakeDesc("la", &grid.desc, CellValueKind::Float, "cells/a/x"));
			registry.Register(lb.MakeDesc("lb", &grid.desc, CellValueKind::Float, "cells/b/x"));
			domain.Update(draw);
		}

		uint32_t WaterCount() const
		{
			uint32_t n = 0;
			for (uint32_t i = 0; i < grid.CellCount(); ++i) { if (i % 4 == 1) { ++n; } }
			return n;
		}
	};

	Json::Value Expr(const char* text)
	{
		Json::Value p(Json::objectValue);
		p["expr"] = text;
		return p;
	}

	Json::Value Exec(const char* cmd, const Json::Value& params) { return ViewerCommands::Exec(cmd, params); }

	struct QueryCommandsTest : ::testing::Test
	{
		void SetUp() override { FakePuId() = 1; Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};
}

TEST_F(QueryCommandsTest, SelectQueryReplacesSelectionAndReportsCount)
{
	Rig rig(66, 42);
	rig.domain.ActiveSelection()->Set(0, true);	// grass cell, must be replaced
	const Json::Value r = Exec("cells.select.query", Expr("terrain == water"));
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_TRUE(r["data"]["ok"].asBool());
	EXPECT_EQ(r["data"]["count"].asUInt(), rig.WaterCount());
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), rig.WaterCount());
	EXPECT_FALSE(rig.domain.ActiveSelectionView()->Contains(0));
	EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(1));
}

TEST_F(QueryCommandsTest, CheckAgreesWithSelectCountAndNeverTouchesSelection)
{
	Rig rig(66, 42);
	rig.domain.ActiveSelection()->Set(0, true);
	const uint32_t version = rig.domain.ActiveSelectionView()->Version();
	const Json::Value c = Exec("cells.query.check", Expr("terrain == water and temp >= 1"));
	ASSERT_TRUE(c["success"].asBool());
	EXPECT_TRUE(c["data"]["ok"].asBool());
	EXPECT_EQ(c["data"]["count"].asUInt(), rig.WaterCount());
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Version(), version);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 1u);
	EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(0));

	const Json::Value s = Exec("cells.select.query", Expr("terrain == water and temp >= 1"));
	EXPECT_EQ(s["data"]["count"].asUInt(), c["data"]["count"].asUInt());
}

TEST_F(QueryCommandsTest, VersionBumpsOnceOnSuccessAndNotOnRepeat)
{
	Rig rig(66, 42);
	const uint32_t v0 = rig.domain.ActiveSelectionView()->Version();
	ASSERT_TRUE(Exec("cells.select.query", Expr("terrain == water"))["data"]["ok"].asBool());
	const uint32_t v1 = rig.domain.ActiveSelectionView()->Version();
	EXPECT_EQ(v1, v0 + 1u);
	ASSERT_TRUE(Exec("cells.select.query", Expr("terrain == water"))["data"]["ok"].asBool());
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Version(), v1);
}

TEST_F(QueryCommandsTest, ErrorsReturnObjectAndLeaveSelectionAndVersionUnchanged)
{
	struct Bad { const char* expr; const char* messagePart; bool suggestion; };
	const Bad bads[] = {
		{ "terrain ==", "", false },							// parse error
		{ "terrian == water", "", true },						// unknown layer, close suggestion
		{ "terrain > 1", "", false },							// numeric op on a category layer
		{ "x > 0.1", "ambiguous", false },						// ambiguous short key
	};
	for (const char* cmd : { "cells.select.query", "cells.query.check" })
	{
		Rig rig(66, 42);
		rig.domain.ActiveSelection()->Set(5, true);
		const uint32_t version = rig.domain.ActiveSelectionView()->Version();
		for (const Bad& b : bads)
		{
			const Json::Value r = Exec(cmd, Expr(b.expr));
			ASSERT_FALSE(r["success"].asBool()) << cmd << " " << b.expr;	// DiaAPI maps a result with "error" to success=false
			const Json::Value& e = r["error"];
			ASSERT_TRUE(e.isObject()) << cmd << " " << b.expr;
			EXPECT_FALSE(e["message"].asString().empty()) << cmd << " " << b.expr;
			EXPECT_NE(e["message"].asString().find(b.messagePart), std::string::npos) << cmd << " " << b.expr;
			EXPECT_TRUE(e["column"].isUInt());
			EXPECT_TRUE(e["length"].isUInt());
			EXPECT_EQ(e.isMember("suggestion"), b.suggestion) << cmd << " " << b.expr;
			if (b.suggestion) { EXPECT_EQ(e["suggestion"].asString(), "terrain"); }
			EXPECT_EQ(rig.domain.ActiveSelectionView()->Version(), version) << cmd << " " << b.expr;
			EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 1u) << cmd << " " << b.expr;
			EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(5));
		}
	}
}

TEST_F(QueryCommandsTest, EmptyOrMissingExprIsAnErrorObject)
{
	Rig rig(66, 42);
	const uint32_t version = rig.domain.ActiveSelectionView()->Version();
	for (const char* cmd : { "cells.select.query", "cells.query.check" })
	{
		const Json::Value cases[] = { Expr(""), Json::Value(Json::objectValue), Expr("   ") };
		for (const Json::Value& p : cases)
		{
			const Json::Value r = Exec(cmd, p);
			ASSERT_FALSE(r["success"].asBool());
			EXPECT_EQ(r["error"]["message"].asString(), "empty query");
			EXPECT_EQ(r["error"]["column"].asUInt(), 0u);
			EXPECT_EQ(r["error"]["length"].asUInt(), 0u);
		}
	}
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Version(), version);
}

TEST_F(QueryCommandsTest, NoActiveGridIsAnErrorObject)
{
	UploaderSchemaFixture fx;
	CellFieldRegistry cellField;
	FakeDebugDraw draw;
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	LayerUploader uploader(cellField, nullptr);
	CellLayerViewerDomain domain(registry, uploader, &fx.Publish());
	ViewerCommands commands(domain);
	domain.Update(draw);
	for (const char* cmd : { "cells.select.query", "cells.query.check" })
	{
		const Json::Value r = Exec(cmd, Expr("temp > 1"));
		ASSERT_FALSE(r["success"].asBool());
		EXPECT_EQ(r["error"]["message"].asString(), "no active grid");
	}
}

TEST_F(QueryCommandsTest, SelectLogsInfoForRunAndRejectAndCheckIsSilent)
{
	Rig rig(66, 42);
	LogCapture log;
	Exec("cells.select.query", Expr("terrain == water"));
	EXPECT_EQ(log.Count(Level::kInfo, "query run"), 1u);
	EXPECT_EQ(log.Count(Level::kInfo, "terrain == water"), 1u);
	log.Clear();
	Exec("cells.select.query", Expr("terrain >"));
	EXPECT_EQ(log.Count(Level::kInfo, "rejected"), 1u);
	log.Clear();
	Exec("cells.query.check", Expr("terrain == water"));
	Exec("cells.query.check", Expr("terrain >"));
	EXPECT_EQ(log.Count(Level::kInfo), 0u);
}

// ---- Task 10a exhaustive pass (Feature 3) ----

namespace
{
	// ---- reentrancy: a layer read delegate that calls back into the domain while a query is evaluating. The viewer has no
	// mutex now, so this must neither deadlock nor corrupt the outer query. A watchdog turns a hang into a failure. ----
	CellLayerViewerDomain* gDomain = nullptr;
	bool gArmed = false;
	uint32_t gReentryCalls = 0;
	std::string gNestedActiveBase;
	size_t gNestedGridCount = 0;
	bool gNestedQueryRejected = false;
	std::string gNestedQueryMessage;
	const char* gNestedExpr = nullptr;
	const char* gNestedCmd = nullptr;

	void ReadReentering(float* out, uint32_t n)
	{
		for (uint32_t i = 0; i < n; ++i) { out[i] = 1.0f; }
		if (!gArmed) { return; }
		gArmed = false;	// re-enter once: the nested query must not recurse into this layer again
		++gReentryCalls;
		Json::Value list;
		gDomain->ListLayers(list);
		gNestedGridCount = list["grids"].size();
		gNestedActiveBase = list["activeBase"].asString();
		Json::Value state(Json::objectValue);
		gDomain->GetJSONState(state);
		if (gNestedCmd != nullptr)
		{
			const Json::Value r = Exec(gNestedCmd, Expr(gNestedExpr));
			gNestedQueryRejected = !r["success"].asBool();
			if (gNestedQueryRejected) { gNestedQueryMessage = r["error"]["message"].asString(); }
		}
	}

	template <typename Fn>
	bool FinishesWithin(std::chrono::milliseconds window, Fn fn)
	{
		auto task = std::make_shared<std::packaged_task<void()>>(fn);
		std::future<void> done = task->get_future();
		std::thread([task] { (*task)(); }).detach();
		if (done.wait_for(window) != std::future_status::ready) { return false; }
		try { done.get(); }
		catch (const std::exception& e) { ADD_FAILURE() << "callback path threw: " << e.what(); return false; }
		return true;
	}

	struct ReentryTest : QueryCommandsTest
	{
		void SetUp() override
		{
			QueryCommandsTest::SetUp();
			gArmed = false; gReentryCalls = 0; gNestedActiveBase.clear(); gNestedGridCount = 0;
			gNestedQueryRejected = false; gNestedQueryMessage.clear(); gNestedExpr = nullptr; gNestedCmd = nullptr;
		}
		void TearDown() override { gDomain = nullptr; QueryCommandsTest::TearDown(); }

		static void AddReentrantLayer(Rig& rig)
		{
			CellLayerDesc d = MakeLayer("rd", &rig.grid.desc);
			d.read = fastdelegate::FastDelegate2<float*, uint32_t>(&ReadReentering);
			ASSERT_EQ(rig.registry.Register(d), CellRegisterResult::Ok);
			gDomain = &rig.domain;
		}
	};

	// Two grids: A (temp) and B (bx); SetBase switches the active grid.
	struct TwoGridRig
	{
		TestGrid gridA, gridB;
		CellFieldRegistry cellField;
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service;
		StubProvider tempA, bx;
		FakeDebugDraw draw;
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;
		ViewerCommands commands{ domain };

		static const Dia::ValueSchema::ValueSchemaService* Build(UploaderSchemaFixture& f)
		{
			f.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
			f.Add(UploaderSchemaFixture::FloatEntry("bx", 0, 10));
			return &f.Publish();
		}

		TwoGridRig()
			: gridA(4, 4), gridB(2, 2)
			, service(Build(fx))
			, registry(nullptr, &FakePuIdFn)
			, uploader(cellField, nullptr)
			, domain(registry, uploader, service)
		{
			tempA.values = std::vector<float>(16, 1.0f);
			bx.values = { 1, 2, 3, 4 };
			registry.Register(tempA.MakeDesc("temp", &gridA.desc, CellValueKind::Float, "cells/temp"));
			registry.Register(bx.MakeDesc("bx", &gridB.desc, CellValueKind::Float, "cells/bx"));
			domain.Update(draw);
		}
	};
}

TEST_F(QueryCommandsTest, RawReplyShapesAreOkCountAndOkFalseErrorObject)
{
	Rig rig(66, 42);
	const Json::Value ok = rig.commands.Raw("cells.select.query", Expr("terrain == water"));
	ASSERT_TRUE(ok.isObject());
	EXPECT_TRUE(ok["ok"].asBool());
	EXPECT_FALSE(ok.isMember("error"));
	EXPECT_FALSE(ok.isMember("success"));	// the wire carries the raw handler result, not the DiaAPI wrapper
	const Json::Value bad = rig.commands.Raw("cells.select.query", Expr("terrain =="));
	ASSERT_TRUE(bad.isObject());
	EXPECT_FALSE(bad["ok"].asBool());
	EXPECT_FALSE(bad.isMember("count"));
	ASSERT_TRUE(bad["error"].isObject());
	EXPECT_TRUE(bad["error"]["message"].isString());
	EXPECT_TRUE(bad["error"]["column"].isUInt());
	EXPECT_TRUE(bad["error"]["length"].isUInt());
}

TEST_F(QueryCommandsTest, BareStringParamsAreAcceptedForBothCommands)
{
	Rig rig(66, 42);
	const Json::Value c = Exec("cells.query.check", Json::Value("terrain == water"));
	ASSERT_TRUE(c["success"].asBool());
	EXPECT_EQ(c["data"]["count"].asUInt(), rig.WaterCount());
	const Json::Value s = Exec("cells.select.query", Json::Value("terrain == water"));
	ASSERT_TRUE(s["success"].asBool());
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), rig.WaterCount());
}

TEST_F(QueryCommandsTest, NonStringExprIsTreatedAsEmpty)
{
	Rig rig(66, 42);
	for (const char* cmd : { "cells.select.query", "cells.query.check" })
	{
		Json::Value num(Json::objectValue);
		num["expr"] = 5;
		Json::Value arr(Json::objectValue);
		arr["expr"] = Json::Value(Json::arrayValue);
		const Json::Value cases[] = { num, arr, Json::Value(7), Json::Value(Json::nullValue) };
		for (const Json::Value& p : cases)
		{
			const Json::Value r = Exec(cmd, p);
			ASSERT_FALSE(r["success"].asBool()) << cmd;
			EXPECT_EQ(r["error"]["message"].asString(), "empty query");
		}
	}
}

TEST_F(QueryCommandsTest, ErrorColumnsAreRelativeToTheOriginalTextIncludingLeadingSpaces)
{
	Rig rig(66, 42);
	const Json::Value r = Exec("cells.query.check", Expr("   terrian == water"));
	ASSERT_FALSE(r["success"].asBool());
	EXPECT_EQ(r["error"]["column"].asUInt(), 3u);
	EXPECT_EQ(r["error"]["length"].asUInt(), 7u);
	EXPECT_EQ(r["error"]["suggestion"].asString(), "terrain");
	const Json::Value e = Exec("cells.query.check", Expr("terrain == watr"));
	EXPECT_EQ(e["error"]["column"].asUInt(), 11u);
	EXPECT_EQ(e["error"]["suggestion"].asString(), "water");
}

TEST_F(QueryCommandsTest, QueryOver512CharsIsRejectedAtColumn512AndExactly512Runs)
{
	Rig rig(66, 42);
	rig.domain.ActiveSelection()->Set(3, true);
	const uint32_t version = rig.domain.ActiveSelectionView()->Version();
	std::string ok = "temp >= 1";
	ok.append(512 - ok.size(), ' ');
	ASSERT_EQ(ok.size(), 512u);
	const Json::Value good = Exec("cells.query.check", Expr(ok.c_str()));
	ASSERT_TRUE(good["success"].asBool());
	EXPECT_EQ(good["data"]["count"].asUInt(), 66u * 42u);
	for (const char* cmd : { "cells.select.query", "cells.query.check" })
	{
		const Json::Value r = Exec(cmd, Expr((ok + " ").c_str()));
		ASSERT_FALSE(r["success"].asBool()) << cmd;
		EXPECT_EQ(r["error"]["column"].asUInt(), 512u);
		EXPECT_EQ(r["error"]["length"].asUInt(), 1u);
		EXPECT_NE(r["error"]["message"].asString().find("512"), std::string::npos);
	}
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Version(), version);
	EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(3));
}

TEST_F(QueryCommandsTest, NestingDepthSeventeenIsRejectedAndSixteenRuns)
{
	Rig rig(66, 42);
	const auto nested = [](int d) { return std::string(d, '(') + "temp >= 1" + std::string(d, ')'); };
	EXPECT_TRUE(Exec("cells.query.check", Expr(nested(16).c_str()))["success"].asBool());
	rig.domain.ActiveSelection()->Set(2, true);
	const uint32_t version = rig.domain.ActiveSelectionView()->Version();
	const Json::Value r = Exec("cells.select.query", Expr(nested(17).c_str()));
	ASSERT_FALSE(r["success"].asBool());
	EXPECT_NE(r["error"]["message"].asString().find("deep"), std::string::npos);
	EXPECT_EQ(r["error"]["column"].asUInt(), 16u);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Version(), version);
}

TEST_F(QueryCommandsTest, EmptyMatchSetReplacesSelectionWithNothingAndBumpsOnce)
{
	Rig rig(66, 42);
	rig.domain.ActiveSelection()->Set(4, true);
	const uint32_t v0 = rig.domain.ActiveSelectionView()->Version();
	const Json::Value r = Exec("cells.select.query", Expr("temp > 100"));
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_TRUE(r["data"]["ok"].asBool());
	EXPECT_EQ(r["data"]["count"].asUInt(), 0u);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 0u);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Version(), v0 + 1);
	EXPECT_EQ(Exec("cells.query.check", Expr("temp > 100"))["data"]["count"].asUInt(), 0u);
}

TEST_F(QueryCommandsTest, SelectingEverythingMatchesTheCellCount)
{
	Rig rig(66, 42);
	const Json::Value r = Exec("cells.select.query", Expr("temp >= 1 or not temp >= 1"));
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["data"]["count"].asUInt(), 66u * 42u);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 66u * 42u);
}

TEST_F(QueryCommandsTest, FailedQueryDoesNotPoisonTheNextOne)
{
	Rig rig(66, 42);
	EXPECT_FALSE(Exec("cells.select.query", Expr("bogus > 1"))["success"].asBool());
	EXPECT_FALSE(Exec("cells.select.query", Expr("terrain > 1"))["success"].asBool());
	EXPECT_FALSE(Exec("cells.select.query", Expr("((temp > 1"))["success"].asBool());
	const Json::Value r = Exec("cells.select.query", Expr("terrain == water"));
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["data"]["count"].asUInt(), rig.WaterCount());
	// A short query after a long one must not see leftover tree nodes.
	ASSERT_TRUE(Exec("cells.select.query", Expr("temp >= 1 and terrain != grass and terrain != rock"))["success"].asBool());
	const Json::Value shortQ = Exec("cells.select.query", Expr("terrain == sand"));
	EXPECT_EQ(shortQ["data"]["count"].asUInt(), rig.domain.ActiveSelectionView()->Count());
}

TEST_F(QueryCommandsTest, CommandOrderingSelectCheckSelectKeepsTheLastSelectResult)
{
	Rig rig(66, 42);
	const uint32_t v0 = rig.domain.ActiveSelectionView()->Version();
	const Json::Value a = rig.commands.Raw("cells.select.query", Expr("terrain == water"));
	const uint32_t water = a["count"].asUInt();
	const uint32_t v1 = rig.domain.ActiveSelectionView()->Version();
	EXPECT_EQ(v1, v0 + 1);
	const Json::Value chk = rig.commands.Raw("cells.query.check", Expr("terrain == rock"));
	EXPECT_EQ(chk["count"].asUInt(), water);	// 66x42 = 2772 = 693 * 4: the four ids cycle evenly
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Version(), v1);
	EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(1));	// still the water cells
	const Json::Value b = rig.commands.Raw("cells.select.query", Expr("terrain == rock"));
	EXPECT_EQ(b["count"].asUInt(), water);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Version(), v1 + 1);
	EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(2));
	EXPECT_FALSE(rig.domain.ActiveSelectionView()->Contains(1));
}

TEST_F(QueryCommandsTest, UnregisteredCommandsAreGoneAndReRegistrationWorks)
{
	Rig rig(66, 42);
	rig.commands.Unregister();
	const Json::Value gone = Exec("cells.select.query", Expr("temp >= 1"));
	EXPECT_FALSE(gone["success"].asBool());
	EXPECT_EQ(gone["error"].asString(), "command not found");
	EXPECT_EQ(rig.commands.Raw("cells.query.check", Expr("temp >= 1")).type(), Json::nullValue);
	{
		ViewerCommands again(rig.domain);
		ASSERT_TRUE(again.Registered());
		EXPECT_TRUE(again.Registrar().IsCommand("cells.select.query"));
		EXPECT_TRUE(again.Registrar().IsCommand("cells.query.check"));
		EXPECT_EQ(Exec("cells.query.check", Expr("temp >= 1"))["data"]["count"].asUInt(), 66u * 42u);
	}
}

TEST_F(QueryCommandsTest, RejectedSelectLogsCommandNameAndMessageAtInfoOnce)
{
	Rig rig(66, 42);
	LogCapture log;
	Exec("cells.select.query", Expr("terrian == water"));
	EXPECT_EQ(log.Count(Level::kInfo, "cells.select.query rejected: unknown layer 'terrian'"), 1u);
	EXPECT_EQ(log.Count(Level::kInfo), 1u);
	log.Clear();
	Exec("cells.select.query", Expr(""));
	EXPECT_EQ(log.Count(Level::kInfo, "rejected: empty query"), 1u);
}

TEST_F(QueryCommandsTest, QueryUsesScrubbedValuesFromTheInstalledValueSource)
{
	Rig rig(66, 42);
	StubViewValueSource source;
	source.live = false;
	source.values = std::vector<float>(66u * 42u, 0.0f);	// the scrubbed sample: temp is 0 everywhere
	rig.registry.SetViewValueSource(&source);
	EXPECT_EQ(Exec("cells.query.check", Expr("temp >= 1"))["data"]["count"].asUInt(), 0u);
	EXPECT_EQ(Exec("cells.select.query", Expr("temp == 0"))["data"]["count"].asUInt(), 66u * 42u);
	source.live = true;
	EXPECT_EQ(Exec("cells.query.check", Expr("temp >= 1"))["data"]["count"].asUInt(), 66u * 42u);
	source.live = false;
	source.hasData = false;
	source.epoch = 2;
	const Json::Value nodata = Exec("cells.query.check", Expr("temp >= 1"));
	ASSERT_FALSE(nodata["success"].asBool());
	EXPECT_EQ(nodata["error"]["message"].asString(), "no data for layer");
	EXPECT_EQ(nodata["error"]["column"].asUInt(), 0u);
	rig.registry.SetViewValueSource(nullptr);
}

TEST_F(QueryCommandsTest, QueriesRunAgainstTheActiveGridOnly)
{
	TwoGridRig rig;
	ASSERT_EQ(rig.domain.ActiveGrid(), &rig.gridA.desc);
	const Json::Value wrong = Exec("cells.query.check", Expr("bx > 1"));
	ASSERT_FALSE(wrong["success"].asBool());
	EXPECT_NE(wrong["error"]["message"].asString().find("unknown layer 'bx'"), std::string::npos);
	ASSERT_TRUE(Exec("cells.select.query", Expr("temp >= 1"))["success"].asBool());
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 16u);

	ASSERT_TRUE(rig.domain.SetBase(StringCRC("bx")));
	ASSERT_EQ(rig.domain.ActiveGrid(), &rig.gridB.desc);
	EXPECT_FALSE(Exec("cells.query.check", Expr("temp >= 1"))["success"].asBool());
	const Json::Value r = Exec("cells.select.query", Expr("bx > 1"));
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["data"]["count"].asUInt(), 3u);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 3u);
	// changing the active grid clears the selection of the grid left and entered (documented viewer behaviour)
	ASSERT_TRUE(rig.domain.SetBase(StringCRC("temp")));
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 0u);
	EXPECT_EQ(Exec("cells.select.query", Expr("temp >= 1"))["data"]["count"].asUInt(), 16u);
}

TEST_F(QueryCommandsTest, CardJsonRecordsCheckAndLastIndependently)
{
	Rig rig(66, 42);
	Json::Value out(Json::objectValue);
	rig.domain.GetJSONState(out);
	ASSERT_TRUE(out["query"]["available"].asBool());
	EXPECT_FALSE(out["query"].isMember("check"));
	EXPECT_FALSE(out["query"].isMember("last"));

	Exec("cells.select.query", Expr("terrian == water"));
	Exec("cells.query.check", Expr("terrain == water"));
	out = Json::Value(Json::objectValue);
	rig.domain.GetJSONState(out);
	const Json::Value& q = out["query"];
	EXPECT_FALSE(q["last"]["ok"].asBool());
	EXPECT_EQ(q["last"]["expr"].asString(), "terrian == water");
	EXPECT_EQ(q["last"]["error"]["suggestion"].asString(), "terrain");
	EXPECT_EQ(q["last"]["error"]["column"].asUInt(), 0u);
	EXPECT_EQ(q["last"]["error"]["length"].asUInt(), 7u);
	EXPECT_TRUE(q["check"]["ok"].asBool());
	EXPECT_EQ(q["check"]["count"].asUInt(), rig.WaterCount());
	EXPECT_EQ(q["check"]["expr"].asString(), "terrain == water");
	EXPECT_FALSE(q["check"].isMember("error"));
}

TEST_F(ReentryTest, ReadDelegateReadingTheDomainDuringAQueryRunDoesNotHang)
{
	Rig rig(66, 42);
	AddReentrantLayer(rig);
	gArmed = true;
	Json::Value r;
	ASSERT_TRUE(FinishesWithin(std::chrono::milliseconds(5000), [&] { r = Exec("cells.select.query", Expr("rd > 0.5")); }))
		<< "select hung: a read delegate re-entered the domain";
	EXPECT_EQ(gReentryCalls, 1u);
	EXPECT_EQ(gNestedGridCount, 1u);
	EXPECT_FALSE(gNestedActiveBase.empty());
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["data"]["count"].asUInt(), 66u * 42u);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 66u * 42u);
}

TEST_F(ReentryTest, ReadDelegateRunningANestedQueryDoesNotCorruptTheOuterQuery)
{
	Rig rig(66, 42);
	AddReentrantLayer(rig);
	gArmed = true;
	gNestedCmd = "cells.query.check";
	gNestedExpr = "temp < 0.5";	// would flip the outer comparison if it shared the parser/resolver state
	Json::Value r;
	ASSERT_TRUE(FinishesWithin(std::chrono::milliseconds(5000), [&] { r = Exec("cells.select.query", Expr("rd > 0.5")); }))
		<< "select hung: a nested query ran inside a read delegate";
	EXPECT_EQ(gReentryCalls, 1u);
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_EQ(r["data"]["count"].asUInt(), 66u * 42u);	// rd is 1.0 everywhere: "rd > 0.5" matches all
	EXPECT_TRUE(gNestedQueryRejected);
	EXPECT_EQ(gNestedQueryMessage, "query already running");
}

TEST_F(ReentryTest, ReadDelegateRunningANestedSelectLeavesTheOuterSelectResultIntact)
{
	Rig rig(66, 42);
	AddReentrantLayer(rig);
	gArmed = true;
	gNestedCmd = "cells.select.query";
	gNestedExpr = "terrain == water";
	Json::Value r;
	ASSERT_TRUE(FinishesWithin(std::chrono::milliseconds(5000), [&] { r = Exec("cells.select.query", Expr("rd > 0.5")); }));
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 66u * 42u);
	EXPECT_TRUE(gNestedQueryRejected);
}

TEST_F(ReentryTest, PanelQueryRunWithReenteringDelegateDoesNotHang)
{
	Rig rig(66, 42);
	AddReentrantLayer(rig);
	gArmed = true;
	gNestedCmd = "cells.query.check";
	gNestedExpr = "temp < 0.5";
	ASSERT_TRUE(FinishesWithin(std::chrono::milliseconds(5000), [&]
	{
		Json::Value a(Json::objectValue);
		a["e"] = "rd > 0.5";
		rig.domain.OnCommand(StringCRC("query_run"), a);
	}));
	Json::Value out(Json::objectValue);
	rig.domain.GetJSONState(out);
	EXPECT_TRUE(out["query"]["last"]["ok"].asBool());
	EXPECT_EQ(out["query"]["last"]["count"].asUInt(), 66u * 42u);
}

TEST_F(ReentryTest, TheDomainStillRunsQueriesAfterARejectedNestedQuery)
{
	Rig rig(66, 42);
	AddReentrantLayer(rig);
	gArmed = true;
	gNestedCmd = "cells.query.check";
	gNestedExpr = "temp < 0.5";
	Exec("cells.query.check", Expr("rd > 0.5"));
	const Json::Value again = Exec("cells.query.check", Expr("terrain == water"));
	ASSERT_TRUE(again["success"].asBool());
	EXPECT_EQ(again["data"]["count"].asUInt(), rig.WaterCount());
}

// ---- Exhaustive pass 2 (Feature 3) ----

TEST_F(ReentryTest, ReadDelegateClearingTheSelectionMidQueryDoesNotDisturbTheOuterResult)
{
	Rig rig(66, 42);
	AddReentrantLayer(rig);
	rig.domain.ActiveSelection()->Set(5, true);
	gArmed = true;
	gNestedCmd = "cells.select.clear";
	gNestedExpr = "";
	Json::Value r;
	ASSERT_TRUE(FinishesWithin(std::chrono::milliseconds(5000), [&] { r = Exec("cells.select.query", Expr("rd > 0.5")); }));
	ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
	EXPECT_EQ(gReentryCalls, 1u);
	EXPECT_FALSE(gNestedQueryRejected);	// a selection command is not blocked by the running query
	EXPECT_EQ(r["data"]["count"].asUInt(), 66u * 42u);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 66u * 42u);	// the outer query replaces whatever the nested call did
}

TEST_F(ReentryTest, ReentrantLayerUsedTwiceInOneQueryReadsOnceAndReleasesTheRunningFlag)
{
	Rig rig(66, 42);
	AddReentrantLayer(rig);
	gArmed = true;
	gNestedCmd = "cells.query.check";
	gNestedExpr = "temp < 0.5";
	const Json::Value r = Exec("cells.select.query", Expr("rd > 0.5 and rd < 2"));
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["data"]["count"].asUInt(), 66u * 42u);
	EXPECT_TRUE(gNestedQueryRejected);
	EXPECT_EQ(gReentryCalls, 1u);	// the layer is read once per query even when two comparisons use it
	const Json::Value after = Exec("cells.query.check", Expr("temp < 0.5"));
	ASSERT_TRUE(after["success"].asBool());
	EXPECT_EQ(after["data"]["count"].asUInt(), 0u);
}

TEST_F(QueryCommandsTest, UppercaseKeywordsAreRejectedAtTheKeywordAndLeaveTheSelectionAlone)
{
	Rig rig(66, 42);
	rig.domain.ActiveSelection()->Set(7, true);
	const uint32_t version = rig.domain.ActiveSelectionView()->Version();
	struct Case { const char* expr; uint32_t column; };
	const Case cases[] = { { "temp >= 1 AND terrain == water", 10 }, { "temp >= 1 Or terrain == water", 10 }, { "NOT temp >= 1", 4 } };
	for (const Case& c : cases)
	{
		for (const char* cmd : { "cells.select.query", "cells.query.check" })
		{
			const Json::Value r = Exec(cmd, Expr(c.expr));
			ASSERT_FALSE(r["success"].asBool()) << cmd << " " << c.expr;
			EXPECT_EQ(r["error"]["column"].asUInt(), c.column) << cmd << " " << c.expr;
		}
	}
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Version(), version);
	EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(7));
}

TEST_F(QueryCommandsTest, SingleCellGridQueriesAndNegation)
{
	Rig rig(1, 1);
	EXPECT_EQ(Exec("cells.select.query", Expr("terrain == grass"))["data"]["count"].asUInt(), 1u);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 1u);
	EXPECT_EQ(Exec("cells.select.query", Expr("not (terrain == grass)"))["data"]["count"].asUInt(), 0u);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 0u);
	EXPECT_EQ(Exec("cells.select.query", Expr("not terrain == water and temp >= 1"))["data"]["count"].asUInt(), 1u);
}

TEST_F(QueryCommandsTest, MillionCellGridEvaluatesWithExactCounts)
{
	Rig rig(1024, 1024);	// 1,048,576 cells, 16,384 words per pool slot
	const uint32_t total = 1024u * 1024u;
	EXPECT_EQ(Exec("cells.query.check", Expr("temp >= 1"))["data"]["count"].asUInt(), total);
	EXPECT_EQ(Exec("cells.query.check", Expr("not temp >= 1"))["data"]["count"].asUInt(), 0u);
	EXPECT_EQ(Exec("cells.query.check", Expr("terrain == rock or terrain == sand"))["data"]["count"].asUInt(), total / 2);
	const Json::Value r = Exec("cells.select.query", Expr("terrain != grass and (terrain == water or temp > 5)"));
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["data"]["count"].asUInt(), total / 4);
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), total / 4);
	EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(1));
	EXPECT_FALSE(rig.domain.ActiveSelectionView()->Contains(2));
	EXPECT_TRUE(rig.domain.ActiveSelectionView()->Contains(total - 3));	// (total-3) % 4 == 1
}

TEST_F(QueryCommandsTest, SimHostedQueryRepliesAreOrderedAndVisibleToTheNextRequest)
{
	Rig rig(66, 42);
	MirrorQueries queries(rig.registry, rig.domain, rig.service);
	FakeMirrorRegistrar& reg = rig.commands.Registrar();
	ASSERT_TRUE(RegisterMirrorQueries(reg, queries));
	ASSERT_TRUE(reg.IsCommand("cells.select.query"));
	ASSERT_TRUE(reg.IsCommand("cells.query.check"));

	EXPECT_EQ(reg.Execute("cells.select.get", Json::Value(Json::objectValue))["total"].asUInt(), 0u);
	const Json::Value chk = reg.Execute("cells.query.check", Expr("terrain == water"));
	EXPECT_TRUE(chk["ok"].asBool());
	EXPECT_EQ(chk["count"].asUInt(), rig.WaterCount());
	EXPECT_EQ(reg.Execute("cells.select.get", Json::Value(Json::objectValue))["total"].asUInt(), 0u);	// check did not select

	const Json::Value sel = reg.Execute("cells.select.query", Expr("terrain == water"));
	EXPECT_TRUE(sel["ok"].asBool());
	EXPECT_FALSE(sel.isMember("success"));	// raw reply, not the DiaAPI wrapper
	EXPECT_EQ(reg.Execute("cells.select.get", Json::Value(Json::objectValue))["total"].asUInt(), rig.WaterCount());

	const Json::Value bad = reg.Execute("cells.select.query", Expr("terrian == water"));
	EXPECT_FALSE(bad["ok"].asBool());
	EXPECT_EQ(bad["error"]["suggestion"].asString(), "terrain");
	EXPECT_EQ(reg.Execute("cells.select.get", Json::Value(Json::objectValue))["total"].asUInt(), rig.WaterCount());	// rejected query changed nothing

	const Json::Value none = reg.Execute("cells.select.query", Expr("temp > 100"));
	EXPECT_EQ(none["count"].asUInt(), 0u);
	EXPECT_EQ(reg.Execute("cells.select.get", Json::Value(Json::objectValue))["total"].asUInt(), 0u);
	UnregisterMirrorQueries(reg);
}

TEST_F(QueryCommandsTest, UnregisteredViewerAnswersQueryAsNotRegisteredNotACrash)
{
	Rig rig(10, 10);
	rig.commands.Unregister();
	EXPECT_TRUE(rig.commands.Raw("cells.select.query", Expr("temp >= 1")).isNull());
	EXPECT_TRUE(rig.commands.Raw("cells.query.check", Expr("temp >= 1")).isNull());
	EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), 0u);
}

#endif // DIA_DEBUG
