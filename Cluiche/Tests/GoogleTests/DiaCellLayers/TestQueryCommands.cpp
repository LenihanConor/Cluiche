#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/Testing/ViewerCommands.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaAPI/DiaAPI.h>

#include <sstream>
#include <string>
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

#endif // DIA_DEBUG
