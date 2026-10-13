////////////////////////////////////////////////////////////////////////////////
// Filename: TestCommandLineParser.cpp
// Description: Unit tests for the console CommandLineParser and ConsoleLineAssembler.
//              Covers AC-1, AC-2 and AC-16 (assembler).
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaVisualDebugger/Console/CommandLineParser.h>
#include <DiaVisualDebugger/Console/ConsoleLineAssembler.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstring>
#include <string>

using namespace Dia::VisualDebugger::Console;
using Dia::Core::StringCRC;

namespace
{
	unsigned int Crc(const char* s) { return StringCRC(s).Value(); }
}

////////////////////////////////////////////////////////////////////////////////
// CommandLineParser
////////////////////////////////////////////////////////////////////////////////

TEST(ConsoleCommandLineParser, AC1_NamePositionalQuotedFlagAndNamed)
{
	ParsedCommand cmd;
	ASSERT_EQ(ParseCommandLine("foo a \"b c\" --v key=val", cmd), ParseError::None);

	EXPECT_EQ(cmd.name, StringCRC("foo"));
	ASSERT_EQ(cmd.args.positionalArgs.Size(), 2u);
	EXPECT_STREQ(cmd.args.positionalArgs[0], "a");
	EXPECT_STREQ(cmd.args.positionalArgs[1], "b c");
	EXPECT_TRUE(cmd.args.HasFlag(Crc("v")));
	ASSERT_NE(cmd.args.GetNamedArg(Crc("key")), nullptr);
	EXPECT_STREQ(cmd.args.GetNamedArg(Crc("key")), "val");
}

TEST(ConsoleCommandLineParser, NameOnly)
{
	ParsedCommand cmd;
	ASSERT_EQ(ParseCommandLine("debug.layer.list", cmd), ParseError::None);
	EXPECT_EQ(cmd.name, StringCRC("debug.layer.list"));
	EXPECT_EQ(cmd.args.positionalArgs.Size(), 0u);
	EXPECT_EQ(cmd.args.flags.Size(), 0u);
	EXPECT_EQ(cmd.args.namedArgs.Size(), 0u);
}

TEST(ConsoleCommandLineParser, CliStyleNamedArgAndShortFlag)
{
	ParsedCommand cmd;
	ASSERT_EQ(ParseCommandLine("foo --level=3 -v", cmd), ParseError::None);
	ASSERT_NE(cmd.args.GetNamedArg(Crc("level")), nullptr);
	EXPECT_STREQ(cmd.args.GetNamedArg(Crc("level")), "3");
	EXPECT_TRUE(cmd.args.HasFlag(Crc("verbose")));
}

TEST(ConsoleCommandLineParser, LeadingTrailingAndRepeatedSpaces)
{
	ParsedCommand cmd;
	ASSERT_EQ(ParseCommandLine("   foo    a   b   ", cmd), ParseError::None);
	EXPECT_EQ(cmd.name, StringCRC("foo"));
	ASSERT_EQ(cmd.args.positionalArgs.Size(), 2u);
	EXPECT_STREQ(cmd.args.positionalArgs[0], "a");
	EXPECT_STREQ(cmd.args.positionalArgs[1], "b");
}

TEST(ConsoleCommandLineParser, QuotedKeyValueStaysPositional)
{
	ParsedCommand cmd;
	ASSERT_EQ(ParseCommandLine("foo \"a=b\"", cmd), ParseError::None);
	ASSERT_EQ(cmd.args.positionalArgs.Size(), 1u);
	EXPECT_STREQ(cmd.args.positionalArgs[0], "a=b");
	EXPECT_EQ(cmd.args.namedArgs.Size(), 0u);
}

TEST(ConsoleCommandLineParser, QuoteInsideTokenGroupsSpaces)
{
	ParsedCommand cmd;
	ASSERT_EQ(ParseCommandLine("foo path=\"C:/My Dir/x.txt\"", cmd), ParseError::None);
	ASSERT_EQ(cmd.args.positionalArgs.Size(), 1u);
	EXPECT_STREQ(cmd.args.positionalArgs[0], "path=C:/My Dir/x.txt");
}

TEST(ConsoleCommandLineParser, EmptyQuotedPositional)
{
	ParsedCommand cmd;
	ASSERT_EQ(ParseCommandLine("foo \"\"", cmd), ParseError::None);
	ASSERT_EQ(cmd.args.positionalArgs.Size(), 1u);
	EXPECT_STREQ(cmd.args.positionalArgs[0], "");
}

TEST(ConsoleCommandLineParser, DoubleDashEndsFlagsAndNamedShorthand)
{
	ParsedCommand cmd;
	ASSERT_EQ(ParseCommandLine("foo -- --x k=v", cmd), ParseError::None);
	ASSERT_EQ(cmd.args.positionalArgs.Size(), 2u);
	EXPECT_STREQ(cmd.args.positionalArgs[0], "--x");
	EXPECT_STREQ(cmd.args.positionalArgs[1], "k=v");
	EXPECT_EQ(cmd.args.flags.Size(), 0u);
	EXPECT_EQ(cmd.args.namedArgs.Size(), 0u);
}

TEST(ConsoleCommandLineParser, AC2_Empty)
{
	ParsedCommand cmd;
	EXPECT_EQ(ParseCommandLine("", cmd), ParseError::Empty);
	EXPECT_EQ(ParseCommandLine(nullptr, cmd), ParseError::Empty);
}

TEST(ConsoleCommandLineParser, AC2_WhitespaceOnly)
{
	ParsedCommand cmd;
	EXPECT_EQ(ParseCommandLine("     \t  ", cmd), ParseError::Empty);
}

TEST(ConsoleCommandLineParser, AC2_EmptyQuotedName)
{
	ParsedCommand cmd;
	EXPECT_EQ(ParseCommandLine("\"\" a", cmd), ParseError::Empty);
}

TEST(ConsoleCommandLineParser, AC2_UnterminatedQuote)
{
	ParsedCommand cmd;
	EXPECT_EQ(ParseCommandLine("foo \"b c", cmd), ParseError::UnterminatedQuote);
	EXPECT_EQ(ParseCommandLine("\"", cmd), ParseError::UnterminatedQuote);
}

TEST(ConsoleCommandLineParser, AC2_TooLong)
{
	ParsedCommand cmd;
	std::string text(241, 'a');
	EXPECT_EQ(ParseCommandLine(text.c_str(), cmd), ParseError::TooLong);
}

TEST(ConsoleCommandLineParser, ExactlyMaxLengthAccepted)
{
	ParsedCommand cmd;
	std::string text = "foo " + std::string(kMaxCommandLineLength - 4, 'x');
	ASSERT_EQ(text.size(), static_cast<size_t>(kMaxCommandLineLength));
	ASSERT_EQ(ParseCommandLine(text.c_str(), cmd), ParseError::None);
	ASSERT_EQ(cmd.args.positionalArgs.Size(), 1u);
	EXPECT_EQ(std::strlen(cmd.args.positionalArgs[0]), static_cast<size_t>(kMaxCommandLineLength - 4));
}

TEST(ConsoleCommandLineParser, TooManyArgs)
{
	ParsedCommand cmd;
	std::string ok = "foo";
	for (unsigned int i = 0; i < kMaxCommandLineTokens; ++i)
	{
		ok += " a";
	}
	EXPECT_EQ(ParseCommandLine(ok.c_str(), cmd), ParseError::None);
	EXPECT_EQ(cmd.args.positionalArgs.Size(), kMaxCommandLineTokens);

	std::string tooMany = ok + " a";
	EXPECT_EQ(ParseCommandLine(tooMany.c_str(), cmd), ParseError::TooManyArgs);
}

TEST(ConsoleCommandLineParser, ManyNamedShorthandFitsStorage)
{
	// Every token gains a "--" prefix; worst case must still fit.
	ParsedCommand cmd;
	std::string text = "f";
	for (unsigned int i = 0; i < kMaxCommandLineTokens; ++i)
	{
		text += " a=b";
	}
	ASSERT_LE(text.size(), static_cast<size_t>(kMaxCommandLineLength));
	EXPECT_EQ(ParseCommandLine(text.c_str(), cmd), ParseError::None);
}

TEST(ConsoleCommandLineParser, InvalidArgumentFromArgumentParser)
{
	ParsedCommand cmd;
	EXPECT_EQ(ParseCommandLine("foo ---x", cmd), ParseError::InvalidArgument);
	EXPECT_EQ(ParseCommandLine("-foo", cmd), ParseError::InvalidArgument);
}

TEST(ConsoleCommandLineParser, ResetsOutputBetweenCalls)
{
	ParsedCommand cmd;
	ASSERT_EQ(ParseCommandLine("foo a b --f", cmd), ParseError::None);
	ASSERT_EQ(ParseCommandLine("bar", cmd), ParseError::None);
	EXPECT_EQ(cmd.name, StringCRC("bar"));
	EXPECT_EQ(cmd.args.positionalArgs.Size(), 0u);
	EXPECT_EQ(cmd.args.flags.Size(), 0u);

	EXPECT_EQ(ParseCommandLine("", cmd), ParseError::Empty);
	EXPECT_EQ(cmd.args.positionalArgs.Size(), 0u);
}

TEST(ConsoleCommandLineParser, NeverCrashesOnOddInput)
{
	const char* inputs[] = {
		"\"\"\"\"", "=", "==", "a=", "foo =", "foo =x", "foo -", "foo --", "foo --=", "foo --=x",
		"\t\r\n", "foo \"\" \"\" \"\"", "foo \"a\"\"b\"", "foo a\"", "\"foo bar\" baz",
	};
	for (const char* in : inputs)
	{
		ParsedCommand cmd;
		ParseError e = ParseCommandLine(in, cmd);
		(void)e;
	}

	// Every byte value as a one-char token after a name.
	for (int c = 1; c < 256; ++c)
	{
		char buf[8] = { 'f', ' ', static_cast<char>(c), '\0' };
		ParsedCommand cmd;
		ParseError e = ParseCommandLine(buf, cmd);
		(void)e;
	}
	SUCCEED();
}

////////////////////////////////////////////////////////////////////////////////
// ConsoleLineAssembler (AC-16)
////////////////////////////////////////////////////////////////////////////////

TEST(ConsoleLineAssembler, OneChunk)
{
	const char* chunks[] = { "debug.layer.list" };
	char out[256];
	ASSERT_TRUE(ConsoleLineAssembler::Assemble(chunks, 1, out, sizeof(out)));
	EXPECT_STREQ(out, "debug.layer.list");
}

TEST(ConsoleLineAssembler, TwoThreeFourChunks)
{
	const char* chunks[] = { "ab", "cd", "ef", "gh" };
	char out[256];
	ASSERT_TRUE(ConsoleLineAssembler::Assemble(chunks, 2, out, sizeof(out)));
	EXPECT_STREQ(out, "abcd");
	ASSERT_TRUE(ConsoleLineAssembler::Assemble(chunks, 3, out, sizeof(out)));
	EXPECT_STREQ(out, "abcdef");
	ASSERT_TRUE(ConsoleLineAssembler::Assemble(chunks, 4, out, sizeof(out)));
	EXPECT_STREQ(out, "abcdefgh");
}

TEST(ConsoleLineAssembler, EmptyAndNullChunks)
{
	const char* chunks[] = { "", "abc", nullptr, "" };
	char out[256];
	ASSERT_TRUE(ConsoleLineAssembler::Assemble(chunks, 4, out, sizeof(out)));
	EXPECT_STREQ(out, "abc");

	ASSERT_TRUE(ConsoleLineAssembler::Assemble(chunks, 0, out, sizeof(out)));
	EXPECT_STREQ(out, "");
}

TEST(ConsoleLineAssembler, AC16_TwoHundredCharsUnchanged)
{
	std::string line;
	for (int i = 0; i < 200; ++i)
	{
		line += static_cast<char>('a' + (i % 26));
	}
	std::string parts[4] = { line.substr(0, 60), line.substr(60, 60), line.substr(120, 60), line.substr(180) };
	const char* chunks[] = { parts[0].c_str(), parts[1].c_str(), parts[2].c_str(), parts[3].c_str() };
	char out[256];
	ASSERT_TRUE(ConsoleLineAssembler::Assemble(chunks, 4, out, sizeof(out)));
	EXPECT_EQ(std::string(out), line);
}

TEST(ConsoleLineAssembler, MaxLineAccepted)
{
	std::string a(60, 'x');
	const char* chunks[] = { a.c_str(), a.c_str(), a.c_str(), a.c_str() };
	char out[256];
	ASSERT_TRUE(ConsoleLineAssembler::Assemble(chunks, 4, out, sizeof(out)));
	EXPECT_EQ(std::strlen(out), 240u);
}

TEST(ConsoleLineAssembler, OverflowLineRejected)
{
	std::string a(61, 'x');	// 4 x 61 = 244 > 240
	const char* chunks[] = { a.c_str(), a.c_str(), a.c_str(), a.c_str() };
	char out[256];
	EXPECT_FALSE(ConsoleLineAssembler::Assemble(chunks, 4, out, sizeof(out)));
	EXPECT_STREQ(out, "");
}

TEST(ConsoleLineAssembler, ChunkTooLongRejected)
{
	std::string ok(63, 'x');
	std::string tooLong(64, 'x');
	const char* good[] = { ok.c_str() };
	const char* bad[] = { tooLong.c_str() };
	char out[256];
	EXPECT_TRUE(ConsoleLineAssembler::Assemble(good, 1, out, sizeof(out)));
	EXPECT_FALSE(ConsoleLineAssembler::Assemble(bad, 1, out, sizeof(out)));
	EXPECT_STREQ(out, "");
}

TEST(ConsoleLineAssembler, TooManyChunksRejected)
{
	const char* chunks[] = { "a", "b", "c", "d", "e" };
	char out[256];
	EXPECT_FALSE(ConsoleLineAssembler::Assemble(chunks, 5, out, sizeof(out)));
}

TEST(ConsoleLineAssembler, SmallOutputBufferRejected)
{
	const char* chunks[] = { "abcdef" };
	char out[4];
	EXPECT_FALSE(ConsoleLineAssembler::Assemble(chunks, 1, out, sizeof(out)));
	EXPECT_STREQ(out, "");
	EXPECT_FALSE(ConsoleLineAssembler::Assemble(chunks, 1, nullptr, 0));
}
