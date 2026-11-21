////////////////////////////////////////////////////////////////////////////////
// Filename: TestCommandInfoParams.cpp
// GoogleTest suite - CommandInfo / CommandInfoJson ValueParam metadata and
// registry Generation counter.
// Spec: docs/specs/applications/dia/systems/diavalueschema/code-side-descriptions.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaAPI/CommandRegistry/ValueParam.h>
#include <DiaCore/CRC/StringCRC.h>
#include <string>
#include <cstring>

using namespace Dia::API;
using Dia::Core::StringCRC;

// Sizes measured on x64 before the params/paramCount/noArgs fields were added.
static constexpr size_t kOldCommandInfoSize = 240;
static constexpr size_t kOldCommandInfoJsonSize = 224;
static constexpr size_t kMaxGrowth = 16; // pointer 8 + unsigned 4 + bool 1 + padding

static_assert(sizeof(CommandInfo) <= kOldCommandInfoSize + kMaxGrowth, "CommandInfo grew too much");
static_assert(sizeof(CommandInfoJson) <= kOldCommandInfoJsonSize + kMaxGrowth, "CommandInfoJson grew too much");

namespace
{
	const ValueParam kParams[2] = {
		{ "path", "string", true, "File path", "m", false, 0.0f, 0.0f, nullptr },
		{ "mode", "enum", false, "Mode", nullptr, true, 1.0f, 5.0f, "weather|ecology" },
	};

	int NoopCli(const CommandArgs&) { return 0; }
	Json::Value NoopJson(const Json::Value&) { return Json::Value(Json::objectValue); }

	const CommandInfo* FindIn(const Dia::Core::Containers::DynamicArrayC<CommandInfo, 256>& list, const char* name)
	{
		for (unsigned int i = 0; i < list.Size(); i++)
			if (std::strcmp(list[i].name.AsChar(), name) == 0)
				return &list[i];
		return nullptr;
	}

	const CommandInfoJson* FindInJson(const Dia::Core::Containers::DynamicArrayC<CommandInfoJson, 256>& list, const char* name)
	{
		for (unsigned int i = 0; i < list.Size(); i++)
			if (std::strcmp(list[i].name.AsChar(), name) == 0)
				return &list[i];
		return nullptr;
	}

	void ExpectParamsIntact(const ValueParam* p, unsigned int count)
	{
		ASSERT_EQ(p, kParams);
		ASSERT_EQ(count, 2u);
		EXPECT_STREQ(p[0].name, "path");
		EXPECT_TRUE(p[0].required);
		EXPECT_STREQ(p[0].unit, "m");
		EXPECT_FALSE(p[0].hasRange);
		EXPECT_STREQ(p[1].type, "enum");
		EXPECT_TRUE(p[1].hasRange);
		EXPECT_FLOAT_EQ(p[1].min, 1.0f);
		EXPECT_FLOAT_EQ(p[1].max, 5.0f);
		EXPECT_STREQ(p[1].enumValues, "weather|ecology");
	}
}

class CommandInfoParamsTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (IsInitialized())
			Shutdown();
		Initialize();
	}
	void TearDown() override
	{
		if (IsInitialized())
			Shutdown();
	}

	static CommandInfo MakeInfo(const char* name)
	{
		CommandInfo info;
		info.name = StringCRC(name);
		info.description = "desc";
		info.category = StringCRC("test");
		info.owner = "Test";
		info.version = "1.0.0";
		info.example = "";
		info.callback = NoopCli;
		return info;
	}

	static CommandInfoJson MakeJson(const char* name)
	{
		CommandInfoJson info;
		info.name = StringCRC(name);
		info.description = "desc";
		info.category = StringCRC("test");
		info.owner = "Test";
		info.callback = NoopJson;
		return info;
	}
};

TEST(CommandInfoParams, DefaultsAreEmpty)
{
	CommandInfo a{};
	EXPECT_EQ(a.params, nullptr);
	EXPECT_EQ(a.paramCount, 0u);
	EXPECT_FALSE(a.noArgs);

	CommandInfoJson j;
	EXPECT_EQ(j.params, nullptr);
	EXPECT_EQ(j.paramCount, 0u);
	EXPECT_FALSE(j.noArgs);
}

TEST(CommandInfoParams, CopyByValueKeepsPointer)
{
	CommandInfo a{};
	a.params = kParams;
	a.paramCount = 2;
	a.noArgs = false;
	CommandInfo b = a;
	ExpectParamsIntact(b.params, b.paramCount);

	CommandInfoJson j;
	j.params = kParams;
	j.paramCount = 2;
	CommandInfoJson k = j;
	ExpectParamsIntact(k.params, k.paramCount);
}

TEST(CommandInfoParams, SizeGrowthWithinBudget)
{
	EXPECT_LE(sizeof(CommandInfo), kOldCommandInfoSize + kMaxGrowth);
	EXPECT_LE(sizeof(CommandInfoJson), kOldCommandInfoJsonSize + kMaxGrowth);
}

TEST(CommandInfoParams, PositionalInitStillCompiles)
{
	CommandInfo info = { StringCRC("commandinfoparams.test.positional"), "desc", StringCRC("test"), "Test", "1.0.0", "ex", NoopCli };
	EXPECT_EQ(info.params, nullptr);
	EXPECT_EQ(info.paramCount, 0u);
	EXPECT_FALSE(info.noArgs);

	CommandInfoJson j = { StringCRC("commandinfoparams.test.positionaljson"), "desc", StringCRC("test"), "Test", NoopJson };
	EXPECT_EQ(j.params, nullptr);
}

TEST_F(CommandInfoParamsTest, PlainCommandRoundTrip)
{
	CommandInfo info = MakeInfo("commandinfoparams.test.plain");
	info.params = kParams;
	info.paramCount = 2;
	ASSERT_TRUE(RegisterCommand(info));

	Dia::Core::Containers::DynamicArrayC<CommandInfo, 256> list;
	ListCommandInfos(list);
	const CommandInfo* found = FindIn(list, "commandinfoparams.test.plain");
	ASSERT_NE(found, nullptr);
	ExpectParamsIntact(found->params, found->paramCount);
	EXPECT_FALSE(found->noArgs);

	CommandInfo copy{};
	ASSERT_TRUE(CopyCommandInfo(StringCRC("commandinfoparams.test.plain"), copy));
	ExpectParamsIntact(copy.params, copy.paramCount);

	const CommandInfo* direct = GetCommand(StringCRC("commandinfoparams.test.plain"));
	ASSERT_NE(direct, nullptr);
	ExpectParamsIntact(direct->params, direct->paramCount);
}

TEST_F(CommandInfoParamsTest, PlainNoArgsRoundTrip)
{
	CommandInfo info = MakeInfo("commandinfoparams.test.noargs");
	info.noArgs = true;
	ASSERT_TRUE(RegisterCommand(info));

	Dia::Core::Containers::DynamicArrayC<CommandInfo, 256> list;
	ListCommandInfos(list);
	const CommandInfo* found = FindIn(list, "commandinfoparams.test.noargs");
	ASSERT_NE(found, nullptr);
	EXPECT_TRUE(found->noArgs);
	EXPECT_EQ(found->params, nullptr);
	EXPECT_EQ(found->paramCount, 0u);
}

TEST_F(CommandInfoParamsTest, JsonCommandRoundTrip)
{
	CommandInfoJson info = MakeJson("commandinfoparams.test.json");
	info.params = kParams;
	info.paramCount = 2;
	ASSERT_TRUE(RegisterCommandJson(info));

	Dia::Core::Containers::DynamicArrayC<CommandInfoJson, 256> list;
	ListJsonCommandInfos(list);
	const CommandInfoJson* found = FindInJson(list, "commandinfoparams.test.json");
	ASSERT_NE(found, nullptr);
	ExpectParamsIntact(found->params, found->paramCount);
	EXPECT_FALSE(found->noArgs);
}

TEST_F(CommandInfoParamsTest, JsonNoArgsRoundTrip)
{
	CommandInfoJson info = MakeJson("commandinfoparams.test.jsonnoargs");
	info.noArgs = true;
	ASSERT_TRUE(RegisterCommandJson(info));

	Dia::Core::Containers::DynamicArrayC<CommandInfoJson, 256> list;
	ListJsonCommandInfos(list);
	const CommandInfoJson* found = FindInJson(list, "commandinfoparams.test.jsonnoargs");
	ASSERT_NE(found, nullptr);
	EXPECT_TRUE(found->noArgs);
	EXPECT_EQ(found->params, nullptr);
}

TEST_F(CommandInfoParamsTest, GenerationIncrementsOnEachRegistration)
{
	const unsigned int g0 = Generation();
	ASSERT_TRUE(RegisterCommand(MakeInfo("commandinfoparams.test.gen1")));
	const unsigned int g1 = Generation();
	EXPECT_EQ(g1, g0 + 1);
	ASSERT_TRUE(RegisterCommandJson(MakeJson("commandinfoparams.test.gen2")));
	const unsigned int g2 = Generation();
	EXPECT_EQ(g2, g1 + 1);

	// Failed (duplicate) registration does not bump.
	EXPECT_FALSE(RegisterCommand(MakeInfo("commandinfoparams.test.gen1")));
	EXPECT_EQ(Generation(), g2);
}

TEST_F(CommandInfoParamsTest, GenerationUnchangedByLookupsAndLists)
{
	ASSERT_TRUE(RegisterCommand(MakeInfo("commandinfoparams.test.genlookup")));
	const unsigned int g = Generation();

	GetCommand(StringCRC("commandinfoparams.test.genlookup"));
	CommandInfo copy{};
	CopyCommandInfo(StringCRC("commandinfoparams.test.genlookup"), copy);
	ListCommands();
	Dia::Core::Containers::DynamicArrayC<CommandInfo, 256> list;
	ListCommandInfos(list);
	Dia::Core::Containers::DynamicArrayC<CommandInfoJson, 256> jlist;
	ListJsonCommandInfos(jlist);
	EXPECT_EQ(Generation(), g);
}
