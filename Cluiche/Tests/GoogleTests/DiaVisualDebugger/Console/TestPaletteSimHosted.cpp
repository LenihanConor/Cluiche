////////////////////////////////////////////////////////////////////////////////
// Filename: TestPaletteSimHosted.cpp
// Description: Sim-Hosted Command Help, Task 4 (AC 6): the palette index includes sim-hosted names with typed params,
//              read from an injected ISimHostedCatalogue; duplicates with DiaAPI commands are listed once.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/PaletteIndex.h>
#include <DiaVisualDebugger/Console/PanelController.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/ISimHostedCatalogue.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstring>
#include <memory>
#include <vector>
#include <string>

using namespace Dia::VisualDebugger::Console;
using Dia::CellLayers::Testing::LogCapture;
using Dia::Core::StringCRC;
using Dia::DebugServer::CopyHelp;
using Dia::DebugServer::DebugServer;
using Dia::DebugServer::SimHelpInfo;
using Dia::DebugServer::SimHelpStored;
using Dia::DebugServer::SimKind;

namespace
{
	SimHelpStored Help(const char* description, bool withParams)
	{
		static const Dia::API::ValueParam params[] = {
			{ "x", "int", true, "the column", "cells", true, 0.0f, 99.0f },
			{ "mode", "enum", false, "how", nullptr, false, 0.0f, 0.0f, "a|b" },
		};
		SimHelpInfo info;
		info.description = description;
		info.category = "cells";
		info.owner = "DiaCellLayers";
		if (withParams) { info.params = params; info.paramCount = 2; }
		else { info.noArgs = true; }
		SimHelpStored out;
		CopyHelp(info, out);
		return out;
	}

	int FindByName(const PaletteIndex& idx, const char* name)
	{
		for (int i = 0; i < idx.GetItemCount(); ++i)
		{
			if (std::strcmp(idx.GetItem(i).name, name) == 0) return i;
		}
		return -1;
	}

	int CountByName(const PaletteIndex& idx, const char* name)
	{
		int n = 0;
		for (int i = 0; i < idx.GetItemCount(); ++i)
		{
			if (std::strcmp(idx.GetItem(i).name, name) == 0) ++n;
		}
		return n;
	}
}

class PaletteSimHostedTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!Dia::API::IsInitialized()) { Dia::API::Initialize(); mInitializedHere = true; }
		mServer = std::make_unique<DebugServer>();
	}
	void TearDown() override
	{
		Dia::API::UnregisterCommand(StringCRC("psim.dup"));
		if (mInitializedHere) { Dia::API::Shutdown(); }
	}

	bool mInitializedHere = false;
	std::unique_ptr<DebugServer> mServer;
};

TEST_F(PaletteSimHostedTest, IncludesSimHostedNamesWithMetadataAndTypedParams)
{
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("psim.cells"), SimKind::Command, Help("Select cells", true)));
	PaletteIndex idx;
	idx.SetSimHostedCatalogue(mServer.get());
	idx.Build(Json::Value(Json::objectValue));

	const int i = FindByName(idx, "psim.cells");
	ASSERT_GE(i, 0);
	const PaletteItem& it = idx.GetItem(i);
	EXPECT_EQ(PaletteItemType::Command, it.type);
	EXPECT_STREQ("Select cells", it.description);
	EXPECT_STREQ("cells", it.category);
	ASSERT_EQ(2u, it.paramCount);
	ASSERT_NE(nullptr, it.params);
	EXPECT_STREQ("x", it.params[0].name);
	EXPECT_STREQ("int", it.params[0].type);
	EXPECT_TRUE(it.params[0].required);
	EXPECT_STREQ("the column", it.params[0].description);
	EXPECT_STREQ("cells", it.params[0].unit);
	EXPECT_TRUE(it.params[0].hasRange);
	EXPECT_FLOAT_EQ(99.0f, it.params[0].max);
	EXPECT_STREQ("enum", it.params[1].type);
	EXPECT_STREQ("a|b", it.params[1].enumValues);
}

TEST_F(PaletteSimHostedTest, ParamsStayValidAfterTheCatalogueChanges)
{
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("psim.cells"), SimKind::Command, Help("Select cells", true)));
	PaletteIndex idx;
	idx.SetSimHostedCatalogue(mServer.get());
	idx.Build(Json::Value(Json::objectValue));
	mServer->UnregisterSimHosted(StringCRC("psim.cells"));	// frees the catalogue entry the snapshot was built from

	const int i = FindByName(idx, "psim.cells");
	ASSERT_GE(i, 0);
	ASSERT_EQ(2u, idx.GetItem(i).paramCount);
	EXPECT_STREQ("the column", idx.GetItem(i).params[0].description);
	EXPECT_STREQ("Select cells", idx.GetItem(i).description);
}

TEST_F(PaletteSimHostedTest, NoArgsSimNameHasNoParams)
{
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("psim.plain"), SimKind::Query, Help("No args", false)));
	PaletteIndex idx;
	idx.SetSimHostedCatalogue(mServer.get());
	idx.Build(Json::Value(Json::objectValue));
	const int i = FindByName(idx, "psim.plain");
	ASSERT_GE(i, 0);
	EXPECT_EQ(0u, idx.GetItem(i).paramCount);
}

TEST_F(PaletteSimHostedTest, SimNamesAreSearchableAsCommands)
{
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("psim.cells"), SimKind::Command, Help("Select cells", true)));
	PaletteIndex idx;
	idx.SetSimHostedCatalogue(mServer.get());
	idx.Build(Json::Value(Json::objectValue));
	Dia::Core::Containers::DynamicArrayC<int, 50> out;
	ASSERT_GE(idx.Search("psim.cel", PaletteIndex::kTypeCommands, out), 1);
	EXPECT_STREQ("psim.cells", idx.GetItem(out[0]).name);
}

TEST_F(PaletteSimHostedTest, NameInBothIsListedOnceFromTheDiaApiEntryWithAWarning)
{
	Dia::API::CommandInfo info;
	info.name = StringCRC("psim.dup");
	info.description = "from the api";
	info.category = StringCRC("apicat");
	info.owner = "TestPaletteSimHosted";
	info.version = "1.0.0";
	info.example = "";
	info.callback = [](const Dia::API::CommandArgs&) { return 0; };
	ASSERT_TRUE(Dia::API::RegisterCommand(info));
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("psim.dup"), SimKind::Command, Help("from the sim", true)));

	LogCapture log("Debug");
	PaletteIndex idx;
	idx.SetSimHostedCatalogue(mServer.get());
	idx.Build(Json::Value(Json::objectValue));
	EXPECT_EQ(1, CountByName(idx, "psim.dup"));
	EXPECT_STREQ("from the api", idx.GetItem(FindByName(idx, "psim.dup")).description);
	EXPECT_EQ(1u, log.Warnings("psim.dup"));
}

TEST_F(PaletteSimHostedTest, EmptyOrAbsentCatalogueChangesNothing)
{
	PaletteIndex plain;
	plain.Build(Json::Value(Json::objectValue));

	PaletteIndex withEmpty;
	withEmpty.SetSimHostedCatalogue(mServer.get());
	withEmpty.Build(Json::Value(Json::objectValue));
	EXPECT_EQ(plain.GetItemCount(), withEmpty.GetItemCount());

	PaletteIndex cleared;
	cleared.SetSimHostedCatalogue(mServer.get());
	cleared.SetSimHostedCatalogue(nullptr);
	cleared.Build(Json::Value(Json::objectValue));
	EXPECT_EQ(plain.GetItemCount(), cleared.GetItemCount());
}

TEST_F(PaletteSimHostedTest, UnregisteredNameIsGoneAfterRebuild)
{
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("psim.cells"), SimKind::Command, Help("Select cells", true)));
	PaletteIndex idx;
	idx.SetSimHostedCatalogue(mServer.get());
	idx.Build(Json::Value(Json::objectValue));
	ASSERT_GE(FindByName(idx, "psim.cells"), 0);
	mServer->UnregisterSimHosted(StringCRC("psim.cells"));
	idx.Build(Json::Value(Json::objectValue));
	EXPECT_EQ(-1, FindByName(idx, "psim.cells"));
}

TEST_F(PaletteSimHostedTest, RebuildDoesNotAccumulateSimItems)
{
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("psim.cells"), SimKind::Command, Help("Select cells", true)));
	PaletteIndex idx;
	idx.SetSimHostedCatalogue(mServer.get());
	idx.Build(Json::Value(Json::objectValue));
	const int first = idx.GetItemCount();
	idx.Build(Json::Value(Json::objectValue));
	idx.Build(Json::Value(Json::objectValue));
	EXPECT_EQ(first, idx.GetItemCount());
}

TEST_F(PaletteSimHostedTest, SimNamesAreNeverInTheDiaApiRegistry)
{
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("psim.cells"), SimKind::Command, Help("Select cells", true)));
	PaletteIndex idx;
	idx.SetSimHostedCatalogue(mServer.get());
	idx.Build(Json::Value(Json::objectValue));
	EXPECT_EQ(nullptr, Dia::API::GetCommand(StringCRC("psim.cells")));
}

TEST_F(PaletteSimHostedTest, PanelControllerPassesTheCatalogueToItsPalette)
{
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("psim.cells"), SimKind::Command, Help("Select cells", true)));
	struct Pusher : IPanelPusher
	{
		std::string mJson;
		bool Push(const char* fn, const char* json) override { if (std::strcmp(fn, "paletteResults") == 0) mJson = json; return true; }
	} pusher;
	PanelController panel;
	panel.SetSimHostedCatalogue(mServer.get());
	panel.RequestPaletteOpen();
	panel.ProcessWithRoot(pusher, nullptr);
	EXPECT_NE(std::string::npos, pusher.mJson.find("psim.cells")) << pusher.mJson;
}


// ---- Exhaustive pass (Task 7) ----

namespace
{
	SimHelpStored SixteenParamHelp(const std::string& description)
	{
		static std::vector<std::string> names;
		if (names.empty()) { for (int i = 0; i < 16; ++i) names.push_back("p" + std::to_string(i)); }
		std::vector<Dia::API::ValueParam> params(16);
		for (int i = 0; i < 16; ++i)
			params[i] = { names[i].c_str(), i % 4 == 3 ? "enum" : "float", i % 2 == 0, "a parameter with a reasonably long description text", "m", true, -1.0f, 1.0f, i % 4 == 3 ? "a|b|c" : nullptr };
		SimHelpInfo info;
		info.description = description.c_str();
		info.category = "cells";
		info.owner = "DiaCellLayers";
		info.params = params.data();
		info.paramCount = 16;
		SimHelpStored out;
		EXPECT_EQ(CopyHelp(info, out), Dia::DebugServer::SimHelpResult::Ok);
		return out;
	}
}

TEST_F(PaletteSimHostedTest, ThirtyTwoSimNamesWithSixteenParamsEachKeepTheirOwnParams)
{
	for (int i = 0; i < 32; ++i)
		ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC(("psim.big" + std::to_string(i)).c_str()), SimKind::Command, SixteenParamHelp("help " + std::to_string(i)))) << i;
	PaletteIndex idx;
	idx.SetSimHostedCatalogue(mServer.get());
	idx.Build(Json::Value(Json::objectValue));
	for (int i = 0; i < 32; ++i)
	{
		const std::string n = "psim.big" + std::to_string(i);
		const int at = FindByName(idx, n.c_str());
		ASSERT_GE(at, 0) << n;
		const PaletteItem& it = idx.GetItem(at);
		EXPECT_STREQ(("help " + std::to_string(i)).c_str(), it.description) << n;
		ASSERT_EQ(16u, it.paramCount) << n;
		EXPECT_STREQ("p15", it.params[15].name) << n;
		EXPECT_STREQ("enum", it.params[15].type) << n;
		EXPECT_STREQ("a|b|c", it.params[15].enumValues) << n;
		EXPECT_STREQ("p0", it.params[0].name) << n;
	}
}

TEST_F(PaletteSimHostedTest, SnapshotStaysValidWhileTheCatalogueIsMutatedAndFreed)
{
	for (int i = 0; i < 8; ++i)
		ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC(("psim.snap" + std::to_string(i)).c_str()), SimKind::Command, SixteenParamHelp("snapshot " + std::to_string(i)))) << i;
	PaletteIndex idx;
	idx.SetSimHostedCatalogue(mServer.get());
	idx.Build(Json::Value(Json::objectValue));
	// Shuffle the catalogue under the held snapshot: unregister half, replace the rest wholesale.
	for (int i = 0; i < 8; i += 2) mServer->UnregisterSimHosted(StringCRC(("psim.snap" + std::to_string(i)).c_str()));
	Dia::DebugServer::SimHelpStored other = Help("totally different", false);
	Dia::DebugServer::SimHostedName repl;
	std::memset(&repl, 0, sizeof(repl));
	std::strcpy(repl.name, "psim.replaced");
	repl.kind = SimKind::Query;
	repl.help = &other;
	ASSERT_TRUE(mServer->ReplaceSimHosted(&repl, 1));
	for (int i = 0; i < 8; ++i)
	{
		const std::string n = "psim.snap" + std::to_string(i);
		const int at = FindByName(idx, n.c_str());
		ASSERT_GE(at, 0) << n;
		EXPECT_STREQ(("snapshot " + std::to_string(i)).c_str(), idx.GetItem(at).description);
		ASSERT_EQ(16u, idx.GetItem(at).paramCount);
		EXPECT_STREQ("p7", idx.GetItem(at).params[7].name);
	}
	EXPECT_EQ(-1, FindByName(idx, "psim.replaced"));
}

TEST_F(PaletteSimHostedTest, NameAlsoRegisteredAsAJsonCommandIsListedOnceAsTheSimEntry)
{
	// The palette has never listed JSON-callback commands, so there is nothing to collide with: exactly one entry.
	Dia::API::CommandInfoJson info;
	info.name = StringCRC("psim.dup");
	info.description = "from the json api";
	info.category = StringCRC("apicat");
	info.owner = "TestPaletteSimHosted";
	info.callback = [](const Json::Value&) { return Json::Value(Json::objectValue); };
	ASSERT_TRUE(Dia::API::RegisterCommandJson(info));
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("psim.dup"), SimKind::Command, Help("from the sim", true)));
	PaletteIndex idx;
	idx.SetSimHostedCatalogue(mServer.get());
	idx.Build(Json::Value(Json::objectValue));
	Dia::API::UnregisterCommandJson(StringCRC("psim.dup"));
	EXPECT_EQ(1, CountByName(idx, "psim.dup"));
}

TEST_F(PaletteSimHostedTest, NullAndEmptyCategoryProduceTheSameItem)
{
	SimHelpInfo a; a.description = "d";
	SimHelpInfo b; b.description = "d"; b.category = ""; b.owner = "";
	SimHelpStored sa, sb;
	ASSERT_EQ(CopyHelp(a, sa), Dia::DebugServer::SimHelpResult::Ok);
	ASSERT_EQ(CopyHelp(b, sb), Dia::DebugServer::SimHelpResult::Ok);
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("psim.null"), SimKind::Query, sa));
	ASSERT_TRUE(mServer->RegisterSimHosted(StringCRC("psim.empty"), SimKind::Query, sb));
	PaletteIndex idx;
	idx.SetSimHostedCatalogue(mServer.get());
	idx.Build(Json::Value(Json::objectValue));
	const PaletteItem& n = idx.GetItem(FindByName(idx, "psim.null"));
	const PaletteItem& e = idx.GetItem(FindByName(idx, "psim.empty"));
	EXPECT_STREQ(n.category, e.category);
	EXPECT_STREQ("", n.category);
	EXPECT_EQ(n.paramCount, e.paramCount);
	EXPECT_EQ(nullptr, n.params);
}

#endif // DIA_DEBUG
