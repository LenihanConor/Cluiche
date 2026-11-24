////////////////////////////////////////////////////////////////////////////////
// Filename: TestHelpSystemArgumentsEdge.cpp
// Description: Edge, property and capture tests for FormatArgumentsSection and
//              the "Arguments" part of ShowCommandHelp.
// Spec: docs/specs/applications/dia/systems/diavalueschema/code-side-descriptions.md (AC 8)
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaAPI/DiaAPI.h>
#include <DiaAPI/Help/HelpSystem.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

using namespace Dia::API;

namespace
{
	ValueParam P(const char* name, const char* type, bool required, const char* desc)
	{
		ValueParam p;
		p.name = name;
		p.type = type;
		p.required = required;
		p.description = desc;
		return p;
	}

	// Two-call pattern: length with out=nullptr, then fill an exactly-sized buffer.
	std::string Full(const ValueParam* params, unsigned count)
	{
		unsigned need = FormatArgumentsSection(params, count, nullptr, 0);
		std::string s(need + 1, '\xAA');
		unsigned got = FormatArgumentsSection(params, count, &s[0], need + 1);
		EXPECT_EQ(need, got);
		EXPECT_EQ('\0', s[need]);
		s.resize(need);
		return s;
	}

	bool NoLineEndsInSpace(const std::string& s)
	{
		for (size_t i = 0; i < s.size(); i++)
		{
			if (s[i] == '\n' && i > 0 && s[i - 1] == ' ')
				return false;
		}
		return s.empty() || s.back() != ' ';
	}

	std::string Spaces(size_t n) { return std::string(n, ' '); }

	int DummyCb(const CommandArgs&) { return 0; }

	std::string G(float mn, float mx)
	{
		char b[128];
		snprintf(b, sizeof(b), "%g..%g", (double)mn, (double)mx);
		return b;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Row mixing: detail / no-detail / description combinations
////////////////////////////////////////////////////////////////////////////////
TEST(HelpSystemArgumentsEdge, NoDetailNoDescRowHasNoTrailingSpaces)
{
	ValueParam ps[2];
	ps[0] = P("a", "int", true, nullptr);
	ps[0].unit = "m";
	ps[1] = P("b", "int", false, nullptr);
	EXPECT_EQ(std::string("Arguments:\n  <a>  int  m\n  [b]  int\n"), Full(ps, 2));
}

TEST(HelpSystemArgumentsEdge, NoDetailRowWithDescAlignsToDetailColumn)
{
	ValueParam ps[2];
	ps[0] = P("a", "int", true, "A");
	ps[0].unit = "m";
	ps[1] = P("b", "int", false, "B");
	std::string expected = "Arguments:\n  <a>  int  m   A\n  [b]  int" + Spaces(2 + 1 + 3) + "B\n";
	EXPECT_EQ(expected, Full(ps, 2));
}

TEST(HelpSystemArgumentsEdge, DetailNoDescRowAmongDescRows)
{
	ValueParam ps[2];
	ps[0] = P("a", "int", true, nullptr);
	ps[0].unit = "weeks";
	ps[1] = P("b", "int", false, "B");
	ps[1].unit = "m";
	// Row a: detail but no desc -> no padding after the detail.
	EXPECT_EQ(std::string("Arguments:\n  <a>  int  weeks\n  [b]  int  m       B\n"), Full(ps, 2));
}

TEST(HelpSystemArgumentsEdge, TypeOnlyRowsPadOnlyBetweenColumns)
{
	ValueParam ps[2];
	ps[0] = P("a", "int", true, nullptr);
	ps[1] = P("bb", "string", false, nullptr);
	EXPECT_EQ(std::string("Arguments:\n  <a>   int\n  [bb]  string\n"), Full(ps, 2));
}

TEST(HelpSystemArgumentsEdge, EmptyTypeRowWithNothingAfterHasNoTrailingSpaces)
{
	ValueParam ps[2];
	ps[0] = P("a", "", true, nullptr);
	ps[1] = P("b", "int", true, "B");
	std::string s = Full(ps, 2);
	EXPECT_TRUE(NoLineEndsInSpace(s)) << s;
	EXPECT_EQ(std::string("Arguments:\n  <a>\n  <b>  int  B\n"), s);
}

TEST(HelpSystemArgumentsEdge, EmptyTypeRowWithDescKeepsAlignment)
{
	ValueParam ps[2];
	ps[0] = P("a", "", true, "A");
	ps[1] = P("b", "int", true, "B");
	EXPECT_EQ(std::string("Arguments:\n  <a>       A\n  <b>  int  B\n").size(), Full(ps, 2).size());
	EXPECT_EQ(std::string("Arguments:\n  <a>       A\n  <b>  int  B\n"), Full(ps, 2));
}

// PIN: when every param has an empty type the type column has zero width and the
// rest of the row (detail, description) is dropped entirely.
TEST(HelpSystemArgumentsEdge, PinAllTypesEmptyDropsDescriptionAndDetail)
{
	ValueParam p = P("a", "", true, "Desc");
	p.unit = "m";
	EXPECT_EQ(std::string("Arguments:\n  <a>\n"), Full(&p, 1));
}

TEST(HelpSystemArgumentsEdge, AllNamesEmptyPrintsBrackets)
{
	ValueParam ps[2];
	ps[0] = P("", "int", true, "A");
	ps[1] = P("", "int", false, "B");
	EXPECT_EQ(std::string("Arguments:\n  <>  int  A\n  []  int  B\n"), Full(ps, 2));
}

TEST(HelpSystemArgumentsEdge, NullNameAndTypePrintQuestionMark)
{
	ValueParam ps[1];
	ps[0] = P(nullptr, nullptr, false, "D");
	EXPECT_EQ(std::string("Arguments:\n  [?]  ?  D\n"), Full(ps, 1));
}

TEST(HelpSystemArgumentsEdge, EmptyDescriptionEqualsNullDescription)
{
	ValueParam a = P("a", "int", true, "");
	ValueParam b = P("a", "int", true, nullptr);
	EXPECT_EQ(Full(&b, 1), Full(&a, 1));
	EXPECT_EQ(std::string("Arguments:\n  <a>  int\n"), Full(&a, 1));
}

////////////////////////////////////////////////////////////////////////////////
// Buffer contract
////////////////////////////////////////////////////////////////////////////////
TEST(HelpSystemArgumentsEdge, CapacitySweepNeverWritesPastCapacity)
{
	ValueParam ps[3];
	ps[0] = P("kind", "enum", true, "Which pack.");
	ps[0].enumValues = "weather|ecology";
	ps[1] = P("id", "string", false, nullptr);
	ps[2] = P("weeks", "int", false, "How long.");
	ps[2].unit = "weeks";
	ps[2].hasRange = true;
	ps[2].min = 1.0f;
	ps[2].max = 52.0f;

	const std::string full = Full(ps, 3);
	const unsigned need = (unsigned)full.size();
	ASSERT_GT(need, 20u);

	for (unsigned cap = 0; cap <= need + 8; cap++)
	{
		const unsigned kPad = 16;
		std::vector<unsigned char> buf(cap + kPad, 0xCD);
		unsigned ret = FormatArgumentsSection(ps, 3, (char*)buf.data(), cap);
		ASSERT_EQ(need, ret) << "cap=" << cap;

		for (unsigned i = cap; i < cap + kPad; i++)
			ASSERT_EQ(0xCD, buf[i]) << "canary overwritten at " << i << " cap=" << cap;

		if (cap == 0)
		{
			EXPECT_EQ(0xCD, buf[0]) << "capacity 0 must not touch the buffer";
			continue;
		}
		// NUL-terminated within capacity, content is the prefix of the full text.
		const unsigned expectedLen = need < cap - 1 ? need : cap - 1;
		ASSERT_EQ('\0', buf[expectedLen]) << "cap=" << cap;
		EXPECT_EQ(full.substr(0, expectedLen), std::string((const char*)buf.data())) << "cap=" << cap;
	}
}

TEST(HelpSystemArgumentsEdge, NullOutReturnsNeededLength)
{
	ValueParam p = P("a", "int", true, "Desc");
	const std::string full = Full(&p, 1);
	EXPECT_EQ((unsigned)full.size(), FormatArgumentsSection(&p, 1, nullptr, 0));
	EXPECT_EQ((unsigned)full.size(), FormatArgumentsSection(&p, 1, nullptr, 64));
	EXPECT_EQ((unsigned)full.size(), FormatArgumentsSection(&p, 1, nullptr, 1));
}

TEST(HelpSystemArgumentsEdge, CapacityZeroLeavesBufferUntouched)
{
	ValueParam p = P("a", "int", true, "Desc");
	char buf[4] = { 'x', 'y', 'z', 'w' };
	unsigned n = FormatArgumentsSection(&p, 1, buf, 0);
	EXPECT_GT(n, 0u);
	EXPECT_EQ('x', buf[0]);
	EXPECT_EQ('y', buf[1]);
	EXPECT_EQ('z', buf[2]);
	EXPECT_EQ('w', buf[3]);
}

TEST(HelpSystemArgumentsEdge, CapacityOneWritesOnlyNul)
{
	ValueParam p = P("a", "int", true, "Desc");
	char buf[4] = { 'x', 'y', 'z', 'w' };
	unsigned n = FormatArgumentsSection(&p, 1, buf, 1);
	EXPECT_EQ(Full(&p, 1).size(), (size_t)n);
	EXPECT_EQ('\0', buf[0]);
	EXPECT_EQ('y', buf[1]);
}

TEST(HelpSystemArgumentsEdge, ZeroCountAndNullParamsWriteEmptyString)
{
	ValueParam p = P("a", "int", true, "Desc");
	char buf[8] = "junk";
	EXPECT_EQ(0u, FormatArgumentsSection(&p, 0, buf, sizeof(buf)));
	EXPECT_STREQ("", buf);
	std::strcpy(buf, "junk");
	EXPECT_EQ(0u, FormatArgumentsSection(nullptr, 5, buf, sizeof(buf)));
	EXPECT_STREQ("", buf);
	EXPECT_EQ(0u, FormatArgumentsSection(nullptr, 5, nullptr, 0));
	// Capacity 0 with a null-params call must not touch the buffer either.
	std::strcpy(buf, "junk");
	EXPECT_EQ(0u, FormatArgumentsSection(nullptr, 0, buf, 0));
	EXPECT_STREQ("junk", buf);
}

////////////////////////////////////////////////////////////////////////////////
// Detail column
////////////////////////////////////////////////////////////////////////////////
namespace
{
	std::string Pattern(size_t n)
	{
		std::string s;
		for (size_t i = 0; i < n; i++) s += (char)('a' + (i % 26));
		return s;
	}
}

TEST(HelpSystemArgumentsEdge, DetailLengthCapAt255)
{
	const size_t lengths[] = { 254, 255, 256, 257, 300, 1000 };
	for (size_t len : lengths)
	{
		const std::string values = Pattern(len);
		ValueParam p = P("a", "enum", true, "D");
		p.enumValues = values.c_str();
		const std::string shown = values.substr(0, len < 255 ? len : 255);
		EXPECT_EQ("Arguments:\n  <a>  enum  " + shown + "   D\n", Full(&p, 1)) << "len=" << len;
	}
}

TEST(HelpSystemArgumentsEdge, CappedDetailWidthMeasureMatchesEmission)
{
	const std::string values = Pattern(300);
	ValueParam ps[2];
	ps[0] = P("a", "enum", true, "D");
	ps[0].enumValues = values.c_str();
	ps[1] = P("b", "enum", true, "E");
	ps[1].enumValues = "x";
	const std::string expected =
		"Arguments:\n  <a>  enum  " + values.substr(0, 255) + "   D\n"
		"  <b>  enum  x" + Spaces(255 - 1 + 3) + "E\n";
	EXPECT_EQ(expected, Full(ps, 2));
}

TEST(HelpSystemArgumentsEdge, EnumEmptyStringFallsBackToUnitAndRange)
{
	ValueParam p = P("a", "enum", true, "D");
	p.enumValues = "";
	p.unit = "m";
	EXPECT_EQ(std::string("Arguments:\n  <a>  enum  m   D\n"), Full(&p, 1));
}

TEST(HelpSystemArgumentsEdge, EnumValuesShownVerbatim)
{
	const char* cases[] = { "a||b", "|a", "a|", "|", "||", "a b|c d", "caf\xC3\xA9|\xE2\x82\xAC" };
	for (const char* v : cases)
	{
		ValueParam p = P("a", "enum", true, "D");
		p.enumValues = v;
		EXPECT_EQ(std::string("Arguments:\n  <a>  enum  ") + v + "   D\n", Full(&p, 1)) << v;
	}
}

TEST(HelpSystemArgumentsEdge, EnumTypeWithNullEnumValuesHasNoDetail)
{
	ValueParam p = P("a", "enum", true, "D");
	EXPECT_EQ(std::string("Arguments:\n  <a>  enum  D\n"), Full(&p, 1));
}

// PIN: the "enum" type check is case-sensitive, so "ENUM" ignores enumValues and
// shows the unit/range detail (or none) instead.
TEST(HelpSystemArgumentsEdge, PinUpperCaseEnumIgnoresEnumValues)
{
	ValueParam p = P("a", "ENUM", true, "D");
	p.enumValues = "x|y";
	EXPECT_EQ(std::string("Arguments:\n  <a>  ENUM  D\n"), Full(&p, 1));
}

TEST(HelpSystemArgumentsEdge, NonEnumTypeIgnoresEnumValues)
{
	ValueParam p = P("a", "string", true, "D");
	p.enumValues = "x|y";
	EXPECT_EQ(std::string("Arguments:\n  <a>  string  D\n"), Full(&p, 1));
}

// PIN: for an enum with values, unit and range are not displayed.
TEST(HelpSystemArgumentsEdge, PinEnumValuesTakePriorityOverUnitAndRange)
{
	ValueParam p = P("a", "enum", true, "D");
	p.enumValues = "x|y";
	p.unit = "m";
	p.hasRange = true;
	p.min = 1.0f;
	p.max = 2.0f;
	EXPECT_EQ(std::string("Arguments:\n  <a>  enum  x|y   D\n"), Full(&p, 1));
}

// PIN: alignment pads by byte count, so multi-byte UTF-8 text misaligns visually.
TEST(HelpSystemArgumentsEdge, PinUtf8AlignmentIsByteBased)
{
	ValueParam ps[2];
	ps[0] = P("caf\xC3\xA9", "int", true, "A");
	ps[1] = P("cafe", "int", true, "B");
	EXPECT_EQ(std::string("Arguments:\n  <caf\xC3\xA9>  int  A\n  <cafe>   int  B\n"), Full(ps, 2));
}

////////////////////////////////////////////////////////////////////////////////
// Range formatting
////////////////////////////////////////////////////////////////////////////////
TEST(HelpSystemArgumentsEdge, RangeEdgeValuesMatchGFormat)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();
	const float denorm = std::numeric_limits<float>::denorm_min();
	const float pairs[][2] = {
		{ 5.0f, 1.0f },            // min > max
		{ nan, nan },
		{ nan, 1.0f },
		{ 0.0f, nan },
		{ -inf, inf },
		{ inf, -inf },
		{ -0.0f, 0.0f },
		{ 0.0f, -0.0f },
		{ 1e-7f, 1e-7f },
		{ 0.0f, 1e10f },
		{ 0.1f + 0.2f, 0.1f + 0.2f },
		{ -FLT_MAX, FLT_MAX },
		{ denorm, -denorm },
		{ FLT_MIN, FLT_EPSILON },
		{ 123456789.0f, -123456789.0f },
		{ 0.5f, 0.25f },
	};
	for (const auto& pr : pairs)
	{
		ValueParam p = P("a", "float", true, "D");
		p.hasRange = true;
		p.min = pr[0];
		p.max = pr[1];
		EXPECT_EQ("Arguments:\n  <a>  float  " + G(pr[0], pr[1]) + "   D\n", Full(&p, 1)) << G(pr[0], pr[1]);
	}
}

TEST(HelpSystemArgumentsEdge, RangeLiteralSamples)
{
	struct Case { float mn, mx; const char* text; };
	const Case cases[] = {
		{ 5.0f, 1.0f, "5..1" },
		{ -0.0f, 0.0f, "-0..0" },
		{ 0.0f, 1e10f, "0..1e+10" },
		{ 0.1f + 0.2f, 0.1f + 0.2f, "0.3..0.3" },
		{ -FLT_MAX, FLT_MAX, "-3.40282e+38..3.40282e+38" },
		{ -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), "-inf..inf" },
	};
	for (const Case& c : cases)
	{
		ValueParam p = P("a", "float", true, nullptr);
		p.hasRange = true;
		p.min = c.mn;
		p.max = c.mx;
		EXPECT_EQ(std::string("Arguments:\n  <a>  float  ") + c.text + "\n", Full(&p, 1)) << c.text;
	}
}

TEST(HelpSystemArgumentsEdge, RangeIgnoredWhenHasRangeFalse)
{
	ValueParam p = P("a", "float", true, nullptr);
	p.min = 3.0f;
	p.max = 9.0f;
	EXPECT_EQ(std::string("Arguments:\n  <a>  float\n"), Full(&p, 1));
}

TEST(HelpSystemArgumentsEdge, EmptyUnitIsIgnored)
{
	ValueParam p = P("a", "float", true, nullptr);
	p.unit = "";
	EXPECT_EQ(std::string("Arguments:\n  <a>  float\n"), Full(&p, 1));
	p.hasRange = true;
	p.min = 1.0f;
	p.max = 2.0f;
	EXPECT_EQ(std::string("Arguments:\n  <a>  float  1..2\n"), Full(&p, 1));
}

////////////////////////////////////////////////////////////////////////////////
// Verbatim text
////////////////////////////////////////////////////////////////////////////////
TEST(HelpSystemArgumentsEdge, DescriptionNewlineAndFormatTokensAreVerbatim)
{
	ValueParam p = P("a", "int", true, "line1\nline2 %s %d %n %% %x");
	EXPECT_EQ(std::string("Arguments:\n  <a>  int  line1\nline2 %s %d %n %% %x\n"), Full(&p, 1));
}

TEST(HelpSystemArgumentsEdge, FormatTokensInNameTypeUnitEnumAreVerbatim)
{
	ValueParam p = P("%s%d", "%n", false, "D");
	p.enumValues = "%s|%d";
	// type is not "enum", so enumValues are ignored; unit carries a token instead.
	p.unit = "%s";
	EXPECT_EQ(std::string("Arguments:\n  [%s%d]  %n  %s   D\n"), Full(&p, 1));
}

TEST(HelpSystemArgumentsEdge, ManyRowsKeepRowOrder)
{
	std::vector<ValueParam> ps;
	for (int i = 0; i < 20; i++)
		ps.push_back(P(i % 2 ? "odd" : "even", "int", i % 3 == 0, "d"));
	std::string s = Full(ps.data(), (unsigned)ps.size());
	size_t lines = 0;
	for (char c : s) if (c == '\n') lines++;
	EXPECT_EQ(21u, lines);
	EXPECT_EQ(0u, s.find("Arguments:\n"));
	EXPECT_TRUE(NoLineEndsInSpace(s));
}

////////////////////////////////////////////////////////////////////////////////
// Property tests (fixed seed)
////////////////////////////////////////////////////////////////////////////////
namespace
{
	struct Row
	{
		std::string name, type, desc, detail;
		bool required;
		bool hasDesc;
	};

	// Independent reference of the documented layout.
	std::string Model(const std::vector<Row>& rows)
	{
		size_t nameW = 0, typeW = 0, detailW = 0;
		for (const Row& r : rows)
		{
			nameW = std::max(nameW, r.name.size() + 2);
			typeW = std::max(typeW, r.type.size());
			detailW = std::max(detailW, r.detail.size());
		}
		std::string out = "Arguments:\n";
		for (const Row& r : rows)
		{
			std::string s = "  ";
			s += r.required ? '<' : '[';
			s += r.name;
			s += r.required ? '>' : ']';
			if (typeW > 0)
			{
				s += Spaces(nameW - (r.name.size() + 2) + 2);
				s += r.type;
				s += Spaces(typeW - r.type.size() + 2);
				if (detailW > 0)
				{
					s += r.detail;
					s += Spaces(detailW - r.detail.size() + 3);
				}
				if (r.hasDesc) s += r.desc;
			}
			while (!s.empty() && s.back() == ' ') s.pop_back();
			out += s;
			out += '\n';
		}
		return out;
	}
}

TEST(HelpSystemArgumentsProperty, RandomParamSetsMatchReferenceAndLengthContract)
{
	std::mt19937 rng(1234);
	const char* names[] = { "a", "id", "longername", "x", "", "path", nullptr };
	const char* types[] = { "int", "float", "string", "bool", "enum", "", "veryverylongtype", nullptr };
	const char* descs[] = { nullptr, "", "Short.", "A longer description with several words." };
	const char* units[] = { nullptr, "", "m", "weeks" };
	const char* enums[] = { nullptr, "", "a|b", "weather|ecology|other" };
	const float nums[] = { 0.0f, 1.0f, -3.5f, 52.0f, 0.05f, 1e6f };

	auto pick = [&](size_t n) { return (size_t)(rng() % n); };

	for (int iter = 0; iter < 1000; iter++)
	{
		const unsigned count = 1 + (unsigned)pick(6);
		std::vector<ValueParam> ps(count);
		std::vector<Row> rows;
		for (unsigned i = 0; i < count; i++)
		{
			ValueParam& p = ps[i];
			p.name = names[pick(7)];
			p.type = types[pick(8)];
			p.required = pick(2) != 0;
			p.description = descs[pick(4)];
			p.unit = units[pick(4)];
			p.hasRange = pick(2) != 0;
			p.min = nums[pick(6)];
			p.max = nums[pick(6)];
			p.enumValues = enums[pick(4)];

			Row r;
			r.name = p.name ? p.name : "?";
			r.type = p.type ? p.type : "?";
			r.required = p.required;
			r.hasDesc = p.description && p.description[0];
			r.desc = p.description ? p.description : "";
			const bool hasUnit = p.unit && p.unit[0];
			if (p.type && std::strcmp(p.type, "enum") == 0 && p.enumValues && p.enumValues[0])
				r.detail = p.enumValues;
			else if (hasUnit && p.hasRange)
				r.detail = std::string(p.unit) + ", " + G(p.min, p.max);
			else if (hasUnit)
				r.detail = p.unit;
			else if (p.hasRange)
				r.detail = G(p.min, p.max);
			rows.push_back(r);
		}

		const unsigned needed = FormatArgumentsSection(ps.data(), count, nullptr, 0);
		std::string buf(needed + 1, '\0');
		const unsigned ret = FormatArgumentsSection(ps.data(), count, &buf[0], needed + 1);
		ASSERT_EQ(needed, ret) << "iter " << iter;
		ASSERT_EQ(needed, (unsigned)std::strlen(buf.c_str())) << "iter " << iter;
		buf.resize(needed);

		ASSERT_TRUE(NoLineEndsInSpace(buf)) << "iter " << iter << "\n" << buf;
		ASSERT_EQ(Model(rows), buf) << "iter " << iter;
		ASSERT_EQ('\n', buf.back());
		size_t lines = 0;
		for (char c : buf) if (c == '\n') lines++;
		ASSERT_EQ((size_t)count + 1, lines) << "iter " << iter;
	}
}

TEST(HelpSystemArgumentsProperty, RandomTruncationsArePrefixes)
{
	std::mt19937 rng(1234);
	ValueParam ps[3];
	ps[0] = P("alpha", "enum", true, "First one.");
	ps[0].enumValues = "x|y|z";
	ps[1] = P("b", "float", false, "Second.");
	ps[1].unit = "m";
	ps[1].hasRange = true;
	ps[1].min = 0.0f;
	ps[1].max = 9.0f;
	ps[2] = P("c", "bool", false, nullptr);
	const std::string full = Full(ps, 3);

	for (int iter = 0; iter < 200; iter++)
	{
		const unsigned cap = (unsigned)(rng() % (full.size() + 20));
		std::vector<char> buf(cap + 8, '\x7F');
		unsigned ret = FormatArgumentsSection(ps, 3, buf.data(), cap);
		ASSERT_EQ((unsigned)full.size(), ret);
		for (unsigned i = cap; i < cap + 8; i++) ASSERT_EQ('\x7F', buf[i]);
		if (cap > 0)
		{
			const size_t len = std::min<size_t>(full.size(), cap - 1);
			ASSERT_EQ(full.substr(0, len), std::string(buf.data())) << "cap " << cap;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// ShowCommandHelp / ShowGlobalHelp capture
////////////////////////////////////////////////////////////////////////////////
class HelpSystemArgumentsEdgeShow : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (IsInitialized()) Shutdown();
		Initialize();
	}
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

	static std::string Help(const char* name, int* rc = nullptr)
	{
		testing::internal::CaptureStdout();
		int r = ShowCommandHelp(Dia::Core::StringCRC(name));
		std::string out = testing::internal::GetCapturedStdout();
		if (rc) *rc = r;
		return out;
	}
};

TEST_F(HelpSystemArgumentsEdgeShow, NonexistentCommandReturnsThree)
{
	int rc = 0;
	std::string out = Help("edge-no-such-command", &rc);
	EXPECT_EQ(3, rc);
	EXPECT_NE(std::string::npos, out.find("Command not found: edge-no-such-command"));
	EXPECT_EQ(std::string::npos, out.find("Arguments:"));
}

TEST_F(HelpSystemArgumentsEdgeShow, ParamsWithZeroCountPrintsNoArgumentsSection)
{
	static const ValueParam kP[] = { P("id", "string", true, "The id.") };
	CommandInfo cmd = Make("edge-zero-count");
	cmd.params = kP;
	cmd.paramCount = 0;
	ASSERT_TRUE(RegisterCommand(cmd));
	EXPECT_EQ(std::string::npos, Help("edge-zero-count").find("Arguments:"));
}

TEST_F(HelpSystemArgumentsEdgeShow, NullParamsWithCountPrintsNoArgumentsSection)
{
	CommandInfo cmd = Make("edge-null-params");
	cmd.params = nullptr;
	cmd.paramCount = 5;
	ASSERT_TRUE(RegisterCommand(cmd));
	int rc = -1;
	EXPECT_EQ(std::string::npos, Help("edge-null-params", &rc).find("Arguments:"));
	EXPECT_EQ(0, rc);
}

// PIN: noArgs does not suppress the Arguments section when params are also set.
TEST_F(HelpSystemArgumentsEdgeShow, PinParamsWithNoArgsStillPrintsArguments)
{
	static const ValueParam kP[] = { P("id", "string", true, "The id.") };
	CommandInfo cmd = Make("edge-params-noargs");
	cmd.params = kP;
	cmd.paramCount = 1;
	cmd.noArgs = true;
	ASSERT_TRUE(RegisterCommand(cmd));
	EXPECT_NE(std::string::npos, Help("edge-params-noargs").find("Arguments:\n  <id>  string  The id.\n"));
}

// PIN: ShowCommandHelp formats into a 4096-byte stack buffer, so a longer section is
// truncated to 4095 characters (the full length is still reported by the formatter).
TEST_F(HelpSystemArgumentsEdgeShow, PinSectionOver4096IsTruncatedTo4095)
{
	static std::vector<ValueParam> ps;
	ps.clear();
	for (int i = 0; i < 100; i++)
		ps.push_back(P("param", "string", false, "A description that is deliberately long enough to overflow."));
	ASSERT_GT(FormatArgumentsSection(ps.data(), (unsigned)ps.size(), nullptr, 0), 4096u);

	CommandInfo cmd = Make("edge-big-section");
	cmd.params = ps.data();
	cmd.paramCount = (unsigned)ps.size();
	ASSERT_TRUE(RegisterCommand(cmd));

	std::string out = Help("edge-big-section");
	const size_t pos = out.find("\n\nArguments:\n");
	ASSERT_NE(std::string::npos, pos);
	EXPECT_EQ(4095u, out.size() - (pos + 2));
}

TEST_F(HelpSystemArgumentsEdgeShow, DescriptionWithFormatTokensIsVerbatim)
{
	static const ValueParam kP[] = { P("id", "string", true, "100%s of %d %n") };
	CommandInfo cmd = Make("edge-fmt-token");
	cmd.params = kP;
	cmd.paramCount = 1;
	ASSERT_TRUE(RegisterCommand(cmd));
	EXPECT_NE(std::string::npos, Help("edge-fmt-token").find("Arguments:\n  <id>  string  100%s of %d %n\n"));
}

TEST_F(HelpSystemArgumentsEdgeShow, MixedRowsThroughShowCommandHelpHaveNoTrailingSpaces)
{
	static ValueParam ps[2];
	ps[0] = P("a", "int", true, nullptr);
	ps[0].unit = "m";
	ps[1] = P("b", "int", false, nullptr);
	CommandInfo cmd = Make("edge-mixed-rows");
	cmd.params = ps;
	cmd.paramCount = 2;
	ASSERT_TRUE(RegisterCommand(cmd));
	std::string out = Help("edge-mixed-rows");
	EXPECT_NE(std::string::npos, out.find("Arguments:\n  <a>  int  m\n  [b]  int\n"));
}

// Defensive: a null description must not reach printf("%s"). Registration rejects null
// descriptions, so the registered entry is mutated in place to reach the guard.
TEST_F(HelpSystemArgumentsEdgeShow, NullDescriptionPrintsEmptyInShowCommandHelp)
{
	CommandInfo cmd = Make("edge-null-desc");
	ASSERT_TRUE(RegisterCommand(cmd));
	CommandInfo* live = const_cast<CommandInfo*>(GetCommand(Dia::Core::StringCRC("edge-null-desc")));
	ASSERT_NE(nullptr, live);
	live->description = nullptr;

	int rc = -1;
	std::string out = Help("edge-null-desc", &rc);
	EXPECT_EQ(0, rc);
	EXPECT_EQ(std::string::npos, out.find("(null)")) << out;
	EXPECT_NE(std::string::npos, out.find("DiaAPI edge-null-desc - \n\n"));
	EXPECT_NE(std::string::npos, out.find("Description:\n  \n"));
}

TEST_F(HelpSystemArgumentsEdgeShow, NullDescriptionPrintsEmptyInShowGlobalHelp)
{
	CommandInfo cmd = Make("edge-null-desc-global");
	ASSERT_TRUE(RegisterCommand(cmd));
	CommandInfo* live = const_cast<CommandInfo*>(GetCommand(Dia::Core::StringCRC("edge-null-desc-global")));
	ASSERT_NE(nullptr, live);
	live->description = nullptr;

	testing::internal::CaptureStdout();
	ShowGlobalHelp();
	std::string out = testing::internal::GetCapturedStdout();
	EXPECT_EQ(std::string::npos, out.find("(null)")) << out;
	EXPECT_NE(std::string::npos, out.find("edge-null-desc-global"));
}
