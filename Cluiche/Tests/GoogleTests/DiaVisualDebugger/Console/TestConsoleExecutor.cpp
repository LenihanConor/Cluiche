////////////////////////////////////////////////////////////////////////////////
// Filename: TestConsoleExecutor.cpp
// Description: Unit tests for ConsoleExecutor (AC-3) and ConsoleHistory (AC-4).
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaVisualDebugger/Console/ConsoleExecutor.h>
#include <DiaVisualDebugger/Console/ConsoleHistory.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

#include <memory>
#include <string>

using namespace Dia::VisualDebugger::Console;
using Dia::Core::StringCRC;

////////////////////////////////////////////////////////////////////////////////
// ConsoleExecutor (AC-3)
////////////////////////////////////////////////////////////////////////////////

namespace
{
	int gCallCount = 0;
	std::string gLastPositional;
	bool gLastFlag = false;
	std::string gLastNamed;

	const char* kOkName = "console_test.ok";
	const char* kFailName = "console_test.fail";
	const char* kEchoName = "console_test.echo";
}

class ConsoleExecutorTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!Dia::API::IsInitialized())
		{
			Dia::API::Initialize();
			mInitializedHere = true;
		}

		gCallCount = 0;
		gLastPositional.clear();
		gLastFlag = false;
		gLastNamed.clear();

		Register(kOkName, [](const Dia::API::CommandArgs&) { ++gCallCount; return 0; });
		Register(kFailName, [](const Dia::API::CommandArgs&) { ++gCallCount; return 7; });
		Register(kEchoName, [](const Dia::API::CommandArgs& args)
		{
			++gCallCount;
			gLastPositional = args.positionalArgs.Size() > 0 ? args.positionalArgs[0] : "";
			gLastFlag = args.HasFlag(StringCRC("v").Value());
			const char* named = args.GetNamedArg(StringCRC("key").Value());
			gLastNamed = named ? named : "";
			return 0;
		});
	}

	void TearDown() override
	{
		Dia::API::UnregisterCommand(StringCRC(kOkName));
		Dia::API::UnregisterCommand(StringCRC(kFailName));
		Dia::API::UnregisterCommand(StringCRC(kEchoName));
		if (mInitializedHere)
		{
			Dia::API::Shutdown();
		}
	}

	static void Register(const char* name, Dia::API::CommandCallback callback)
	{
		Dia::API::CommandInfo info;
		info.name = StringCRC(name);
		info.description = "console executor test command";
		info.category = StringCRC("test");
		info.owner = "TestConsoleExecutor";
		info.version = "1.0.0";
		info.example = name;
		info.callback = callback;
		ASSERT_TRUE(Dia::API::RegisterCommand(info));
	}

	bool mInitializedHere = false;
};

TEST_F(ConsoleExecutorTest, AC3_ReturnsZeroExitCode)
{
	ConsoleResult r = ExecuteConsoleCommand("console_test.ok");
	EXPECT_EQ(r.exitCode, 0);
	EXPECT_EQ(r.parseError, ParseError::None);
	EXPECT_EQ(gCallCount, 1);
}

TEST_F(ConsoleExecutorTest, AC3_ReturnsCommandSpecificExitCode)
{
	ConsoleResult r = ExecuteConsoleCommand("console_test.fail");
	EXPECT_EQ(r.exitCode, 7);
	EXPECT_EQ(r.parseError, ParseError::None);
	EXPECT_EQ(gCallCount, 1);
}

TEST_F(ConsoleExecutorTest, AC3_UnknownCommandReturnsThree)
{
	ConsoleResult r = ExecuteConsoleCommand("console_test.does_not_exist");
	EXPECT_EQ(r.exitCode, kConsoleExitNotFound);
	EXPECT_EQ(r.parseError, ParseError::None);
	EXPECT_EQ(gCallCount, 0);
}

TEST_F(ConsoleExecutorTest, ParseErrorReturnsTwoAndDoesNotExecute)
{
	ConsoleResult r = ExecuteConsoleCommand("console_test.ok \"unterminated");
	EXPECT_EQ(r.exitCode, kConsoleExitBadArguments);
	EXPECT_EQ(r.parseError, ParseError::UnterminatedQuote);

	r = ExecuteConsoleCommand("   ");
	EXPECT_EQ(r.exitCode, kConsoleExitBadArguments);
	EXPECT_EQ(r.parseError, ParseError::Empty);

	r = ExecuteConsoleCommand(nullptr);
	EXPECT_EQ(r.exitCode, kConsoleExitBadArguments);
	EXPECT_EQ(r.parseError, ParseError::Empty);

	EXPECT_EQ(gCallCount, 0);
}

TEST_F(ConsoleExecutorTest, ArgumentsReachTheCommand)
{
	ConsoleResult r = ExecuteConsoleCommand("console_test.echo \"hello world\" --v key=val");
	EXPECT_EQ(r.exitCode, 0);
	EXPECT_EQ(gCallCount, 1);
	EXPECT_EQ(gLastPositional, "hello world");
	EXPECT_TRUE(gLastFlag);
	EXPECT_EQ(gLastNamed, "val");
}

////////////////////////////////////////////////////////////////////////////////
// ConsoleHistory (AC-4)
////////////////////////////////////////////////////////////////////////////////

TEST(ConsoleHistory, EmptyHistoryReturnsNull)
{
	ConsoleHistory h;
	EXPECT_EQ(h.GetCount(), 0u);
	EXPECT_EQ(h.GetPrevious(), nullptr);
	EXPECT_EQ(h.GetNext(), nullptr);
}

TEST(ConsoleHistory, AC4_UpDownWalk)
{
	ConsoleHistory h;
	h.Add("one");
	h.Add("two");
	h.Add("three");

	EXPECT_STREQ(h.GetPrevious(), "three");
	EXPECT_STREQ(h.GetPrevious(), "two");
	EXPECT_STREQ(h.GetPrevious(), "one");
	EXPECT_STREQ(h.GetPrevious(), "one");	// clamps at oldest
	EXPECT_STREQ(h.GetNext(), "two");
	EXPECT_STREQ(h.GetNext(), "three");
	EXPECT_EQ(h.GetNext(), nullptr);		// back at the end
	EXPECT_EQ(h.GetNext(), nullptr);
	EXPECT_STREQ(h.GetPrevious(), "three");
}

TEST(ConsoleHistory, AC4_ResetReturnsToEnd)
{
	ConsoleHistory h;
	h.Add("one");
	h.Add("two");
	EXPECT_STREQ(h.GetPrevious(), "two");
	EXPECT_STREQ(h.GetPrevious(), "one");
	h.Reset();
	EXPECT_EQ(h.GetNext(), nullptr);
	EXPECT_STREQ(h.GetPrevious(), "two");
}

TEST(ConsoleHistory, AC4_AddResetsCursor)
{
	ConsoleHistory h;
	h.Add("one");
	h.Add("two");
	h.GetPrevious();
	h.GetPrevious();
	h.Add("three");
	EXPECT_STREQ(h.GetPrevious(), "three");
}

TEST(ConsoleHistory, AC4_ConsecutiveDuplicatesNotStored)
{
	ConsoleHistory h;
	h.Add("a");
	h.Add("a");
	h.Add("b");
	h.Add("b");
	h.Add("a");
	ASSERT_EQ(h.GetCount(), 3u);
	EXPECT_STREQ(h.GetEntry(0), "a");
	EXPECT_STREQ(h.GetEntry(1), "b");
	EXPECT_STREQ(h.GetEntry(2), "a");
}

TEST(ConsoleHistory, EmptyAndNullLinesNotStored)
{
	ConsoleHistory h;
	h.Add("");
	h.Add(nullptr);
	EXPECT_EQ(h.GetCount(), 0u);
}

TEST(ConsoleHistory, AC4_KeepsLastFifty)
{
	ConsoleHistory h;
	for (int i = 0; i < 75; ++i)
	{
		h.Add(("cmd" + std::to_string(i)).c_str());
	}
	ASSERT_EQ(h.GetCount(), ConsoleHistory::kMaxEntries);
	EXPECT_STREQ(h.GetEntry(0), "cmd25");
	EXPECT_STREQ(h.GetEntry(49), "cmd74");
	EXPECT_EQ(h.GetEntry(50), nullptr);

	// Walk all the way up: 50 distinct entries, newest first.
	for (int i = 74; i >= 25; --i)
	{
		EXPECT_EQ(std::string(h.GetPrevious()), "cmd" + std::to_string(i));
	}
	EXPECT_STREQ(h.GetPrevious(), "cmd25");
}

TEST(ConsoleHistory, LongEntriesTruncated)
{
	ConsoleHistory h;
	std::string longLine(400, 'x');
	h.Add(longLine.c_str());
	ASSERT_EQ(h.GetCount(), 1u);
	EXPECT_EQ(std::string(h.GetEntry(0)), std::string(ConsoleHistory::kMaxEntryLength, 'x'));

	// A different long line sharing the first 255 chars is a duplicate after truncation.
	h.Add((longLine + "y").c_str());
	EXPECT_EQ(h.GetCount(), 1u);
}

TEST(ConsoleHistory, ClearEmptiesHistory)
{
	ConsoleHistory h;
	h.Add("one");
	h.Clear();
	EXPECT_EQ(h.GetCount(), 0u);
	EXPECT_EQ(h.GetPrevious(), nullptr);
}
