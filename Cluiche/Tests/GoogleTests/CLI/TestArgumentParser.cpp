////////////////////////////////////////////////////////////////////////////////
// Filename: TestArgumentParser.cpp
// Description: Unit tests for DiaAPI argument parser
// Feature spec: docs/specs/features/dia/diacli/cli-parser.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaAPI/DiaAPI.h>

using namespace Dia::API;


////////////////////////////////////////////////////////////////////////////////
// Test fixture
////////////////////////////////////////////////////////////////////////////////
class ArgumentParserTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Parser doesn't require Initialize() - it's self-contained
		// Initialize() is only needed for command registry
	}

	void TearDown() override
	{
		// No cleanup needed
	}

	// Helper: Create argv from string array
	template<size_t N>
	char** MakeArgv(const char* (&args)[N], int& argc)
	{
		argc = N;
		static char* argv[32];
		for (size_t i = 0; i < N; i++)
		{
			argv[i] = const_cast<char*>(args[i]);
		}
		return argv;
	}
};

////////////////////////////////////////////////////////////////////////////////
// AC1: Parse positional arguments
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParsePositionalArguments)
{
	const char* args[] = { "DiaAPI", "build", "foo.txt", "bar.txt" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_EQ(Dia::Core::StringCRC("build"), result.commandName);
	EXPECT_EQ(2u, result.args.positionalArgs.Size());
	EXPECT_STREQ("foo.txt", result.args.positionalArgs[0]);
	EXPECT_STREQ("bar.txt", result.args.positionalArgs[1]);
}

////////////////////////////////////////////////////////////////////////////////
// AC1: Parse with no arguments
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseNoArguments)
{
	const char* args[] = { "DiaAPI", "build" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_EQ(Dia::Core::StringCRC("build"), result.commandName);
	EXPECT_EQ(0u, result.args.positionalArgs.Size());
	EXPECT_EQ(0u, result.args.namedArgs.size());
	EXPECT_EQ(0u, result.args.flags.size());
}

////////////////////////////////////////////////////////////////////////////////
// AC2: Parse named arguments with --key=value format
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseNamedArguments)
{
	const char* args[] = { "DiaAPI", "build", "--format=gltf" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_EQ(Dia::Core::StringCRC("build"), result.commandName);
	EXPECT_TRUE(result.args.namedArgs.find(Dia::Core::StringCRC("format").Value()) != result.args.namedArgs.end());
	EXPECT_STREQ("gltf", result.args.namedArgs[Dia::Core::StringCRC("format").Value()]);
}

////////////////////////////////////////////////////////////////////////////////
// AC2: Parse multiple named arguments
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseMultipleNamedArguments)
{
	const char* args[] = { "DiaAPI", "build", "--key=value", "--other=123" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_EQ(2u, result.args.namedArgs.size());
	EXPECT_STREQ("value", result.args.namedArgs[Dia::Core::StringCRC("key").Value()]);
	EXPECT_STREQ("123", result.args.namedArgs[Dia::Core::StringCRC("other").Value()]);
}

////////////////////////////////////////////////////////////////////////////////
// AC3: Parse boolean flags with --flag format
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseFlags)
{
	const char* args[] = { "DiaAPI", "build", "--verbose" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_TRUE(result.args.flags.find(Dia::Core::StringCRC("verbose").Value()) != result.args.flags.end());
	EXPECT_TRUE(result.args.flags[Dia::Core::StringCRC("verbose").Value()]);
}

////////////////////////////////////////////////////////////////////////////////
// AC3: Parse multiple flags
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseMultipleFlags)
{
	const char* args[] = { "DiaAPI", "build", "--flag1", "--flag2" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_EQ(2u, result.args.flags.size());
	EXPECT_TRUE(result.args.flags.find(Dia::Core::StringCRC("flag1").Value()) != result.args.flags.end());
	EXPECT_TRUE(result.args.flags.find(Dia::Core::StringCRC("flag2").Value()) != result.args.flags.end());
}

////////////////////////////////////////////////////////////////////////////////
// AC4: Support short-form flags with single dash
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseShortFlags)
{
	const char* args[] = { "DiaAPI", "build", "-v" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_TRUE(result.args.flags.find(Dia::Core::StringCRC("verbose").Value()) != result.args.flags.end());
}

////////////////////////////////////////////////////////////////////////////////
// AC4: Parse multiple short flags
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseMultipleShortFlags)
{
	const char* args[] = { "DiaAPI", "build", "-v", "-h" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_TRUE(result.args.flags.find(Dia::Core::StringCRC("verbose").Value()) != result.args.flags.end());
	EXPECT_TRUE(result.args.flags.find(Dia::Core::StringCRC("help").Value()) != result.args.flags.end());
}

////////////////////////////////////////////////////////////////////////////////
// AC5: First argument after executable is command name
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ExtractCommandName)
{
	const char* args[] = { "DiaAPI", "build", "--flag" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_EQ(Dia::Core::StringCRC("build"), result.commandName);
	EXPECT_EQ(0u, result.args.positionalArgs.Size());
	EXPECT_EQ(1u, result.args.flags.size());
}

////////////////////////////////////////////////////////////////////////////////
// AC6: Handle quoted arguments (note: OS/shell handles quotes, argv already unquoted)
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseQuotedArguments)
{
	// Simulating shell behavior: quotes removed by shell, we receive unquoted string
	const char* args[] = { "DiaAPI", "build", "path with spaces" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_EQ(1u, result.args.positionalArgs.Size());
	EXPECT_STREQ("path with spaces", result.args.positionalArgs[0]);
}

////////////////////////////////////////////////////////////////////////////////
// AC7/AC9: --key without = is treated as a flag (not error)
// Parser cannot distinguish between --key (flag) vs --key (malformed named arg)
// Commands validate if flag makes sense for them
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, KeyWithoutEqualsIsTreatedAsFlag)
{
	const char* args[] = { "DiaAPI", "build", "--key" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_TRUE(result.args.flags.find(Dia::Core::StringCRC("key").Value()) != result.args.flags.end());
}

////////////////////////////////////////////////////////////////////////////////
// AC7: Return error for triple-dash
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ErrorTripleDash)
{
	const char* args[] = { "DiaAPI", "build", "---invalid" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(2, result.errorCode);
	EXPECT_NE(nullptr, result.errorMessage);
}

////////////////////////////////////////////////////////////////////////////////
// AC7: Return error for unknown short flag
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ErrorUnknownShortFlag)
{
	const char* args[] = { "DiaAPI", "build", "-x" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(2, result.errorCode);
	EXPECT_NE(nullptr, result.errorMessage);
}

////////////////////////////////////////////////////////////////////////////////
// AC7: Return error when no command specified
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ErrorNoCommand)
{
	const char* args[] = { "DiaAPI" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(2, result.errorCode);
	EXPECT_NE(nullptr, result.errorMessage);
}

////////////////////////////////////////////////////////////////////////////////
// AC9: Unknown/unrecognized flags are stored (no validation at parser level)
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseUnknownFlag)
{
	const char* args[] = { "DiaAPI", "build", "--unknown-flag" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_TRUE(result.args.flags.find(Dia::Core::StringCRC("unknown-flag").Value()) != result.args.flags.end());
}

////////////////////////////////////////////////////////////////////////////////
// AC10: Support -- to mark end of flags
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseEndOfFlagsMarker)
{
	const char* args[] = { "DiaAPI", "build", "--flag", "--", "--not-a-flag" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_TRUE(result.args.flags.find(Dia::Core::StringCRC("flag").Value()) != result.args.flags.end());
	EXPECT_EQ(1u, result.args.positionalArgs.Size());
	EXPECT_STREQ("--not-a-flag", result.args.positionalArgs[0]);
}

////////////////////////////////////////////////////////////////////////////////
// AC10: Everything after -- is positional (including --help)
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseEndOfFlagsWithHelp)
{
	const char* args[] = { "DiaAPI", "build", "--", "--help" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_FALSE(result.args.flags.find(Dia::Core::StringCRC("help").Value()) != result.args.flags.end());
	EXPECT_EQ(1u, result.args.positionalArgs.Size());
	EXPECT_STREQ("--help", result.args.positionalArgs[0]);
}

////////////////////////////////////////////////////////////////////////////////
// AC11: Handle empty values for named args
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseEmptyValue)
{
	const char* args[] = { "DiaAPI", "build", "--key=" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_TRUE(result.args.namedArgs.find(Dia::Core::StringCRC("key").Value()) != result.args.namedArgs.end());
	EXPECT_STREQ("", result.args.namedArgs[Dia::Core::StringCRC("key").Value()]);
}

////////////////////////////////////////////////////////////////////////////////
// Mixed arguments: positional + named + flags
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseMixedArguments)
{
	const char* args[] = { "DiaAPI", "build", "file.txt", "--format=gltf", "--verbose" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_EQ(1u, result.args.positionalArgs.Size());
	EXPECT_STREQ("file.txt", result.args.positionalArgs[0]);
	EXPECT_STREQ("gltf", result.args.namedArgs[Dia::Core::StringCRC("format").Value()]);
	EXPECT_TRUE(result.args.flags.find(Dia::Core::StringCRC("verbose").Value()) != result.args.flags.end());
}

////////////////////////////////////////////////////////////////////////////////
// Special case: --help with no command
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ParseHelpNoCommand)
{
	const char* args[] = { "DiaAPI", "--help" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_EQ(Dia::Core::StringCRC(""), result.commandName);
	EXPECT_TRUE(result.args.flags.find(Dia::Core::StringCRC("help").Value()) != result.args.flags.end());
}

////////////////////////////////////////////////////////////////////////////////
// Error: Command name cannot start with dash
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, ErrorCommandNameWithDash)
{
	const char* args[] = { "DiaAPI", "-invalid" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(2, result.errorCode);
	EXPECT_NE(nullptr, result.errorMessage);
}

////////////////////////////////////////////////////////////////////////////////
// RegisterShortFlag: Custom short flag registration
////////////////////////////////////////////////////////////////////////////////
TEST_F(ArgumentParserTest, RegisterCustomShortFlag)
{
	// Register custom short flag
	RegisterShortFlag("-f", "force");

	const char* args[] = { "DiaAPI", "build", "-f" };
	int argc;
	char** argv = MakeArgv(args, argc);

	ParseResult result;
	ParseArguments(argc, argv, result);

	EXPECT_EQ(0, result.errorCode);
	EXPECT_TRUE(result.args.flags.find(Dia::Core::StringCRC("force").Value()) != result.args.flags.end());
}
