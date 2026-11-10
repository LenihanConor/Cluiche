////////////////////////////////////////////////////////////////////////////////
// Filename: TestHelpSystemArguments.cpp
// Description: Tests for the "Arguments" section of command help
// Spec: docs/specs/applications/dia/systems/diavalueschema/code-side-descriptions.md (AC 8)
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaAPI/DiaAPI.h>
#include <DiaAPI/Help/HelpSystem.h>
#include <string>

using namespace Dia::API;

namespace
{
	std::string Format(const ValueParam* params, unsigned count)
	{
		char buf[2048];
		unsigned n = FormatArgumentsSection(params, count, buf, sizeof(buf));
		EXPECT_LT(n, sizeof(buf));
		return std::string(buf);
	}

	ValueParam P(const char* name, const char* type, bool required, const char* desc)
	{
		ValueParam p;
		p.name = name;
		p.type = type;
		p.required = required;
		p.description = desc;
		return p;
	}

	int DummyCb(const CommandArgs&) { return 0; }
}

TEST(HelpSystemArguments, SampleFromSpec)
{
	ValueParam ps[3];
	ps[0] = P("kind", "enum", true, "Which pack to save: weather or ecology.");
	ps[0].enumValues = "weather|ecology";
	ps[1] = P("id", "string", false, "Asset id for the new pack.");
	ps[2] = P("weeks", "int", false, "How long to run.");
	ps[2].unit = "weeks";
	ps[2].hasRange = true;
	ps[2].min = 1.0f;
	ps[2].max = 52.0f;

	const char* expected =
		"Arguments:\n"
		"  <kind>   enum    weather|ecology   Which pack to save: weather or ecology.\n"
		"  [id]     string                    Asset id for the new pack.\n"
		"  [weeks]  int     weeks, 1..52      How long to run.\n";
	EXPECT_EQ(std::string(expected), Format(ps, 3));
}

TEST(HelpSystemArguments, RequiredUsesAngleOptionalUsesSquare)
{
	ValueParam ps[2];
	ps[0] = P("a", "int", true, "A.");
	ps[1] = P("b", "int", false, "B.");
	EXPECT_EQ(std::string("Arguments:\n  <a>  int  A.\n  [b]  int  B.\n"), Format(ps, 2));
}

TEST(HelpSystemArguments, UnitOnly)
{
	ValueParam p = P("w", "int", false, "Wait.");
	p.unit = "weeks";
	EXPECT_EQ(std::string("Arguments:\n  [w]  int  weeks   Wait.\n"), Format(&p, 1));
}

TEST(HelpSystemArguments, RangeOnly)
{
	ValueParam p = P("w", "int", false, "Wait.");
	p.hasRange = true;
	p.min = 1.0f;
	p.max = 52.0f;
	EXPECT_EQ(std::string("Arguments:\n  [w]  int  1..52   Wait.\n"), Format(&p, 1));
}

TEST(HelpSystemArguments, UnitAndRange)
{
	ValueParam p = P("d", "float", true, "Dist.");
	p.unit = "m";
	p.hasRange = true;
	p.min = 0.0f;
	p.max = 10.0f;
	EXPECT_EQ(std::string("Arguments:\n  <d>  float  m, 0..10   Dist.\n"), Format(&p, 1));
}

TEST(HelpSystemArguments, EnumValuesShown)
{
	ValueParam p = P("k", "enum", true, "Kind.");
	p.enumValues = "a|b";
	EXPECT_EQ(std::string("Arguments:\n  <k>  enum  a|b   Kind.\n"), Format(&p, 1));
}

TEST(HelpSystemArguments, NumberFormattingIsGStyle)
{
	ValueParam p = P("r", "float", false, "R.");
	p.hasRange = true;
	p.min = 0.05f;
	p.max = 1.0f;
	EXPECT_EQ(std::string("Arguments:\n  [r]  float  0.05..1   R.\n"), Format(&p, 1));
}

TEST(HelpSystemArguments, NullDescriptionHasNoTrailingText)
{
	ValueParam p = P("a", "int", true, nullptr);
	EXPECT_EQ(std::string("Arguments:\n  <a>  int\n"), Format(&p, 1));

	ValueParam q = P("a", "int", true, nullptr);
	q.unit = "m";
	EXPECT_EQ(std::string("Arguments:\n  <a>  int  m\n"), Format(&q, 1));
}

TEST(HelpSystemArguments, AlignmentAcrossDifferentWidths)
{
	ValueParam ps[2];
	ps[0] = P("x", "int", true, "X.");
	ps[1] = P("longname", "string", false, "L.");
	EXPECT_EQ(std::string("Arguments:\n  <x>         int     X.\n  [longname]  string  L.\n"), Format(ps, 2));
}

TEST(HelpSystemArguments, NullNameOrTypeIsSkippedOrPlaceholder)
{
	ValueParam ps[2];
	ps[0] = P(nullptr, "int", true, "Bad.");
	ps[1] = P("ok", nullptr, true, "Ok.");
	std::string s = Format(ps, 2);
	EXPECT_NE(std::string::npos, s.find("<?>"));
	EXPECT_NE(std::string::npos, s.find("<ok>"));
	EXPECT_NE(std::string::npos, s.find("?"));
}

TEST(HelpSystemArguments, NoParamsProducesEmptyOutput)
{
	char buf[16] = "junk";
	EXPECT_EQ(0u, FormatArgumentsSection(nullptr, 0, buf, sizeof(buf)));
	EXPECT_STREQ("", buf);
	ValueParam p = P("a", "int", true, "A.");
	EXPECT_EQ(0u, FormatArgumentsSection(&p, 0, buf, sizeof(buf)));
	EXPECT_EQ(0u, FormatArgumentsSection(nullptr, 3, buf, sizeof(buf)));
}

TEST(HelpSystemArguments, TruncatesSafelyWhenBufferSmall)
{
	ValueParam p = P("a", "int", true, "A long description here.");
	char buf[8];
	unsigned n = FormatArgumentsSection(&p, 1, buf, sizeof(buf));
	EXPECT_GT(n, 7u); // reports full required length
	EXPECT_EQ(7u, strlen(buf));
}

class HelpSystemArgumentsCapture : public ::testing::Test
{
protected:
	void SetUp() override { if (!IsInitialized()) Initialize(); }
	void TearDown() override { if (IsInitialized()) Shutdown(); }

	static CommandInfo Make(const char* name)
	{
		CommandInfo cmd;
		cmd.name = Dia::Core::StringCRC(name);
		cmd.description = "Desc";
		cmd.category = Dia::Core::StringCRC("cat");
		cmd.owner = "Owner";
		cmd.version = "1.0.0";
		cmd.example = "ex --a";
		cmd.callback = DummyCb;
		return cmd;
	}
};

TEST_F(HelpSystemArgumentsCapture, NoParamsOutputUnchangedGolden)
{
	CommandInfo cmd = Make("gold-cmd");
	RegisterCommand(cmd);

	testing::internal::CaptureStdout();
	ShowCommandHelp(Dia::Core::StringCRC("gold-cmd"));
	std::string out = testing::internal::GetCapturedStdout();

	const char* expected =
		"DiaAPI gold-cmd - Desc\n\n"
		"Category: cat\n"
		"Owner:    Owner\n"
		"Version:  1.0.0\n\n"
		"Usage:\n  ex --a\n\n"
		"Description:\n  Desc\n";
	EXPECT_EQ(std::string(expected), out);
}

TEST_F(HelpSystemArgumentsCapture, NoArgsCommandPrintsNothingNew)
{
	CommandInfo cmd = Make("noargs-cmd");
	cmd.noArgs = true;
	RegisterCommand(cmd);

	testing::internal::CaptureStdout();
	ShowCommandHelp(Dia::Core::StringCRC("noargs-cmd"));
	std::string out = testing::internal::GetCapturedStdout();
	EXPECT_EQ(std::string::npos, out.find("Arguments:"));
}

TEST_F(HelpSystemArgumentsCapture, CommandWithParamsAppendsArgumentsSection)
{
	static const ValueParam kParams[] = { P("id", "string", true, "The id.") };
	CommandInfo cmd = Make("args-cmd");
	cmd.params = kParams;
	cmd.paramCount = 1;
	RegisterCommand(cmd);

	testing::internal::CaptureStdout();
	ShowCommandHelp(Dia::Core::StringCRC("args-cmd"));
	std::string out = testing::internal::GetCapturedStdout();

	const char* tail = "Description:\n  Desc\n\nArguments:\n  <id>  string  The id.\n";
	EXPECT_NE(std::string::npos, out.find(tail));
}
