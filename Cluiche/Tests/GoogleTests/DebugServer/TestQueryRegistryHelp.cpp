#include <gtest/gtest.h>

#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/QueryRegistry.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

// Main-hosted query help: QueryRegistry::Register(name, handler, help) and its listing in BuildMergedCommandList.

using Dia::Core::StringCRC;
using Dia::DebugServer::BuildMergedCommandList;
using Dia::DebugServer::QueryRegistry;
using Dia::DebugServer::SimHelpInfo;
using Dia::CellLayers::Testing::LogCapture;

namespace
{
	Json::Value Ok(const Json::Value&) { return Json::Value(); }

	const Dia::API::ValueParam kParams[] = {
		{ "stage", "string", true, "the stage name" },
		{ "kind", "enum", false, "kind filter", nullptr, false, 0.0f, 0.0f, "a|b" },
	};

	SimHelpInfo Info(const char* description)
	{
		SimHelpInfo info;
		info.description = description;
		info.category = "mainq";
		info.owner = "TestOwner";
		info.params = kParams;
		info.paramCount = 2;
		return info;
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
}

TEST(QueryRegistryHelp, HelpIsStoredWithHandler)
{
	QueryRegistry reg;
	ASSERT_TRUE(reg.Register(StringCRC("mainq.a"), &Ok, Info("does a")));
	EXPECT_TRUE(reg.Has(StringCRC("mainq.a")));
	ASSERT_EQ(1u, reg.HelpCount());
	EXPECT_EQ(StringCRC("mainq.a"), reg.HelpNameAt(0));
	const auto* h = reg.HelpAt(0);
	ASSERT_NE(nullptr, h);
	EXPECT_STREQ("does a", h->description);
	EXPECT_STREQ("mainq", h->category);
	EXPECT_STREQ("TestOwner", h->owner);
	EXPECT_EQ(2u, h->paramCount);
	EXPECT_STREQ("stage", h->params[0].name);
	EXPECT_EQ(h, reg.FindHelp(StringCRC("mainq.a")));
	EXPECT_EQ(nullptr, reg.FindHelp(StringCRC("nope")));
}

TEST(QueryRegistryHelp, TwoArgRegisterHasNoHelp)
{
	QueryRegistry reg;
	ASSERT_TRUE(reg.Register(StringCRC("plain"), &Ok));
	EXPECT_TRUE(reg.Has(StringCRC("plain")));
	EXPECT_EQ(0u, reg.HelpCount());
	EXPECT_EQ(nullptr, reg.FindHelp(StringCRC("plain")));
}

TEST(QueryRegistryHelp, UnregisterRemovesHelp)
{
	QueryRegistry reg;
	ASSERT_TRUE(reg.Register(StringCRC("mainq.a"), &Ok, Info("a")));
	ASSERT_TRUE(reg.Register(StringCRC("mainq.b"), &Ok, Info("b")));
	ASSERT_EQ(2u, reg.HelpCount());
	EXPECT_TRUE(reg.Unregister(StringCRC("mainq.a")));
	EXPECT_EQ(1u, reg.HelpCount());
	EXPECT_EQ(nullptr, reg.FindHelp(StringCRC("mainq.a")));
	ASSERT_NE(nullptr, reg.FindHelp(StringCRC("mainq.b")));
	EXPECT_STREQ("b", reg.FindHelp(StringCRC("mainq.b"))->description);
	// Re-register with new help after removal gives the new text.
	ASSERT_TRUE(reg.Register(StringCRC("mainq.a"), &Ok, Info("a2")));
	EXPECT_STREQ("a2", reg.FindHelp(StringCRC("mainq.a"))->description);
}

TEST(QueryRegistryHelp, InvalidHelpIsRejectedLoggedAndNotRegistered)
{
	QueryRegistry reg;
	LogCapture log("DebugServer");
	SimHelpInfo bad = Info("");   // empty description is invalid
	EXPECT_FALSE(reg.Register(StringCRC("mainq.bad"), &Ok, bad));
	EXPECT_FALSE(reg.Has(StringCRC("mainq.bad")));
	EXPECT_EQ(0u, reg.HelpCount());
	EXPECT_EQ(1u, log.Errors("mainq.bad"));

	SimHelpInfo null;
	EXPECT_FALSE(reg.Register(StringCRC("mainq.null"), &Ok, null));
	EXPECT_FALSE(reg.Has(StringCRC("mainq.null")));
}

TEST(QueryRegistryHelp, WrongThreadRegisterWithHelpIsRefused)
{
	QueryRegistry reg;
	reg.SetOwnerThread(QueryRegistry::CurrentThreadId() + 1);
	LogCapture log("DebugServer");
	EXPECT_FALSE(reg.Register(StringCRC("mainq.t"), &Ok, Info("t")));
	EXPECT_FALSE(reg.Has(StringCRC("mainq.t")));
	EXPECT_EQ(0u, reg.HelpCount());
}

TEST(QueryRegistryHelp, MergedListShowsMainHelpEntriesWithSourceMain)
{
	QueryRegistry reg;
	ASSERT_TRUE(reg.Register(StringCRC("mainq.listed"), &Ok, Info("listed help")));
	ASSERT_TRUE(reg.Register(StringCRC("mainq.nohelp"), &Ok));

	const Json::Value list = BuildMergedCommandList(nullptr, &reg);
	const Json::Value* d = FindDetail(list, "mainq.listed");
	ASSERT_NE(nullptr, d);
	EXPECT_EQ("main", (*d)["source"].asString());
	EXPECT_EQ("query", (*d)["kind"].asString());
	EXPECT_EQ("listed help", (*d)["description"].asString());
	EXPECT_EQ("mainq", (*d)["category"].asString());
	EXPECT_EQ("TestOwner", (*d)["owner"].asString());
	ASSERT_EQ(2u, (*d)["params"].size());
	EXPECT_EQ("stage", (*d)["params"][0]["name"].asString());
	EXPECT_TRUE((*d)["params"][0]["required"].asBool());
	EXPECT_EQ("a|b", (*d)["params"][1]["enum"].asString());
	EXPECT_EQ(nullptr, FindDetail(list, "mainq.nohelp"));

	bool inNames = false;
	for (Json::ArrayIndex i = 0; i < list["api_commands"].size(); ++i)
		inNames = inNames || list["api_commands"][i].asString() == "mainq.listed";
	EXPECT_TRUE(inNames);
}

TEST(QueryRegistryHelp, MergedListWithoutRegistryUnchanged)
{
	const Json::Value a = BuildMergedCommandList(nullptr);
	const Json::Value b = BuildMergedCommandList(nullptr, nullptr);
	EXPECT_EQ(a["api_commands"].size(), b["api_commands"].size());
}

TEST(QueryRegistryHelp, DebugServerListsItsRegistrysHelp)
{
	Dia::DebugServer::DebugServer server;
	server.EnableAutoStart(false);
	server.Start();
	ASSERT_TRUE(server.GetQueryRegistry().Register(StringCRC("mainq.srv"), &Ok, Info("via server")));
	const Json::Value reply = server.GetQueryRegistry().Execute(StringCRC("list_commands"), Json::Value());
	const Json::Value* d = FindDetail(reply, "mainq.srv");
	ASSERT_NE(nullptr, d);
	EXPECT_EQ("main", (*d)["source"].asString());
	server.Stop();
}
