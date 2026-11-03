#include <gtest/gtest.h>

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/ISimHostedCatalogue.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaDebugServer/Testing/SimHelpTesting.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCore/CRC/StringCRC.h>

#include <memory>
#include <string>

// Sim-Hosted Command Help, Task 4 (AC 5, 9): the DebugServer command list merges DiaAPI commands and the sim-hosted catalogue.

namespace Dia
{
	namespace DebugServer
	{
		struct DebugServerTestAccess
		{
			static void RegisterProtocolCommands(DebugServer& s) { s.RegisterProtocolCommands(); }
		};
	}
}

namespace
{
	using Dia::CellLayers::Testing::LogCapture;
	using Dia::Core::StringCRC;
	using Dia::DebugServer::BuildMergedCommandList;
	using Dia::DebugServer::CopyHelp;
	using Dia::DebugServer::DebugServer;
	using Dia::DebugServer::SimHelpInfo;
	using Dia::DebugServer::SimHelpStored;
	using Dia::DebugServer::SimKind;

	std::unique_ptr<DebugServer> NewServer() { return std::make_unique<DebugServer>(); }

	SimHelpStored RichHelp(const char* description)
	{
		static const Dia::API::ValueParam params[] = {
			{ "x", "int", true, "the column" },
			{ "mode", "enum", false, "how", nullptr, false, 0.0f, 0.0f, "a|b" },
		};
		SimHelpInfo info;
		info.description = description;
		info.category = "cells";
		info.owner = "DiaCellLayers";
		info.params = params;
		info.paramCount = 2;
		SimHelpStored out;
		CopyHelp(info, out);
		return out;
	}

	const Json::Value* FindDetail(const Json::Value& list, const char* name)
	{
		const Json::Value& details = list["command_details"];
		for (Json::ArrayIndex i = 0; i < details.size(); ++i)
		{
			if (details[i]["name"].asString() == name) { return &details[i]; }
		}
		return nullptr;
	}

	unsigned CountDetail(const Json::Value& list, const char* name)
	{
		unsigned n = 0;
		const Json::Value& details = list["command_details"];
		for (Json::ArrayIndex i = 0; i < details.size(); ++i)
		{
			if (details[i]["name"].asString() == name) { ++n; }
		}
		return n;
	}

	unsigned CountName(const Json::Value& list, const char* name)
	{
		unsigned n = 0;
		const Json::Value& names = list["api_commands"];
		for (Json::ArrayIndex i = 0; i < names.size(); ++i)
		{
			if (names[i].asString() == name) { ++n; }
		}
		return n;
	}

	class SimHostedCommandListTest : public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			if (!Dia::API::IsInitialized()) { Dia::API::Initialize(); mInitializedHere = true; }
		}
		void TearDown() override
		{
			Dia::API::UnregisterCommand(StringCRC("cmdlist.dup"));
			Dia::API::UnregisterCommand(StringCRC("cmdlist.api_only"));
			if (mInitializedHere) { Dia::API::Shutdown(); }
		}

		static bool RegisterApi(const char* name, const char* description)
		{
			Dia::API::CommandInfo info;
			info.name = StringCRC(name);
			info.description = description;
			info.category = StringCRC("apicat");
			info.owner = "TestSimHostedCommandList";
			info.version = "1.0.0";
			info.example = "";
			info.callback = [](const Dia::API::CommandArgs&) { return 0; };
			return Dia::API::RegisterCommand(info);
		}

		bool mInitializedHere = false;
	};
}

TEST_F(SimHostedCommandListTest, SimNamesAppearWithDescriptionCategoryOwnerAndParams)
{
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cmdlist.sim_cmd"), SimKind::Command, RichHelp("sim command help")));

	const Json::Value list = BuildMergedCommandList(s.get());
	EXPECT_EQ(1u, CountName(list, "cmdlist.sim_cmd"));
	const Json::Value* d = FindDetail(list, "cmdlist.sim_cmd");
	ASSERT_NE(nullptr, d);
	EXPECT_EQ("sim", (*d)["source"].asString());
	EXPECT_EQ("command", (*d)["kind"].asString());
	EXPECT_EQ("sim command help", (*d)["description"].asString());
	EXPECT_EQ("cells", (*d)["category"].asString());
	EXPECT_EQ("DiaCellLayers", (*d)["owner"].asString());
	ASSERT_EQ(2u, (*d)["params"].size());
	EXPECT_EQ("x", (*d)["params"][0]["name"].asString());
	EXPECT_EQ("int", (*d)["params"][0]["type"].asString());
	EXPECT_TRUE((*d)["params"][0]["required"].asBool());
	EXPECT_EQ("the column", (*d)["params"][0]["description"].asString());
	EXPECT_EQ("a|b", (*d)["params"][1]["enum"].asString());
}

TEST_F(SimHostedCommandListTest, SimQueryKindIsReported)
{
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cmdlist.sim_q"), SimKind::Query, RichHelp("a query")));
	const Json::Value list = BuildMergedCommandList(s.get());
	const Json::Value* d = FindDetail(list, "cmdlist.sim_q");
	ASSERT_NE(nullptr, d);
	EXPECT_EQ("query", (*d)["kind"].asString());
}

TEST_F(SimHostedCommandListTest, MergesWithDiaApiCommands)
{
	ASSERT_TRUE(RegisterApi("cmdlist.api_only", "api help"));
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cmdlist.sim_cmd"), SimKind::Command, RichHelp("sim help")));

	const Json::Value list = BuildMergedCommandList(s.get());
	const Json::Value* api = FindDetail(list, "cmdlist.api_only");
	const Json::Value* sim = FindDetail(list, "cmdlist.sim_cmd");
	ASSERT_NE(nullptr, api);
	ASSERT_NE(nullptr, sim);
	EXPECT_EQ("api", (*api)["source"].asString());
	EXPECT_EQ("api help", (*api)["description"].asString());
	EXPECT_EQ("apicat", (*api)["category"].asString());
	EXPECT_EQ("sim", (*sim)["source"].asString());
}

TEST_F(SimHostedCommandListTest, NameInBothIsReportedOnceFromTheMainHostedEntryWithAWarning)
{
	ASSERT_TRUE(RegisterApi("cmdlist.dup", "from the api"));
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cmdlist.dup"), SimKind::Command, RichHelp("from the sim")));

	LogCapture log("DebugServer");
	const Json::Value list = BuildMergedCommandList(s.get());
	EXPECT_EQ(1u, CountName(list, "cmdlist.dup"));
	EXPECT_EQ(1u, CountDetail(list, "cmdlist.dup"));
	const Json::Value* d = FindDetail(list, "cmdlist.dup");
	ASSERT_NE(nullptr, d);
	EXPECT_EQ("api", (*d)["source"].asString());
	EXPECT_EQ("from the api", (*d)["description"].asString());
	EXPECT_EQ(1u, log.Warnings("cmdlist.dup"));
}

TEST_F(SimHostedCommandListTest, EmptyCatalogueAddsNothingAndNullCatalogueIsAccepted)
{
	ASSERT_TRUE(RegisterApi("cmdlist.api_only", "api help"));
	auto s = NewServer();
	const Json::Value withEmpty = BuildMergedCommandList(s.get());
	const Json::Value withNull = BuildMergedCommandList(nullptr);
	EXPECT_EQ(withNull["api_commands"].size(), withEmpty["api_commands"].size());
	EXPECT_EQ(withNull["command_details"].size(), withEmpty["command_details"].size());
	for (Json::ArrayIndex i = 0; i < withEmpty["command_details"].size(); ++i)
	{
		EXPECT_EQ("api", withEmpty["command_details"][i]["source"].asString());
	}
}

TEST_F(SimHostedCommandListTest, UnregisteredSimNameDisappears)
{
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cmdlist.sim_cmd"), SimKind::Command, RichHelp("sim help")));
	ASSERT_NE(nullptr, FindDetail(BuildMergedCommandList(s.get()), "cmdlist.sim_cmd"));
	s->UnregisterSimHosted(StringCRC("cmdlist.sim_cmd"));
	const Json::Value after = BuildMergedCommandList(s.get());
	EXPECT_EQ(nullptr, FindDetail(after, "cmdlist.sim_cmd"));
	EXPECT_EQ(0u, CountName(after, "cmdlist.sim_cmd"));
}

TEST_F(SimHostedCommandListTest, SimNamesStayOutOfTheDiaApiRegistry)
{
	auto s = NewServer();
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cmdlist.sim_cmd"), SimKind::Command, RichHelp("sim help")));
	(void)BuildMergedCommandList(s.get());
	EXPECT_EQ(nullptr, Dia::API::GetCommand(StringCRC("cmdlist.sim_cmd")));
}

TEST_F(SimHostedCommandListTest, ListCommandsQueryServesTheMergedList)
{
	auto s = NewServer();
	Dia::DebugServer::DebugServerTestAccess::RegisterProtocolCommands(*s);
	ASSERT_TRUE(s->RegisterSimHosted(StringCRC("cmdlist.sim_cmd"), SimKind::Command, RichHelp("sim help")));
	const Json::Value r = s->GetQueryRegistry().Execute(StringCRC("list_commands"), Json::Value(Json::objectValue));
	EXPECT_EQ(1u, CountName(r, "cmdlist.sim_cmd"));
	EXPECT_NE(nullptr, FindDetail(r, "cmdlist.sim_cmd"));
	EXPECT_TRUE(r.isMember("query_commands"));
}
