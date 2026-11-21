#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <Modules/SimEchoTestModule.h>
#include <Modules/TestStages/CellLayersPointerQueue.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#ifdef DIA_DEBUG

// Sim-Hosted Command Help Task 5 (AC 10): every name the CellLayers viewer registers carries real help (description, category,
// owner, params matching the CellLayerViewerDomain comment block), and no placeholder text remains.

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::DebugServer::SimHelpStored;
using Dia::DebugServer::SimParamStored;

namespace
{
	struct CommandHelpRig
	{
		TestGrid grid{ 20, 20 };
		Dia::Graphics::CellFieldRegistry cellField;
		StubProvider temp;
		FakeDebugDraw draw;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		LayerUploader uploader{ cellField, nullptr };
		CellLayerViewerDomain domain{ registry, uploader, nullptr };
		MirrorQueries queries{ registry, domain, nullptr };
		FakeMirrorRegistrar reg;
		CluicheTest::PointerFrameQueue pointerQueue;
		CluicheTest::HoverOverride hoverOverride;

		CommandHelpRig()
		{
			FakePuId() = 1;
			temp.values = std::vector<float>(400, 1.0f);
			registry.Register(temp.MakeDesc("temp", &grid.desc, CellValueKind::Float, "cells/temp"));
			domain.Update(draw);
		}

		bool RegisterAll()
		{
			return RegisterMirrorQueries(reg, queries) && domain.RegisterCommands(reg) &&
				CluicheTest::RegisterPointerInjectionCommands(reg, pointerQueue, hoverOverride, [](const Json::Value&) { return Json::Value(Json::objectValue); });
		}
	};

	const SimParamStored* FindParam(const SimHelpStored& h, const char* name)
	{
		for (uint32_t i = 0; i < h.paramCount; ++i) { if (std::strcmp(h.params[i].name, name) == 0) { return &h.params[i]; } }
		return nullptr;
	}

	struct Expect { const char* name; uint32_t paramCount; bool noArgs; };
}

TEST(CellLayersCommandHelp, EveryRegisteredNameHasRealHelp)
{
	CommandHelpRig rig;
	ASSERT_TRUE(rig.RegisterAll());

	// Enumerate by registration, not by a hard-coded list: 4 mirror queries + 17 viewer commands + 3 pointer-injection names.
	const std::vector<std::string> names = rig.reg.Names();
	EXPECT_EQ(names.size(), 24u);
	for (const std::string& n : names)
	{
		const SimHelpStored* h = rig.reg.Help(n.c_str());
		ASSERT_NE(h, nullptr) << n;
		EXPECT_GT(std::strlen(h->description), 0u) << n;
		EXPECT_NE(std::string(h->description).rfind("TODO", 0), 0u) << n << ": placeholder description";
		EXPECT_EQ(std::string(h->description).find("TODO describe"), std::string::npos) << n;
		EXPECT_GT(std::strlen(h->category), 0u) << n;
		EXPECT_GT(std::strlen(h->owner), 0u) << n;
		EXPECT_TRUE(h->noArgs || h->paramCount > 0) << n << ": takes args but declares no params, or no-arg without noArgs";
		EXPECT_FALSE(h->noArgs && h->paramCount > 0) << n;
		for (uint32_t i = 0; i < h->paramCount; ++i)
		{
			EXPECT_GT(std::strlen(h->params[i].name), 0u) << n;
			EXPECT_GT(std::strlen(h->params[i].type), 0u) << n << "." << h->params[i].name;
			EXPECT_GT(std::strlen(h->params[i].description), 0u) << n << "." << h->params[i].name;
		}
	}
}

TEST(CellLayersCommandHelp, ParamsMatchTheDomainCommentBlock)
{
	CommandHelpRig rig;
	ASSERT_TRUE(rig.RegisterAll());

	const Expect expects[] = {
		{ "cells.layer.list", 0, true },
		{ "cells.select.get", 2, false },
		{ "cells.layer.values", 3, false },
		{ "cells.select.stats", 1, false },
		{ "cells.layer.base_by_id", 1, false },
		{ "cells.overlay.toggle_by_id", 1, false },
		{ "cells.select.cell", 2, false },
		{ "cells.select.toggle", 2, false },
		{ "cells.select.box", 4, false },
		{ "cells.select.poly", 1, false },
		{ "cells.select.category", 2, false },
		{ "cells.select.cells", 1, false },
		{ "cells.select.clear", 0, true },
		{ "cells.select.query", 1, false },
		{ "cells.query.check", 1, false },
		{ "cells.select.tool", 1, false },
		{ "cells.select.group", 1, false },
		{ "cells.select.pin", 1, false },
		{ "cells.select.unpin", 0, true },
		{ "cells.inspect", 4, false },
		{ "cells.inspector.expand", 2, false },
		{ "cluichetest.celllayers.pointer_frames", 2, false },	// frames, hover
		{ "cluichetest.celllayers.pointer_pending", 0, true },
		{ "cluichetest.celllayers.cell_pixel", 2, false },
	};
	for (const Expect& e : expects)
	{
		const SimHelpStored* h = rig.reg.Help(e.name);
		ASSERT_NE(h, nullptr) << e.name;
		EXPECT_EQ(h->paramCount, e.paramCount) << e.name;
		EXPECT_EQ(h->noArgs, e.noArgs) << e.name;
	}

	// Spot-check names, types and requiredness against the comment block.
	const SimHelpStored* tool = rig.reg.Help("cells.select.tool");
	ASSERT_NE(tool, nullptr);
	ASSERT_NE(FindParam(*tool, "tool"), nullptr);
	EXPECT_STREQ(FindParam(*tool, "tool")->type, "enum");
	EXPECT_STREQ(FindParam(*tool, "tool")->enumValues, "select|pan");
	EXPECT_TRUE(FindParam(*tool, "tool")->required);

	const SimHelpStored* box = rig.reg.Help("cells.select.box");
	ASSERT_NE(box, nullptr);
	for (const char* p : { "q0", "r0", "q1", "r1" })
	{
		ASSERT_NE(FindParam(*box, p), nullptr) << p;
		EXPECT_STREQ(FindParam(*box, p)->type, "int");
		EXPECT_TRUE(FindParam(*box, p)->required);
	}

	const SimHelpStored* values = rig.reg.Help("cells.layer.values");
	ASSERT_NE(values, nullptr);
	ASSERT_NE(FindParam(*values, "layer"), nullptr);
	EXPECT_TRUE(FindParam(*values, "layer")->required);
	ASSERT_NE(FindParam(*values, "count"), nullptr);
	EXPECT_FALSE(FindParam(*values, "count")->required);

	const SimHelpStored* pin = rig.reg.Help("cells.select.pin");
	ASSERT_NE(pin, nullptr);
	ASSERT_NE(FindParam(*pin, "freeze"), nullptr);
	EXPECT_STREQ(FindParam(*pin, "freeze")->type, "bool");
	EXPECT_FALSE(FindParam(*pin, "freeze")->required);

	const SimHelpStored* expand = rig.reg.Help("cells.inspector.expand");
	ASSERT_NE(expand, nullptr);
	ASSERT_NE(FindParam(*expand, "expanded"), nullptr);
	EXPECT_STREQ(FindParam(*expand, "expanded")->type, "bool");
	EXPECT_TRUE(FindParam(*expand, "expanded")->required);
}

TEST(CellLayersCommandHelp, CategoriesAndOwners)
{
	CommandHelpRig rig;
	ASSERT_TRUE(rig.RegisterAll());
	for (const std::string& n : rig.reg.Names())
	{
		const SimHelpStored* h = rig.reg.Help(n.c_str());
		ASSERT_NE(h, nullptr);
		if (n.rfind("cells.", 0) == 0)
		{
			EXPECT_STREQ(h->category, "cells") << n;
			EXPECT_STREQ(h->owner, "DiaCellLayers") << n;
		}
		else
		{
			EXPECT_STREQ(h->category, "test") << n;
			EXPECT_STREQ(h->owner, "CluicheTest") << n;
		}
	}
}

TEST(CellLayersCommandHelp, SimEchoHasADescription)
{
	const Dia::DebugServer::SimHelpInfo info = CluicheTest::SimEchoTestModule::HelpInfo();
	SimHelpStored stored;
	ASSERT_EQ(Dia::DebugServer::CopyHelp(info, stored), Dia::DebugServer::SimHelpResult::Ok);
	EXPECT_GT(std::strlen(stored.description), 0u);
	EXPECT_NE(std::string(stored.description).rfind("TODO", 0), 0u);
	EXPECT_STREQ(stored.category, "test");
	EXPECT_STREQ(stored.owner, "CluicheTest");
}

TEST(CellLayersCommandHelp, RegistrarRejectsInvalidHelp)
{
	FakeMirrorRegistrar reg;
	Dia::DebugServer::SimHelpInfo none;	// no description
	EXPECT_FALSE(reg.Register("x.y", none, [](const Json::Value&) { return Json::Value(Json::objectValue); }));
	EXPECT_EQ(reg.Count(), 0u);
}

#endif // DIA_DEBUG
