#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/ViewerCommands.h>
#include <DiaDebugServer/SimQueryHost.h>
#include <DiaAPI/DiaAPI.h>

#include <algorithm>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

// Streams Migration Task 3: the viewer's commands are sim-hosted (registered through the registrar seam), never in the DiaAPI
// CommandRegistry, and cells.layer.list stays a single handler (the mirror query).

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;

namespace
{
	const char* const kViewerCommandNames[] = {
		"cells.layer.base_by_id", "cells.overlay.toggle_by_id",
		"cells.select.cell", "cells.select.toggle", "cells.select.box", "cells.select.poly",
		"cells.select.category", "cells.select.cells", "cells.select.clear",
		"cells.select.query", "cells.query.check",
		"cells.select.tool", "cells.select.group", "cells.select.pin", "cells.select.unpin",
		"cells.inspect", "cells.inspector.expand"
	};

	struct Rig
	{
		TestGrid grid;
		CellFieldRegistry cellField;
		StubProvider temp;
		FakeDebugDraw draw;
		CellLayerRegistry registry;
		LayerUploader uploader;
		CellLayerViewerDomain domain;

		Rig()
			: grid(20, 20)
			, registry(nullptr, &FakePuIdFn)
			, uploader(cellField, nullptr)
			, domain(registry, uploader, nullptr)
		{
			temp.values = std::vector<float>(400, 1.0f);
			registry.Register(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
			domain.Update(draw);
		}
	};

	Json::Value Obj() { return Json::Value(Json::objectValue); }

	struct ViewerSimHostedCommandsTest : ::testing::Test
	{
		void SetUp() override { FakePuId() = 1; Dia::API::Initialize(); }
		void TearDown() override { Dia::API::Shutdown(); }
	};
}

TEST_F(ViewerSimHostedCommandsTest, RegistersExactlyTheViewerCommandsAsCommands)
{
	Rig rig;
	FakeMirrorRegistrar reg;
	ASSERT_TRUE(rig.domain.RegisterCommands(reg));
	std::vector<std::string> got = reg.Names();
	std::vector<std::string> want(std::begin(kViewerCommandNames), std::end(kViewerCommandNames));
	std::sort(got.begin(), got.end());
	std::sort(want.begin(), want.end());
	EXPECT_EQ(got, want);
	for (const char* n : kViewerCommandNames) { EXPECT_TRUE(reg.IsCommand(n)) << n; }
	EXPECT_FALSE(reg.Has("cells.layer.list"));
	rig.domain.UnregisterCommands(reg);
	EXPECT_EQ(reg.Count(), 0u);
}

TEST_F(ViewerSimHostedCommandsTest, NothingIsRegisteredInTheDiaApiCommandRegistry)
{
	Rig rig;
	FakeMirrorRegistrar reg;
	ASSERT_TRUE(rig.domain.RegisterCommands(reg));
	for (const char* n : kViewerCommandNames)
	{
		EXPECT_FALSE(Dia::API::ExecuteCommandJson(StringCRC(n), Obj())["success"].asBool()) << n;
		EXPECT_EQ(Dia::API::ExecuteCommandJson(StringCRC(n), Obj())["error"].asString(), "command not found") << n;
		EXPECT_EQ(Dia::API::GetCommand(StringCRC(n)), nullptr) << n;
	}
	EXPECT_EQ(Dia::API::GetCommand(StringCRC("cells.layer.base")), nullptr);
	EXPECT_EQ(Dia::API::GetCommand(StringCRC("cells.overlay.toggle")), nullptr);
	rig.domain.UnregisterCommands(reg);
}

TEST_F(ViewerSimHostedCommandsTest, RegisterIsIdempotentAndRollsBackOnRejection)
{
	Rig rig;
	FakeMirrorRegistrar reg;
	ASSERT_TRUE(rig.domain.RegisterCommands(reg));
	const size_t count = reg.Count();
	EXPECT_TRUE(rig.domain.RegisterCommands(reg));	// idempotent
	EXPECT_EQ(reg.Count(), count);
	rig.domain.UnregisterCommands(reg);

	FakeMirrorRegistrar blocked;
	ASSERT_TRUE(blocked.RegisterCommand("cells.select.clear", [](const Json::Value&) { return Json::Value(Json::objectValue); }));
	EXPECT_FALSE(rig.domain.RegisterCommands(blocked));
	EXPECT_EQ(blocked.Count(), 1u);	// the pre-existing one only: everything this call registered was rolled back
}

TEST_F(ViewerSimHostedCommandsTest, LayerListHasExactlyOneHandlerTheMirrorQuery)
{
	Rig rig;
	MirrorQueries queries(rig.registry, rig.domain, nullptr);
	FakeMirrorRegistrar reg;
	ASSERT_TRUE(RegisterMirrorQueries(reg, queries));
	ASSERT_TRUE(rig.domain.RegisterCommands(reg));
	size_t listCount = 0;
	for (const std::string& n : reg.Names()) { if (n == "cells.layer.list") { ++listCount; } }
	EXPECT_EQ(listCount, 1u);
	EXPECT_FALSE(reg.IsCommand("cells.layer.list"));	// the mirror query (superset), not a viewer command
	const Json::Value list = reg.Execute("cells.layer.list", Obj());
	EXPECT_TRUE(list.isMember("layersVersion"));
	EXPECT_TRUE(list.isMember("activeBase"));
	rig.domain.UnregisterCommands(reg);
	UnregisterMirrorQueries(reg);
}

TEST_F(ViewerSimHostedCommandsTest, SimQueryHostKeepsTheFirstHandlerOnDuplicateName)
{
	Dia::DebugServer::SimQueryHost host;
	EXPECT_TRUE(host.RegisterQuery(StringCRC("cells.layer.list"), [](const Json::Value&) { return Json::Value(Json::objectValue); }));
	EXPECT_FALSE(host.RegisterCommand(StringCRC("cells.layer.list"), [](const Json::Value&) { return Json::Value(Json::objectValue); }));
	EXPECT_FALSE(host.RegisterQuery(StringCRC("cells.layer.list"), [](const Json::Value&) { return Json::Value(Json::objectValue); }));
	EXPECT_TRUE(host.Has(StringCRC("cells.layer.list")));
	EXPECT_TRUE(host.UnregisterQuery(StringCRC("cells.layer.list")));
	EXPECT_FALSE(host.Has(StringCRC("cells.layer.list")));
}

TEST_F(ViewerSimHostedCommandsTest, HandlersInvokedInOrderSeeEarlierEffects)
{
	Rig rig;
	MirrorQueries queries(rig.registry, rig.domain, nullptr);
	FakeMirrorRegistrar reg;
	ASSERT_TRUE(RegisterMirrorQueries(reg, queries));
	ASSERT_TRUE(rig.domain.RegisterCommands(reg));

	EXPECT_EQ(reg.Execute("cells.select.get", Obj())["total"].asUInt(), 0u);
	Json::Value box(Json::objectValue);
	box["q0"] = 0; box["r0"] = 0; box["q1"] = 1; box["r1"] = 1;
	const Json::Value boxed = reg.Execute("cells.select.box", box);
	EXPECT_TRUE(boxed["ok"].asBool());
	const Json::Value got = reg.Execute("cells.select.get", Obj());	// next request in the stream: sees the box
	EXPECT_EQ(got["total"].asUInt(), 4u);
	EXPECT_EQ(got["indices"].size(), 4u);
	reg.Execute("cells.select.clear", Obj());
	EXPECT_EQ(reg.Execute("cells.select.get", Obj())["total"].asUInt(), 0u);

	rig.domain.UnregisterCommands(reg);
	UnregisterMirrorQueries(reg);
}

#endif // DIA_DEBUG
