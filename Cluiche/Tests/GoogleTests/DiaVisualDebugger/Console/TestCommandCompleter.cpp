////////////////////////////////////////////////////////////////////////////////
// Filename: TestCommandCompleter.cpp
// Description: Unit tests for CommandCompleter (AC-6).
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaVisualDebugger/Console/CommandCompleter.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstring>

using namespace Dia::VisualDebugger::Console;
using Dia::Core::StringCRC;

namespace
{
	const char* kNames[] = {
		"bus_traffic.start", "bus_traffic.stop", "bus_traffic.status",
		"bus_traffic.reset", "bus_traffic.dump", "bus_traffic.clear", "xx_bus_other"
	};
	const unsigned kNameCount = sizeof(kNames) / sizeof(kNames[0]);
}

class CommandCompleterTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!Dia::API::IsInitialized())
		{
			Dia::API::Initialize();
			mInitializedHere = true;
		}
		for (unsigned i = 0; i < kNameCount; ++i)
		{
			Dia::API::CommandInfo info;
			info.name = StringCRC(kNames[i]);
			info.description = "completer test command";
			info.category = StringCRC("test");
			info.owner = "TestCommandCompleter";
			info.version = "1.0.0";
			info.example = kNames[i];
			info.callback = [](const Dia::API::CommandArgs&) { return 0; };
			ASSERT_TRUE(Dia::API::RegisterCommand(info));
		}
	}

	void TearDown() override
	{
		for (unsigned i = 0; i < kNameCount; ++i)
		{
			Dia::API::UnregisterCommand(StringCRC(kNames[i]));
		}
		if (mInitializedHere)
		{
			Dia::API::Shutdown();
		}
	}

	bool mInitializedHere = false;
};

TEST_F(CommandCompleterTest, AC6_PrefixMatchesComeFirstAndCapAtFive)
{
	Completion out[16];
	unsigned n = CommandCompleter::Complete("bus_", out, 5);
	ASSERT_EQ(n, 5u);
	for (unsigned i = 0; i < n; ++i)
	{
		EXPECT_EQ(std::strncmp(out[i].name, "bus_traffic.", 12), 0) << out[i].name;
		EXPECT_EQ(out[i].rank, MatchRank::Prefix);
	}
	// alphabetical among ties
	EXPECT_STREQ(out[0].name, "bus_traffic.clear");
}

TEST_F(CommandCompleterTest, AC6_ContainsRanksAfterPrefix)
{
	Completion out[16];
	unsigned n = CommandCompleter::Complete("bus_", out, 16);
	ASSERT_GE(n, 7u);
	EXPECT_STREQ(out[6].name, "xx_bus_other");
	EXPECT_EQ(out[6].rank, MatchRank::Contains);
}

TEST_F(CommandCompleterTest, AC6_UnregisteredCommandDisappearsOnNextCall)
{
	Completion out[16];
	unsigned before = CommandCompleter::Complete("bus_traffic.stop", out, 16);
	ASSERT_GE(before, 1u);
	EXPECT_STREQ(out[0].name, "bus_traffic.stop");
	EXPECT_EQ(out[0].rank, MatchRank::Exact);

	ASSERT_TRUE(Dia::API::UnregisterCommand(StringCRC("bus_traffic.stop")));
	unsigned after = CommandCompleter::Complete("bus_traffic.stop", out, 16);
	for (unsigned i = 0; i < after; ++i)
	{
		EXPECT_STRNE(out[i].name, "bus_traffic.stop");
	}
}

TEST_F(CommandCompleterTest, AC6_NewlyRegisteredCommandAppears)
{
	Completion out[16];
	EXPECT_EQ(CommandCompleter::Complete("zzq_new", out, 16), 0u);

	Dia::API::CommandInfo info;
	info.name = StringCRC("zzq_new_cmd");
	info.description = "x";
	info.category = StringCRC("test");
	info.owner = "TestCommandCompleter";
	info.version = "1.0.0";
	info.example = "zzq_new_cmd";
	info.callback = [](const Dia::API::CommandArgs&) { return 0; };
	ASSERT_TRUE(Dia::API::RegisterCommand(info));

	unsigned n = CommandCompleter::Complete("zzq_new", out, 16);
	EXPECT_EQ(n, 1u);
	EXPECT_STREQ(out[0].name, "zzq_new_cmd");
	Dia::API::UnregisterCommand(StringCRC("zzq_new_cmd"));
}

TEST_F(CommandCompleterTest, AC6_InputWithSpaceReturnsZero)
{
	Completion out[16];
	EXPECT_EQ(CommandCompleter::Complete("bus_traffic.start arg", out, 16), 0u);
	EXPECT_EQ(CommandCompleter::Complete("bus_ ", out, 16), 0u);
}
