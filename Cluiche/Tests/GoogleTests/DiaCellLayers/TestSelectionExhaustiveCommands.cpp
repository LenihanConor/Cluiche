#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/SelectionTools.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/CellPickerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaAPI/DiaAPI.h>
#include <DiaPathfinding/HexTopology.h>

#include <atomic>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Task 10a exhaustive pass for Selection (Feature 2): registry selection store logging, cells.select.* JSON edge cases,
// active-grid clearing, tool changes and concurrency on the viewer domain.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexTopology;
typedef LogCapture::Level Level;

// ---------------------------------------------------------------- registry selection store (not DIA_DEBUG-only)

TEST(SelectionExhaustiveRegistry, GridSizeChangeClearsAndLogsInfoOnceWithDroppedCount)
{
	FakePuId() = 1;
	TestGrid grid(4, 3);
	StubProvider p;
	p.values.assign(12, 0.0f);
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	ASSERT_EQ(CellRegisterResult::Ok, registry.Register(p.MakeDesc("a", &grid.desc)));
	CellSelection* sel = registry.SelectionFor(&grid.desc);
	sel->Set(1, true);
	sel->Set(2, true);
	LogCapture log;
	log.Clear();
	grid.desc.height = 5;
	registry.SelectionFor(&grid.desc);
	registry.SelectionFor(&grid.desc);		// second access: nothing left to drop, no second log
	EXPECT_EQ(1u, log.Count(Level::kInfo, "Selection cleared"));
	EXPECT_EQ(1u, log.Count(Level::kInfo, "2 cells dropped"));
	EXPECT_EQ(0u, sel->Count());
	EXPECT_EQ(5u, sel->Height());
}

TEST(SelectionExhaustiveRegistry, EmptySelectionResizedLogsNothing)
{
	FakePuId() = 1;
	TestGrid grid(4, 3);
	StubProvider p;
	p.values.assign(12, 0.0f);
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	registry.Register(p.MakeDesc("a", &grid.desc));
	registry.SelectionFor(&grid.desc);
	LogCapture log;
	log.Clear();
	grid.desc.width = 9;
	registry.SelectionFor(&grid.desc);
	EXPECT_EQ(0u, log.Count(Level::kInfo, "Selection cleared"));
}

TEST(SelectionExhaustiveRegistry, StoreFullWarnsExactlyOnceAcrossRepeatedRequests)
{
	FakePuId() = 1;
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	std::vector<std::unique_ptr<TestGrid>> grids;
	for (uint32_t i = 0; i < CellLayerRegistry::kMaxSelectionGrids; ++i)
	{
		grids.emplace_back(new TestGrid(2, 2));
		ASSERT_NE(nullptr, registry.SelectionFor(&grids.back()->desc));
	}
	LogCapture log;
	log.Clear();
	TestGrid ninth(2, 2), tenth(2, 2);
	EXPECT_EQ(nullptr, registry.SelectionFor(&ninth.desc));
	EXPECT_EQ(nullptr, registry.SelectionFor(&ninth.desc));
	EXPECT_EQ(nullptr, registry.SelectionFor(&tenth.desc));
	EXPECT_EQ(1u, log.Warnings("Selection store full"));
}

#ifdef DIA_DEBUG

namespace
{
	struct Dim { uint16_t w; uint16_t h; };
	const Dim kDims[] = { { 66, 42 }, { 104, 64 } };

	// terrain (category: grass water rock sand unused, value = index % 4), temp (float), second grid "soil" (category).
	struct Rig
	{
		TestGrid grid;
		TestGrid gridB;
		CellFieldRegistry cellField;
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		StubProvider terrain, temp, soil;
		FakeDebugDraw draw;
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;

		static const Dia::ValueSchema::ValueSchemaService* Build(UploaderSchemaFixture& f)
		{
			for (const char* key : { "terrain", "soil" })
			{
				Dia::ValueSchema::ValueInfo t = Dia::ValueSchema::Testing::MakeValueInfo("cells", key);
				t.mType = Dia::ValueSchema::ValueType::Enum;
				t.mEnumValues.Reserve(5);
				for (const char* id : { "grass", "water", "rock", "sand", "unused" })
				{
					Dia::ValueSchema::EnumValue v;
					Dia::ValueSchema::SetText(v.mId, id);
					t.mEnumValues.Add(v);
				}
				f.Add(t);
			}
			f.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
			return &f.Publish();
		}

		Rig(uint16_t w, uint16_t h)
			: grid(w, h)
			, gridB(5, 4)
			, service(Build(fx))
			, registry(nullptr, &FakePuIdFn)
			, uploader(cellField, nullptr)
			, domain(registry, uploader, service)
		{
			FakePuId() = 1;
			terrain.values.resize(static_cast<size_t>(w) * h);
			for (size_t i = 0; i < terrain.values.size(); ++i) { terrain.values[i] = static_cast<float>(i % 4); }
			temp.values = std::vector<float>(static_cast<size_t>(w) * h, 1.0f);
			soil.values = std::vector<float>(20, 1.0f);
			registry.Register(terrain.MakeDesc("terrain", &grid.desc, CellValueKind::Category, "cells/terrain"));
			registry.Register(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
			registry.Register(soil.MakeDesc("soil", &gridB.desc, CellValueKind::Category, "cells/soil"));
			domain.RegisterCommands();
			domain.SetBase(StringCRC("terrain"));
			domain.Update(draw);
		}

		uint32_t Idx(int q, int r) const { return static_cast<uint32_t>(r) * grid.desc.width + static_cast<uint32_t>(q); }
		CellSelection* Sel() { return domain.ActiveSelection(); }
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
	Json::Value Exec(const char* cmd, const Json::Value& params) { return Dia::API::ExecuteCommandJson(StringCRC(cmd), params); }

	struct SelectionExhaustiveCommandsTest : ::testing::Test
	{
		void SetUp() override { FakePuId() = 1; Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};

	Json::Value CellsParams(uint32_t n, uint32_t perRow = 100)
	{
		Json::Value cells(Json::arrayValue);
		for (uint32_t i = 0; i < n; ++i)
		{
			Json::Value p(Json::arrayValue);
			p.append(int(i % perRow));
			p.append(int(i / perRow));
			cells.append(p);
		}
		Json::Value params(Json::objectValue);
		params["cells"] = cells;
		return params;
	}
}

// ---- cells.select.cells

TEST_F(SelectionExhaustiveCommandsTest, CellsExactlyAtCapIsNotOverCap)
{
	Rig rig(104, 64);
	LogCapture log;
	log.Clear();
	const Json::Value r = Exec("cells.select.cells", CellsParams(2048));
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_FALSE(r["data"].isMember("overCap"));
	EXPECT_EQ(2048u, r["data"]["requested"].asUInt());
	EXPECT_EQ(2048u, r["data"]["accepted"].size());
	EXPECT_EQ(2048u, r["data"]["count"].asUInt());
	EXPECT_EQ(0u, log.Warnings());
}

TEST_F(SelectionExhaustiveCommandsTest, CellsOneOverCapIsOverCapAndIgnoresTheExtra)
{
	Rig rig(104, 64);
	LogCapture log;
	log.Clear();
	const Json::Value r = Exec("cells.select.cells", CellsParams(2049));
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_TRUE(r["data"]["overCap"].asBool());
	EXPECT_EQ(2049u, r["data"]["requested"].asUInt());
	EXPECT_EQ(2048u, r["data"]["accepted"].size());
	EXPECT_EQ(2048u, rig.Sel()->Count());
	EXPECT_FALSE(rig.Sel()->Contains(rig.Idx(2048 % 100, 2048 / 100)));
	EXPECT_EQ(1u, log.Warnings());
}

TEST_F(SelectionExhaustiveCommandsTest, CellsMalformedEntryBeyondTheCapStillRejectsTheWholeCall)
{
	Rig rig(104, 64);
	Rig& r0 = rig;
	r0.Sel()->Set(3, true);
	const uint32_t v = r0.Sel()->Version();
	Json::Value params = CellsParams(2100);
	params["cells"][2090] = Parse("[\"x\", 1]");
	const Json::Value r = Exec("cells.select.cells", params);
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_NE(std::string::npos, r["error"].asString().find("cell 2090"));
	EXPECT_EQ(v, r0.Sel()->Version());
	EXPECT_EQ(1u, r0.Sel()->Count());
}

TEST_F(SelectionExhaustiveCommandsTest, CellsEmptyArrayClearsAndReportsZero)
{
	Rig rig(66, 42);
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":2,\"r1\":2}");
	const Json::Value r = Exec("cells.select.cells", "{\"cells\":[]}");
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(0u, r["data"]["requested"].asUInt());
	EXPECT_EQ(0u, r["data"]["accepted"].size());
	EXPECT_EQ(0u, r["data"]["count"].asUInt());
	EXPECT_EQ(0u, rig.Sel()->Count());
}

TEST_F(SelectionExhaustiveCommandsTest, CellsAllOutOfBoundsClearsPriorSelectionAndAcceptsNone)
{
	Rig rig(66, 42);
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	const Json::Value r = Exec("cells.select.cells", "{\"cells\":[[66,0],[0,42],[-1,-1],[2147483647,2147483647],[-2147483648,0]]}");
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(5u, r["data"]["requested"].asUInt());
	EXPECT_EQ(0u, r["data"]["accepted"].size());
	EXPECT_EQ(0u, rig.Sel()->Count());
}

TEST_F(SelectionExhaustiveCommandsTest, CellsRepeatedIdenticalCallDoesNotBumpVersion)
{
	Rig rig(66, 42);
	Exec("cells.select.cells", "{\"cells\":[[1,1],[2,2]]}");
	const uint32_t v = rig.Sel()->Version();
	const Json::Value r = Exec("cells.select.cells", "{\"cells\":[[2,2],[1,1]]}");
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(2u, r["data"]["count"].asUInt());
	EXPECT_EQ(v, rig.Sel()->Version());
}

TEST_F(SelectionExhaustiveCommandsTest, CellsRejectsNonArrayElementsAndWrongSizedPairs)
{
	Rig rig(66, 42);
	const char* bad[] = {
		"{\"cells\":[5]}", "{\"cells\":[null]}", "{\"cells\":[[]]}", "{\"cells\":[[1,2,3]]}", "{\"cells\":[{\"q\":1,\"r\":2}]}",
		"{\"cells\":[[true,false]]}", "{\"cells\":[[4294967296,1]]}", "{\"cells\":null}", "{\"cells\":{}}",
	};
	for (const char* params : bad)
	{
		const Json::Value r = Exec("cells.select.cells", params);
		EXPECT_FALSE(r["success"].asBool()) << params;
	}
	EXPECT_EQ(0u, rig.Sel()->Version());
}

// ---- cell / toggle / box

TEST_F(SelectionExhaustiveCommandsTest, CoordinatesOutsideInt32AreRejectedWhileInt32ExtremesAreIgnored)
{
	Rig rig(66, 42);
	Exec("cells.select.cell", "{\"q\":2,\"r\":2}");
	const uint32_t v = rig.Sel()->Version();
	EXPECT_FALSE(Exec("cells.select.cell", "{\"q\":2147483648,\"r\":0}")["success"].asBool());
	EXPECT_FALSE(Exec("cells.select.toggle", "{\"q\":0,\"r\":-2147483649}")["success"].asBool());
	EXPECT_FALSE(Exec("cells.select.cell", "{\"q\":1e30,\"r\":0}")["success"].asBool());
	for (const char* cmd : { "cells.select.cell", "cells.select.toggle" })
	{
		const Json::Value hi = Exec(cmd, "{\"q\":2147483647,\"r\":2147483647}");
		const Json::Value lo = Exec(cmd, "{\"q\":-2147483648,\"r\":-2147483648}");
		ASSERT_TRUE(hi["success"].asBool()) << cmd;
		ASSERT_TRUE(lo["success"].asBool()) << cmd;
		EXPECT_EQ(1u, hi["data"]["count"].asUInt());
	}
	EXPECT_EQ(v, rig.Sel()->Version());
}

TEST_F(SelectionExhaustiveCommandsTest, CellAndToggleAtGridCornersOnBothSizes)
{
	for (const Dim& d : kDims)
	{
		Rig rig(d.w, d.h);
		char buf[96];
		snprintf(buf, sizeof(buf), "{\"q\":%d,\"r\":%d}", d.w - 1, d.h - 1);
		ASSERT_TRUE(Exec("cells.select.cell", buf)["success"].asBool());
		EXPECT_TRUE(rig.Sel()->Contains(rig.Idx(d.w - 1, d.h - 1)));
		snprintf(buf, sizeof(buf), "{\"q\":%d,\"r\":0}", d.w);
		const Json::Value r = Exec("cells.select.toggle", buf);
		ASSERT_TRUE(r["success"].asBool());
		EXPECT_EQ(1u, r["data"]["count"].asUInt());
		const Json::Value z = Exec("cells.select.toggle", "{\"q\":0,\"r\":0}");
		EXPECT_EQ(2u, z["data"]["count"].asUInt());
	}
}

TEST_F(SelectionExhaustiveCommandsTest, ExtraAndNullParamsAreHandledWithoutCrashing)
{
	Rig rig(66, 42);
	EXPECT_TRUE(Exec("cells.select.cell", "{\"q\":1,\"r\":1,\"extra\":[1,2,3]}")["success"].asBool());
	EXPECT_TRUE(Exec("cells.select.clear", Json::Value())["success"].asBool());
	EXPECT_TRUE(Exec("cells.select.clear", "[1,2]")["success"].asBool());
	for (const char* cmd : { "cells.select.cell", "cells.select.toggle", "cells.select.box", "cells.select.poly", "cells.select.category", "cells.select.cells" })
	{
		EXPECT_FALSE(Exec(cmd, Json::Value())["success"].asBool()) << cmd;
		EXPECT_FALSE(Exec(cmd, "[]")["success"].asBool()) << cmd;
		EXPECT_FALSE(Exec(cmd, "\"text\"")["success"].asBool()) << cmd;
		EXPECT_FALSE(Exec(cmd, "42")["success"].asBool()) << cmd;
	}
}

TEST_F(SelectionExhaustiveCommandsTest, BoxExtremesCoverTheWholeGridAndRepeatIsIdempotent)
{
	for (const Dim& d : kDims)
	{
		Rig rig(d.w, d.h);
		const char* params = "{\"q0\":-2147483648,\"r0\":-2147483648,\"q1\":2147483647,\"r1\":2147483647}";
		const Json::Value r = Exec("cells.select.box", params);
		ASSERT_TRUE(r["success"].asBool());
		EXPECT_EQ(uint32_t(d.w) * d.h, r["data"]["count"].asUInt());
		const uint32_t v = rig.Sel()->Version();
		Exec("cells.select.box", params);
		EXPECT_EQ(v, rig.Sel()->Version());
	}
}

TEST_F(SelectionExhaustiveCommandsTest, BoxCompletelyOutsideKeepsThePriorSelection)
{
	Rig rig(66, 42);
	Exec("cells.select.cell", "{\"q\":5,\"r\":5}");
	const uint32_t v = rig.Sel()->Version();
	const Json::Value r = Exec("cells.select.box", "{\"q0\":70,\"r0\":0,\"q1\":80,\"r1\":10}");
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(1u, r["data"]["count"].asUInt());
	EXPECT_EQ(v, rig.Sel()->Version());
}

// ---- cells.select.poly

TEST_F(SelectionExhaustiveCommandsTest, PolyAroundOneCellCentreSelectsThatCell)
{
	Rig rig(66, 42);
	const auto c = HexTopology::CellCenter(CellCoord{ 6, 7 }, 1.0f);
	Json::Value pts(Json::arrayValue);
	const float xs[3] = { c.X() - 0.3f, c.X() + 0.4f, c.X() };
	const float ys[3] = { c.Y() - 0.3f, c.Y() - 0.3f, c.Y() + 0.4f };
	for (int i = 0; i < 3; ++i)
	{
		Json::Value p(Json::arrayValue);
		p.append(double(xs[i]));
		p.append(double(ys[i]));
		pts.append(p);
	}
	Json::Value params(Json::objectValue);
	params["points"] = pts;
	const Json::Value r = Exec("cells.select.poly", params);
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(1u, r["data"]["count"].asUInt());
	EXPECT_TRUE(rig.Sel()->Contains(rig.Idx(6, 7)));
}

TEST_F(SelectionExhaustiveCommandsTest, PolyDegenerateIsOkAndReplacesWithNothing)
{
	Rig rig(66, 42);
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	const Json::Value r = Exec("cells.select.poly", "{\"points\":[[0,0],[5,5],[10,10]]}");
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(0u, r["data"]["count"].asUInt());
	EXPECT_EQ(0u, rig.Sel()->Count());
}

TEST_F(SelectionExhaustiveCommandsTest, PolyCoordinatesBeyondFloatRangeAreRejectedAndKeepTheSelection)
{
	Rig rig(66, 42);
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	const uint32_t v = rig.Sel()->Version();
	LogCapture log;
	log.Clear();
	const Json::Value r = Exec("cells.select.poly", "{\"points\":[[0,0],[1e300,0],[0,1e300]]}");
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_EQ(v, rig.Sel()->Version());
	EXPECT_EQ(1u, rig.Sel()->Count());
	EXPECT_EQ(1u, log.Count(Level::kInfo));
}

TEST_F(SelectionExhaustiveCommandsTest, PolyPointWithExtraComponentsOrNullIsRejected)
{
	Rig rig(66, 42);
	EXPECT_FALSE(Exec("cells.select.poly", "{\"points\":[[0,0],[1,0],[0,1,2]]}")["success"].asBool());
	EXPECT_FALSE(Exec("cells.select.poly", "{\"points\":[[0,0],[1,0],null]}")["success"].asBool());
	EXPECT_FALSE(Exec("cells.select.poly", "{\"points\":[[0,0],[1,0],[0,null]]}")["success"].asBool());
	EXPECT_FALSE(Exec("cells.select.poly", "{\"points\":[]}")["success"].asBool());
	EXPECT_EQ(0u, rig.Sel()->Version());
}

// ---- cells.select.category

TEST_F(SelectionExhaustiveCommandsTest, CategoryNumericFormsMatchTheEnumIdSet)
{
	for (const Dim& d : kDims)
	{
		Rig rig(d.w, d.h);
		Exec("cells.select.category", "{\"layer\":\"terrain\",\"value\":\"rock\"}");
		const uint32_t expected = rig.Sel()->Count();
		ASSERT_GT(expected, 0u);
		for (const char* params : { "{\"layer\":\"terrain\",\"value\":2}", "{\"layer\":\"terrain\",\"value\":2.0}", "{\"layer\":\"terrain\",\"value\":\"2\"}" })
		{
			const uint32_t v = rig.Sel()->Version();
			const Json::Value r = Exec("cells.select.category", params);
			ASSERT_TRUE(r["success"].asBool()) << params;
			EXPECT_EQ(expected, r["data"]["count"].asUInt()) << params;
			EXPECT_EQ(v, rig.Sel()->Version()) << params;	// same set: no bump
		}
	}
}

TEST_F(SelectionExhaustiveCommandsTest, CategoryValueHeldByNoCellReplacesWithEmpty)
{
	Rig rig(66, 42);
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	for (const char* params : { "{\"layer\":\"terrain\",\"value\":\"unused\"}", "{\"layer\":\"terrain\",\"value\":99}", "{\"layer\":\"terrain\",\"value\":1.5}", "{\"layer\":\"terrain\",\"value\":-3}", "{\"layer\":\"terrain\",\"value\":1e20}" })
	{
		Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
		const Json::Value r = Exec("cells.select.category", params);
		ASSERT_TRUE(r["success"].asBool()) << params;
		EXPECT_EQ(0u, r["data"]["count"].asUInt()) << params;
	}
}

TEST_F(SelectionExhaustiveCommandsTest, CategoryBadParamTypesAreRejected)
{
	Rig rig(66, 42);
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	const uint32_t v = rig.Sel()->Version();
	const char* bad[] = {
		"{\"layer\":5,\"value\":1}", "{\"layer\":\"\",\"value\":1}", "{\"layer\":null,\"value\":1}", "{\"layer\":\"terrain\",\"value\":true}",
		"{\"layer\":\"terrain\",\"value\":null}", "{\"layer\":\"terrain\",\"value\":[1]}", "{\"layer\":\"terrain\",\"value\":{}}",
		"{\"layer\":\"terrain\",\"value\":\"\"}", "{\"layer\":\"terrain\",\"value\":\"nan\"}",
	};
	for (const char* params : bad)
	{
		EXPECT_FALSE(Exec("cells.select.category", params)["success"].asBool()) << params;
	}
	EXPECT_EQ(v, rig.Sel()->Version());
	EXPECT_EQ(1u, rig.Sel()->Count());
}

TEST_F(SelectionExhaustiveCommandsTest, CategoryOnALayerOfAnotherGridIsAMismatchError)
{
	Rig rig(66, 42);
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	const Json::Value r = Exec("cells.select.category", "{\"layer\":\"soil\",\"value\":1}");
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_NE(std::string::npos, r["error"].asString().find("not on the active grid"));
	EXPECT_EQ(1u, rig.Sel()->Count());
}

// ---- versions, idempotence

TEST_F(SelectionExhaustiveCommandsTest, EveryToolIsIdempotentAndVersionBumpsOnlyOnRealChange)
{
	Rig rig(66, 42);
	struct Step { const char* cmd; const char* params; };
	const Step steps[] = {
		{ "cells.select.cell", "{\"q\":3,\"r\":3}" },
		{ "cells.select.box", "{\"q0\":1,\"r0\":1,\"q1\":4,\"r1\":4}" },
		{ "cells.select.cells", "{\"cells\":[[1,1],[2,2]]}" },
		{ "cells.select.category", "{\"layer\":\"terrain\",\"value\":\"water\"}" },
		{ "cells.select.poly", "{\"points\":[[-1000,-1000],[1000,-1000],[1000,1000]]}" },
	};
	for (const Step& s : steps)
	{
		const uint32_t v0 = rig.Sel()->Version();
		ASSERT_TRUE(Exec(s.cmd, s.params)["success"].asBool()) << s.cmd;
		const uint32_t v1 = rig.Sel()->Version();
		EXPECT_EQ(v0 + 1, v1) << s.cmd;
		ASSERT_TRUE(Exec(s.cmd, s.params)["success"].asBool()) << s.cmd;
		EXPECT_EQ(v1, rig.Sel()->Version()) << s.cmd;
	}
	const uint32_t v = rig.Sel()->Version();
	EXPECT_TRUE(Exec("cells.select.clear", "{}")["success"].asBool());
	EXPECT_EQ(v + 1, rig.Sel()->Version());
	EXPECT_TRUE(Exec("cells.select.clear", "{}")["success"].asBool());
	EXPECT_EQ(v + 1, rig.Sel()->Version());
}

TEST_F(SelectionExhaustiveCommandsTest, ToggleTwiceRestoresTheSetButBumpsTwice)
{
	Rig rig(66, 42);
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	const uint32_t v = rig.Sel()->Version();
	Exec("cells.select.toggle", "{\"q\":9,\"r\":9}");
	Exec("cells.select.toggle", "{\"q\":9,\"r\":9}");
	EXPECT_EQ(v + 2, rig.Sel()->Version());
	EXPECT_EQ(1u, rig.Sel()->Count());
}

TEST_F(SelectionExhaustiveCommandsTest, ResultCountMatchesTheViewForEveryTool)
{
	Rig rig(104, 64);
	const char* const calls[][2] = {
		{ "cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":9,\"r1\":9}" },
		{ "cells.select.toggle", "{\"q\":50,\"r\":50}" },
		{ "cells.select.category", "{\"layer\":\"terrain\",\"value\":3}" },
		{ "cells.select.cells", "{\"cells\":[[1,1],[1,1],[2,2]]}" },
		{ "cells.select.clear", "{}" },
	};
	for (const auto& c : calls)
	{
		const Json::Value r = Exec(c[0], c[1]);
		ASSERT_TRUE(r["success"].asBool()) << c[0];
		EXPECT_EQ(rig.domain.ActiveSelectionView()->Count(), r["data"]["count"].asUInt()) << c[0];
	}
}

// ---- active grid change

TEST_F(SelectionExhaustiveCommandsTest, SwitchingBaseToAnotherGridClearsBothSelectionsAndLogsOnce)
{
	Rig rig(66, 42);
	Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":2,\"r1\":2}");
	// pre-existing selection on the grid about to be entered
	CellSelection* b = rig.registry.SelectionFor(&rig.gridB.desc);
	b->Set(3, true);
	LogCapture log;
	log.Clear();
	ASSERT_TRUE(Exec("cells.layer.base_by_id", "{\"id\":\"soil\"}")["success"].asBool());
	EXPECT_EQ(&rig.gridB.desc, rig.domain.ActiveGrid());
	EXPECT_EQ(0u, b->Count());
	EXPECT_EQ(0u, rig.registry.SelectionFor(&rig.grid.desc)->Count());
	EXPECT_EQ(1u, log.Count(Level::kInfo, "active grid changed (10 cells dropped)"));
	// switching to the same grid again does not clear or log
	b->Set(1, true);
	log.Clear();
	Exec("cells.layer.base_by_id", "{\"id\":\"soil\"}");
	EXPECT_EQ(1u, b->Count());
	EXPECT_EQ(0u, log.Count(Level::kInfo, "active grid changed"));
	// switching between two layers of the same grid keeps the selection
	Exec("cells.layer.base_by_id", "{\"id\":\"terrain\"}");
	EXPECT_EQ(0u, rig.Sel()->Count());
	Exec("cells.select.cell", "{\"q\":1,\"r\":1}");
	Exec("cells.layer.base_by_id", "{\"id\":\"temp\"}");
	EXPECT_EQ(1u, rig.Sel()->Count());
}

TEST_F(SelectionExhaustiveCommandsTest, SwitchingGridWithNothingSelectedDoesNotLogOrBump)
{
	Rig rig(66, 42);
	LogCapture log;
	log.Clear();
	const uint32_t v = rig.Sel()->Version();
	Exec("cells.layer.base_by_id", "{\"id\":\"soil\"}");
	EXPECT_EQ(0u, log.Count(Level::kInfo, "active grid changed"));
	EXPECT_EQ(v, rig.registry.SelectionFor(&rig.grid.desc)->Version());
}

TEST_F(SelectionExhaustiveCommandsTest, CommandsActOnTheNewActiveGridAfterASwitch)
{
	Rig rig(66, 42);
	Exec("cells.layer.base_by_id", "{\"id\":\"soil\"}");
	const Json::Value r = Exec("cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":100,\"r1\":100}");
	ASSERT_TRUE(r["success"].asBool());
	EXPECT_EQ(20u, r["data"]["count"].asUInt());		// gridB is 5x4: the box is clamped to it
	EXPECT_EQ(0u, rig.registry.SelectionFor(&rig.grid.desc)->Count());
}

// ---- tool command and gesture interaction

TEST_F(SelectionExhaustiveCommandsTest, SwitchingToPanMidGestureCancelsTheGesture)
{
	Rig rig(66, 42);
	CellPickerView view;
	view.viewportWidth = 1280; view.viewportHeight = 720; view.cameraX = 50; view.cameraY = 40; view.zoom = 8.0f;
	const Dia::Maths::Vector2D c = HexTopology::CellCenter(CellCoord{ 12, 8 }, 1.0f);
	SelectionInput::PointerFrame f;
	WorldToScreen(view, c.X(), c.Y(), f.x, f.y);
	f.valid = true;
	f.left = true;
	rig.domain.SubmitPointerFrame(f, view);
	rig.domain.Update(rig.draw);
	ASSERT_TRUE(Exec("cells.select.tool", "{\"tool\":\"pan\"}")["success"].asBool());
	f.left = false;
	rig.domain.SubmitPointerFrame(f, view);
	rig.domain.Update(rig.draw);
	EXPECT_EQ(0u, rig.Sel()->Count());
	// pan tool: gestures produce nothing; back to select: the next click works
	Exec("cells.select.tool", "{\"tool\":\"select\"}");
	f.left = true;
	rig.domain.SubmitPointerFrame(f, view);
	rig.domain.Update(rig.draw);
	f.left = false;
	rig.domain.SubmitPointerFrame(f, view);
	rig.domain.Update(rig.draw);
	EXPECT_EQ(1u, rig.Sel()->Count());
}

TEST_F(SelectionExhaustiveCommandsTest, PointerFrameIsConsumedOnceEvenWithoutAnActiveGrid)
{
	CellFieldRegistry cf;
	CellLayerRegistry registry(nullptr, &FakePuIdFn);
	LayerUploader uploader(cf, nullptr);
	CellLayerViewerDomain domain(registry, uploader, nullptr);
	FakeDebugDraw draw;
	domain.RegisterCommands();
	CellPickerView view;
	SelectionInput::PointerFrame f;
	f.left = true;
	domain.SubmitPointerFrame(f, view);
	domain.Update(draw);
	domain.Update(draw);
	EXPECT_EQ(nullptr, domain.ActiveSelectionView());
}

TEST_F(SelectionExhaustiveCommandsTest, SubmittedFrameIsProcessedOnlyOncePerSubmit)
{
	Rig rig(66, 42);
	CellPickerView view;
	view.viewportWidth = 1280; view.viewportHeight = 720; view.cameraX = 50; view.cameraY = 40; view.zoom = 8.0f;
	const Dia::Maths::Vector2D c = HexTopology::CellCenter(CellCoord{ 12, 8 }, 1.0f);
	SelectionInput::PointerFrame f;
	WorldToScreen(view, c.X(), c.Y(), f.x, f.y);
	f.valid = true;
	f.left = true;
	rig.domain.SubmitPointerFrame(f, view);
	rig.domain.Update(rig.draw);
	f.left = false;
	rig.domain.SubmitPointerFrame(f, view);
	rig.domain.Update(rig.draw);
	ASSERT_EQ(1u, rig.Sel()->Count());
	rig.Sel()->Clear();
	const uint32_t v = rig.Sel()->Version();
	rig.domain.Update(rig.draw);		// the release frame is not replayed
	rig.domain.Update(rig.draw);
	EXPECT_EQ(v, rig.Sel()->Version());
	EXPECT_EQ(0u, rig.Sel()->Count());
}

// ---- concurrency

TEST_F(SelectionExhaustiveCommandsTest, CommandsAndPointerFramesFromOtherThreadsWhileTheDomainUpdates)
{
	Rig rig(66, 42);
	CellPickerView view;
	view.viewportWidth = 1280; view.viewportHeight = 720; view.cameraX = 50; view.cameraY = 40; view.zoom = 8.0f;
	std::atomic<bool> stop{ false };
	std::atomic<uint32_t> failures{ 0 };

	std::vector<std::thread> workers;
	for (int t = 0; t < 3; ++t)
	{
		workers.emplace_back([&, t]()
		{
			for (int i = 0; i < 150; ++i)
			{
				char buf[96];
				snprintf(buf, sizeof(buf), "{\"q\":%d,\"r\":%d}", (i * 7 + t) % 66, (i * 3 + t) % 42);
				const Json::Value a = Exec(i % 3 == 0 ? "cells.select.cell" : "cells.select.toggle", buf);
				const Json::Value b = Exec("cells.select.category", "{\"layer\":\"terrain\",\"value\":\"water\"}");
				const Json::Value c = Exec(i % 10 == 0 ? "cells.select.clear" : "cells.select.box", "{\"q0\":0,\"r0\":0,\"q1\":3,\"r1\":3}");
				if (!a["success"].asBool() || !b["success"].asBool() || !c["success"].asBool()) { ++failures; }
			}
		});
	}
	workers.emplace_back([&]()
	{
		SelectionInput::PointerFrame f;
		f.valid = true;
		for (int i = 0; i < 300; ++i)
		{
			f.x = 100.0f + float(i % 50) * 10.0f;
			f.y = 100.0f + float(i % 30) * 10.0f;
			f.left = (i % 2) == 0;
			rig.domain.SubmitPointerFrame(f, view);
		}
	});
	for (int i = 0; i < 200; ++i) { rig.domain.Update(rig.draw); }
	for (std::thread& w : workers) { w.join(); }
	stop = true;

	EXPECT_EQ(0u, failures.load());
	rig.domain.Update(rig.draw);
	CellSelection* sel = rig.Sel();
	ASSERT_NE(nullptr, sel);
	uint32_t visited = 0;
	sel->ForEach([](uint32_t, void* u) { ++*static_cast<uint32_t*>(u); }, &visited);
	EXPECT_EQ(sel->Count(), visited);
	EXPECT_LE(sel->Count(), 66u * 42u);
}

#endif // DIA_DEBUG
