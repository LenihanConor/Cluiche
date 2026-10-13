////////////////////////////////////////////////////////////////////////////////
// Filename: TestPaletteJson.cpp
// Description: Unit tests for ConsoleJson::BuildPaletteResults.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-palette.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/ConsoleJson.h>
#include <DiaVisualDebugger/Console/PaletteIndex.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstdio>
#include <cstring>
#include <string>

using namespace Dia::VisualDebugger;
using namespace Dia::VisualDebugger::Console;
using Dia::Core::StringCRC;

namespace
{
	class StubJsonDomain : public IDebugDomain
	{
	public:
		StringCRC GetDomainId() const override { return StringCRC("PaletteJsonStubDomain"); }
		const char* GetDisplayName() const override { return "Palette Json Stub"; }
		const char* GetDescription() const override { return "Stub domain for palette json tests"; }
		StringCRC GetGroup() const override { return StringCRC("CoreDebug"); }
		Dia::Core::RGBA GetAccentColour() const override { return Dia::Core::RGBA(); }
		bool HasWorldDrawers() const override { return false; }
		void GetJSONState(Json::Value&) override {}
		void OnCommand(StringCRC, const Json::Value&) override {}
	};

	bool RegisterJsonCommand(const char* name, const char* description, const char* category, const char* example)
	{
		Dia::API::CommandInfo info;
		info.name = StringCRC(name);
		info.description = description;
		info.category = StringCRC(category);
		info.owner = "TestPaletteJson";
		info.version = "1.0.0";
		info.example = example;
		info.callback = [](const Dia::API::CommandArgs&) { return 0; };
		return Dia::API::RegisterCommand(info);
	}

	bool Parse(const char* text, Json::Value& root)
	{
		Json::Reader reader;
		return reader.parse(text, root);
	}
}

class PaletteJsonTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!Dia::API::IsInitialized())
		{
			Dia::API::Initialize();
			mInitializedHere = true;
		}
		ASSERT_TRUE(RegisterJsonCommand("pjson_alpha.start", "Start \"alpha\" with C:\\path", "pjson", "pjson_alpha.start --rate=10"));
		ASSERT_TRUE(RegisterJsonCommand("pjson_alpha.stop", "Stop alpha", "pjson", ""));
		mDomains.Register(mDomain);
	}

	void TearDown() override
	{
		mDomains.Clear();
		Dia::API::UnregisterCommand(StringCRC("pjson_alpha.start"));
		Dia::API::UnregisterCommand(StringCRC("pjson_alpha.stop"));
		for (int i = 0; i < 80; ++i)
		{
			char name[32];
			std::snprintf(name, sizeof(name), "pjson_bulk_%02d", i);
			Dia::API::UnregisterCommand(StringCRC(name));
		}
		if (mInitializedHere)
		{
			Dia::API::Shutdown();
		}
	}

	bool mInitializedHere = false;
	StubJsonDomain mDomain;
	DiaDebugDomainRegistry mDomains;
};

TEST_F(PaletteJsonTest, CountsDifferPerMask)
{
	PaletteIndex idx;
	idx.Build(mDomains);

	char buf[16384];
	ASSERT_TRUE(ConsoleJson::BuildPaletteResults(idx, "pjson", 3, buf, sizeof(buf)));
	Json::Value root;
	ASSERT_TRUE(Parse(buf, root));
	EXPECT_EQ(root["counts"]["commands"].asInt(), 2);
	EXPECT_EQ(root["counts"]["domains"].asInt(), 1);   // "Palette Json Stub" subsequence-matches "pjson"
	EXPECT_EQ(root["counts"]["all"].asInt(), root["counts"]["commands"].asInt() + root["counts"]["domains"].asInt());
	EXPECT_EQ(root["total"].asInt(), root["counts"]["all"].asInt());

	ASSERT_TRUE(ConsoleJson::BuildPaletteResults(idx, "pjson", 1, buf, sizeof(buf)));
	ASSERT_TRUE(Parse(buf, root));
	EXPECT_EQ(root["total"].asInt(), 2);
	EXPECT_EQ(root["counts"]["all"].asInt(), 3);   // counts do not depend on the filter
	EXPECT_EQ(root["items"].size(), 2u);
	EXPECT_EQ(root["items"][0]["type"].asString(), "command");

	ASSERT_TRUE(ConsoleJson::BuildPaletteResults(idx, "pjson", 2, buf, sizeof(buf)));
	ASSERT_TRUE(Parse(buf, root));
	EXPECT_EQ(root["total"].asInt(), 1);
	ASSERT_EQ(root["items"].size(), 1u);
	EXPECT_EQ(root["items"][0]["type"].asString(), "domain");
	EXPECT_EQ(root["items"][0]["action"].asString(), "reveal");
	EXPECT_EQ(root["items"][0]["actionText"].asString(), "PaletteJsonStubDomain");
	EXPECT_EQ(root["items"][0]["domainId"].asString(), "PaletteJsonStubDomain");
}

TEST_F(PaletteJsonTest, CommandItemHasFillActionAndFields)
{
	PaletteIndex idx;
	idx.Build(mDomains);

	char buf[16384];
	ASSERT_TRUE(ConsoleJson::BuildPaletteResults(idx, "pjson_alpha.start", 1, buf, sizeof(buf)));
	Json::Value root;
	ASSERT_TRUE(Parse(buf, root));
	ASSERT_GE(root["items"].size(), 1u);
	const Json::Value& item = root["items"][0];
	EXPECT_EQ(item["name"].asString(), "pjson_alpha.start");
	EXPECT_EQ(item["category"].asString(), "pjson");
	EXPECT_EQ(item["example"].asString(), "pjson_alpha.start --rate=10");
	EXPECT_EQ(item["action"].asString(), "fill");
	EXPECT_EQ(item["actionText"].asString(), "pjson_alpha.start --rate=10");
}

TEST_F(PaletteJsonTest, EscapesQuotesAndBackslashesInDescription)
{
	PaletteIndex idx;
	idx.Build(mDomains);

	char buf[16384];
	ASSERT_TRUE(ConsoleJson::BuildPaletteResults(idx, "pjson_alpha.start", 1, buf, sizeof(buf)));
	EXPECT_NE(std::strstr(buf, "\\\"alpha\\\""), nullptr);
	EXPECT_NE(std::strstr(buf, "C:\\\\path"), nullptr);
	Json::Value root;
	ASSERT_TRUE(Parse(buf, root));
	EXPECT_EQ(root["items"][0]["description"].asString(), "Start \"alpha\" with C:\\path");
}

TEST_F(PaletteJsonTest, EightyMatchesYieldFiftyItemsAndTotalEighty)
{
	for (int i = 0; i < 80; ++i)
	{
		char name[32];
		std::snprintf(name, sizeof(name), "pjson_bulk_%02d", i);
		ASSERT_TRUE(RegisterJsonCommand(name, "bulk", "pjson", ""));
	}
	PaletteIndex idx;
	idx.Build(mDomains);

	static char buf[65536];
	ASSERT_TRUE(ConsoleJson::BuildPaletteResults(idx, "pjson_bulk_", 1, buf, sizeof(buf)));
	Json::Value root;
	ASSERT_TRUE(Parse(buf, root));
	EXPECT_EQ(root["total"].asInt(), 80);
	EXPECT_EQ(root["counts"]["commands"].asInt(), 80);
	EXPECT_EQ(root["items"].size(), 50u);
}

TEST_F(PaletteJsonTest, HlMarksMatchedCharactersForPrefixQuery)
{
	PaletteIndex idx;
	idx.Build(mDomains);

	char buf[16384];
	ASSERT_TRUE(ConsoleJson::BuildPaletteResults(idx, "pjson_alpha.sta", 1, buf, sizeof(buf)));
	Json::Value root;
	ASSERT_TRUE(Parse(buf, root));
	ASSERT_GE(root["items"].size(), 1u);
	const Json::Value& item = root["items"][0];
	ASSERT_EQ(item["name"].asString(), "pjson_alpha.start");
	const std::string hl = item["hl"].asString();
	ASSERT_EQ(hl.size(), std::strlen("pjson_alpha.start"));
	EXPECT_EQ(hl, "11111111111111100");   // first 15 chars matched, "rt" not
}

TEST_F(PaletteJsonTest, EmptyQueryHasAllZeroHighlightAndAllCounts)
{
	PaletteIndex idx;
	idx.Build(mDomains);

	char buf[16384];
	ASSERT_TRUE(ConsoleJson::BuildPaletteResults(idx, "", 3, buf, sizeof(buf)));
	Json::Value root;
	ASSERT_TRUE(Parse(buf, root));
	EXPECT_EQ(root["total"].asInt(), idx.GetItemCount());
	ASSERT_GE(root["items"].size(), 1u);
	for (Json::ArrayIndex i = 0; i < root["items"].size(); ++i)
	{
		const std::string hl = root["items"][i]["hl"].asString();
		EXPECT_EQ(hl.find('1'), std::string::npos);
		EXPECT_EQ(hl.size(), root["items"][i]["name"].asString().size());
	}
}

TEST_F(PaletteJsonTest, TinyBufferTruncatesItemsButStaysValidJson)
{
	PaletteIndex idx;
	idx.Build(mDomains);

	char full[16384];
	ASSERT_TRUE(ConsoleJson::BuildPaletteResults(idx, "pjson", 3, full, sizeof(full)));
	Json::Value fullRoot;
	ASSERT_TRUE(Parse(full, fullRoot));

	// Big enough for the counts and about one item, too small for all three.
	char small[700];
	ASSERT_TRUE(ConsoleJson::BuildPaletteResults(idx, "pjson", 3, small, sizeof(small)));
	Json::Value root;
	ASSERT_TRUE(Parse(small, root));
	EXPECT_EQ(root["total"].asInt(), fullRoot["total"].asInt());
	EXPECT_EQ(root["counts"]["all"].asInt(), fullRoot["counts"]["all"].asInt());
	EXPECT_LT(root["items"].size(), fullRoot["items"].size());

	// Too small even for the counts: false, empty string.
	char tiny[16];
	EXPECT_FALSE(ConsoleJson::BuildPaletteResults(idx, "pjson", 3, tiny, sizeof(tiny)));
	EXPECT_STREQ(tiny, "");
}

#endif // DIA_DEBUG
