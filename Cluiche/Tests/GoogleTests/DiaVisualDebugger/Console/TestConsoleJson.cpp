////////////////////////////////////////////////////////////////////////////////
// Filename: TestConsoleJson.cpp
// Description: Unit tests for ConsoleJson (panel console JSON helpers).
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaVisualDebugger/Console/ConsoleJson.h>

#include <cstring>
#include <string>

using namespace Dia::VisualDebugger::Console;
using Dia::Observation::Log::LogLevel;

namespace
{
	ConsoleLogLine MakeLine(uint64_t ns, LogLevel level, const char* channel, const char* msg)
	{
		ConsoleLogLine l = {};
		l.timestampNs = ns;
		l.level = level;
		l.channel = Dia::Core::StringCRC(channel);
		strncpy_s(l.message, msg, sizeof(l.message) - 1);
		return l;
	}
}

TEST(ConsoleJsonTest, ResultPlain)
{
	char buf[256];
	ASSERT_TRUE(ConsoleJson::BuildConsoleResult(7, 3, 0, "foo --v", buf, sizeof(buf)));
	EXPECT_STREQ("{\"seq\":7,\"exitCode\":3,\"parseError\":0,\"text\":\"foo --v\"}", buf);
}

TEST(ConsoleJsonTest, ResultEscapesQuotesBackslashesAndControls)
{
	char buf[256];
	ASSERT_TRUE(ConsoleJson::BuildConsoleResult(1, 0, 0, "a\"b\\c\n\t\x01", buf, sizeof(buf)));
	EXPECT_STREQ("{\"seq\":1,\"exitCode\":0,\"parseError\":0,\"text\":\"a\\\"b\\\\c\\n\\t\\u0001\"}", buf);
}

TEST(ConsoleJsonTest, ResultPassesUtf8Through)
{
	char buf[256];
	ASSERT_TRUE(ConsoleJson::BuildConsoleResult(1, -2, 0, "caf\xC3\xA9", buf, sizeof(buf)));
	EXPECT_NE(nullptr, strstr(buf, "caf\xC3\xA9"));
	EXPECT_NE(nullptr, strstr(buf, "\"exitCode\":-2"));
}

TEST(ConsoleJsonTest, ResultNullTextIsEmpty)
{
	char buf[256];
	ASSERT_TRUE(ConsoleJson::BuildConsoleResult(1, 0, 0, nullptr, buf, sizeof(buf)));
	EXPECT_NE(nullptr, strstr(buf, "\"text\":\"\""));
}

TEST(ConsoleJsonTest, ResultTooSmallBufferFails)
{
	char buf[16];
	memset(buf, 'x', sizeof(buf));
	EXPECT_FALSE(ConsoleJson::BuildConsoleResult(1, 0, 0, "hello", buf, sizeof(buf)));
	EXPECT_STREQ("", buf);
	EXPECT_FALSE(ConsoleJson::BuildConsoleResult(1, 0, 0, "hello", nullptr, 0));
}

TEST(ConsoleJsonTest, LogLinesShapeAndLevels)
{
	ConsoleLogLine lines[4] = {
		MakeLine(1500000000ull, LogLevel::kInfo, "Debug", "one"),
		MakeLine(2000000000ull, LogLevel::kWarning, "Debug", "two"),
		MakeLine(2500000000ull, LogLevel::kError, "Debug", "th\"ree"),
		MakeLine(3000000000ull, LogLevel::kDebug, "Debug", "four"),
	};
	char buf[1024];
	unsigned written = 0;
	ASSERT_TRUE(ConsoleJson::BuildAppendLogLines(lines, 4, buf, sizeof(buf), &written));
	EXPECT_EQ(4u, written);
	EXPECT_EQ(0, strncmp(buf, "{\"lines\":[{\"t\":1.500,\"level\":\"INFO\",\"channel\":", 46));
	EXPECT_NE(nullptr, strstr(buf, "\"level\":\"WARN\""));
	EXPECT_NE(nullptr, strstr(buf, "\"level\":\"ERROR\""));
	EXPECT_NE(nullptr, strstr(buf, "\"level\":\"DEBUG\""));
	EXPECT_NE(nullptr, strstr(buf, "\"msg\":\"th\\\"ree\""));
	EXPECT_EQ(']', buf[strlen(buf) - 2]);
	EXPECT_EQ('}', buf[strlen(buf) - 1]);
}

TEST(ConsoleJsonTest, LogLinesEmptyList)
{
	char buf[64];
	unsigned written = 99;
	ASSERT_TRUE(ConsoleJson::BuildAppendLogLines(nullptr, 0, buf, sizeof(buf), &written));
	EXPECT_EQ(0u, written);
	EXPECT_STREQ("{\"lines\":[]}", buf);
}

TEST(ConsoleJsonTest, LogLinesTruncatedToWholeLinesAndStillBalanced)
{
	ConsoleLogLine lines[10];
	for (int i = 0; i < 10; ++i)
		lines[i] = MakeLine(1000000000ull * i, LogLevel::kInfo, "Debug", "message text here");

	char buf[300];
	unsigned written = 0;
	ASSERT_TRUE(ConsoleJson::BuildAppendLogLines(lines, 10, buf, sizeof(buf), &written));
	EXPECT_GT(written, 0u);
	EXPECT_LT(written, 10u);

	int open = 0, close = 0, ts = 0;
	for (const char* p = buf; *p; ++p)
	{
		if (*p == '{') ++open;
		if (*p == '}') ++close;
	}
	for (const char* p = buf; (p = strstr(p, "\"t\":")) != nullptr; p += 4) ++ts;
	EXPECT_EQ(open, close);
	EXPECT_EQ(static_cast<int>(written), ts);
	EXPECT_EQ('}', buf[strlen(buf) - 1]);
}

TEST(ConsoleJsonTest, LogLinesWrapperDoesNotFit)
{
	char buf[8];
	EXPECT_FALSE(ConsoleJson::BuildAppendLogLines(nullptr, 0, buf, sizeof(buf), nullptr));
}

TEST(ConsoleJsonTest, Completions)
{
	Completion items[2] = {};
	strcpy_s(items[0].name, "debug.layer.list");
	items[0].rank = MatchRank::Prefix;
	strcpy_s(items[1].name, "x\"y");
	items[1].rank = MatchRank::Subsequence;

	char buf[256];
	ASSERT_TRUE(ConsoleJson::BuildCompletions(items, 2, buf, sizeof(buf)));
	EXPECT_STREQ("{\"items\":[{\"name\":\"debug.layer.list\",\"rank\":\"prefix\"},"
				 "{\"name\":\"x\\\"y\",\"rank\":\"subsequence\"}]}", buf);

	ASSERT_TRUE(ConsoleJson::BuildCompletions(nullptr, 0, buf, sizeof(buf)));
	EXPECT_STREQ("{\"items\":[]}", buf);

	char tiny[20];
	EXPECT_FALSE(ConsoleJson::BuildCompletions(items, 2, tiny, sizeof(tiny)));
}

TEST(ConsoleJsonTest, SetInput)
{
	char buf[128];
	ASSERT_TRUE(ConsoleJson::BuildSetInput("say \"hi\"", false, buf, sizeof(buf)));
	EXPECT_STREQ("{\"text\":\"say \\\"hi\\\"\",\"restore\":false}", buf);
	ASSERT_TRUE(ConsoleJson::BuildSetInput("", true, buf, sizeof(buf)));
	EXPECT_STREQ("{\"text\":\"\",\"restore\":true}", buf);
	char tiny[10];
	EXPECT_FALSE(ConsoleJson::BuildSetInput("abc", false, tiny, sizeof(tiny)));
}

TEST(ConsoleJsonTest, CompletionRankNames)
{
	Completion items[2] = {};
	strcpy_s(items[0].name, "a"); items[0].rank = MatchRank::Exact;
	strcpy_s(items[1].name, "b"); items[1].rank = MatchRank::Contains;
	char buf[256];
	ASSERT_TRUE(ConsoleJson::BuildCompletions(items, 2, buf, sizeof(buf)));
	EXPECT_NE(nullptr, strstr(buf, "\"rank\":\"exact\""));
	EXPECT_NE(nullptr, strstr(buf, "\"rank\":\"contains\""));
}
