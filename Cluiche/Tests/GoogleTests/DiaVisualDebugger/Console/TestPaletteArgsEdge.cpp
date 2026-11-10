////////////////////////////////////////////////////////////////////////////////
// Filename: TestPaletteArgsEdge.cpp
// Description: Exhaustive edge/property tests for the palette "args" JSON
//              (ConsoleJson::BuildPaletteResults + WriteArgs, DiaValueSchema
//              Feature 4 task 7). Labels: FIX = behaviour fixed together with
//              these tests, PIN = documents current behaviour on purpose.
//
//              Limits documented here (and in ConsoleJson.h):
//               - enum value tokens are cut at 127 bytes (UTF-8 safe: a multibyte
//                 sequence is never split); empty tokens are skipped.
//               - args name/type/description/unit are NOT length-limited; only the
//                 caller's output buffer limits them (the item is then dropped whole).
//               - min/max are emitted only when hasRange and BOTH are finite.
//
//              Mixed-document golden: palette_results_commands_mixed_args.json
//              (regenerate with DIA_UPDATE_GOLDEN=1 and --filter="*PaletteArgsEdge.MixedDocument*").
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-palette.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/ConsoleJson.h>
#include <DiaVisualDebugger/Console/PaletteIndex.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaAPI/CommandRegistry/ValueParam.h>
#include <DiaAPI/Help/HelpSystem.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cfloat>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace Dia::VisualDebugger;
using namespace Dia::VisualDebugger::Console;
using Dia::API::ValueParam;
using Dia::Core::StringCRC;

namespace
{
	class StubArgsDomain : public IDebugDomain
	{
	public:
		StringCRC GetDomainId() const override { return StringCRC("ZArgsStubDomain"); }
		const char* GetDisplayName() const override { return "ZArgs Stub Domain"; }
		const char* GetDescription() const override { return "Stub domain for palette args tests"; }
		StringCRC GetGroup() const override { return StringCRC("CoreDebug"); }
		Dia::Core::RGBA GetAccentColour() const override { return Dia::Core::RGBA(); }
		bool HasWorldDrawers() const override { return false; }
		void GetJSONState(Json::Value&) override {}
		void OnCommand(StringCRC, const Json::Value&) override {}
	};

	ValueParam MakeParam(const char* name, const char* type, bool required = false, const char* desc = nullptr)
	{
		ValueParam p;
		p.name = name;
		p.type = type;
		p.required = required;
		p.description = desc;
		return p;
	}

	ValueParam MakeRange(const char* name, float mn, float mx)
	{
		ValueParam p = MakeParam(name, "float");
		p.hasRange = true;
		p.min = mn;
		p.max = mx;
		return p;
	}

	bool Parse(const std::string& text, Json::Value& root)
	{
		Json::Reader reader;
		return reader.parse(text, root);
	}

	std::filesystem::path FindGoldenDir()
	{
		namespace fs = std::filesystem;
		std::error_code ec;
		fs::path dir = fs::absolute(fs::path(__FILE__), ec).parent_path();
		for (int i = 0; i < 12 && !dir.empty(); ++i)
		{
			const fs::path candidate = dir / "Contracts" / "debugpanel";
			if (fs::is_directory(candidate, ec)) return candidate;
			if (dir == dir.parent_path()) break;
			dir = dir.parent_path();
		}
		return fs::path();
	}

	bool ReadFile(const std::filesystem::path& p, std::string& out)
	{
		std::ifstream f(p, std::ios::binary);
		if (!f) return false;
		std::ostringstream ss;
		ss << f.rdbuf();
		out = ss.str();
		return true;
	}

	// The number text the help formatter prints for the range of a float param ("lo", "hi").
	void HelpRangeText(float mn, float mx, std::string& lo, std::string& hi)
	{
		ValueParam p = MakeRange("n", mn, mx);
		char out[1024];
		Dia::API::FormatArgumentsSection(&p, 1, out, sizeof(out));
		const std::string s = out;   // "Arguments:\n  [n]  float  lo..hi\n"
		const size_t t = s.find("float");
		const size_t b = s.find_first_not_of(' ', t + 5);
		const size_t e = s.find('\n', b);
		const std::string detail = s.substr(b, e - b);
		const size_t dots = detail.find("..");
		lo = detail.substr(0, dots);
		hi = detail.substr(dots + 2);
	}
}

class PaletteArgsEdgeTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Known-empty command registry so commands from other tests never leak in.
		mWasInitialized = Dia::API::IsInitialized();
		if (mWasInitialized) Dia::API::Shutdown();
		Dia::API::Initialize();
	}

	void TearDown() override
	{
		mDomains.Clear();
		Dia::API::Shutdown();
		if (mWasInitialized) Dia::API::Initialize();
	}

	bool Reg(const char* name, const ValueParam* params, unsigned count, bool noArgs = false,
			 const char* desc = "args test command", const char* example = "")
	{
		Dia::API::CommandInfo info;
		info.name = StringCRC(name);
		info.description = desc;
		info.category = StringCRC("zargs");
		info.owner = "TestPaletteArgsEdge";
		info.version = "1.0.0";
		info.example = example;
		info.params = params;
		info.paramCount = count;
		info.noArgs = noArgs;
		info.callback = [](const Dia::API::CommandArgs&) { return 0; };
		return Dia::API::RegisterCommand(info);
	}

	// Raw builder call with an exact-size buffer; returns the builder result.
	bool BuildInto(const char* query, int mask, char* out, unsigned size)
	{
		PaletteIndex idx;
		idx.Build(mDomains);
		return ConsoleJson::BuildPaletteResults(idx, query, mask, out, size);
	}

	std::string Build(const char* query, int mask, unsigned size = 1u << 17)
	{
		std::vector<char> buf(size, 0);
		EXPECT_TRUE(BuildInto(query, mask, buf.data(), size));
		return std::string(buf.data());
	}

	// Registers "zargs.cmd" with params, builds the palette, returns the raw JSON text.
	std::string BuildOne(const ValueParam* params, unsigned count)
	{
		Dia::API::UnregisterCommand(StringCRC("zargs.cmd"));
		EXPECT_TRUE(Reg("zargs.cmd", params, count));
		return Build("zargs.cmd", 1);
	}

	// Same, parsed; returns items[0] (nullValue on failure).
	Json::Value ItemOf(const ValueParam* params, unsigned count, std::string* rawOut = nullptr)
	{
		const std::string raw = BuildOne(params, count);
		if (rawOut) *rawOut = raw;
		Json::Value root;
		EXPECT_TRUE(Parse(raw, root)) << raw;
		if (!root.isObject() || !root["items"].isArray() || root["items"].size() != 1)
		{
			ADD_FAILURE() << "expected exactly one item in: " << raw;
			return Json::Value();
		}
		return root["items"][0];
	}

	void CheckGolden(const char* fileName, const std::string& produced)
	{
		const std::filesystem::path dir = FindGoldenDir();
		ASSERT_FALSE(dir.empty()) << "Golden folder Contracts/debugpanel not found above " << __FILE__;
		const char* update = std::getenv("DIA_UPDATE_GOLDEN");
		const std::filesystem::path file = dir / fileName;
		if (update != nullptr && std::strcmp(update, "1") == 0)
		{
			std::ofstream f(file, std::ios::binary | std::ios::trunc);
			ASSERT_TRUE(f.good()) << "cannot write " << file.string();
			f << produced;
			return;
		}
		std::string expected;
		ASSERT_TRUE(ReadFile(file, expected)) << "golden file " << fileName << " is missing; regenerate with DIA_UPDATE_GOLDEN=1";
		EXPECT_EQ(expected, produced) << "golden file " << fileName << " differs; regenerate with DIA_UPDATE_GOLDEN=1 and review";
	}

	bool mWasInitialized = false;
	StubArgsDomain mDomain;
	DiaDebugDomainRegistry mDomains;
};

////////////////////////////////////////////////////////////////////////////////
// FIX (3): non-finite range bounds
////////////////////////////////////////////////////////////////////////////////

TEST_F(PaletteArgsEdgeTest, NonFiniteRangeOmitsBothBounds)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();
	const float cases[][2] = { { nan, 1.0f }, { 1.0f, nan }, { nan, nan }, { 0.0f, inf }, { -inf, 0.0f }, { -inf, inf }, { inf, inf } };
	for (const auto& c : cases)
	{
		const ValueParam params[] = { MakeRange("r", c[0], c[1]) };
		std::string raw;
		const Json::Value item = ItemOf(params, 1, &raw);
		ASSERT_TRUE(item.isObject()) << "min=" << c[0] << " max=" << c[1] << " -> " << raw;   // RED: "nan"/"inf" is invalid JSON
		const Json::Value& a = item["args"][0];
		EXPECT_EQ(a["name"].asString(), "r");
		EXPECT_FALSE(a.isMember("min")) << raw;
		EXPECT_FALSE(a.isMember("max")) << raw;
		EXPECT_EQ(raw.find("nan"), std::string::npos) << raw;
		EXPECT_EQ(raw.find("inf"), std::string::npos) << raw;
	}
}

TEST_F(PaletteArgsEdgeTest, FiniteRangeStillEmittedIncludingExtremes)
{
	const float cases[][2] = { { 0.0f, 0.0f }, { -1.0f, 1.0f }, { -FLT_MAX, FLT_MAX }, { FLT_MIN, FLT_MAX } };
	for (const auto& c : cases)
	{
		const ValueParam params[] = { MakeRange("r", c[0], c[1]) };
		const Json::Value item = ItemOf(params, 1);
		ASSERT_TRUE(item.isObject());
		const Json::Value& a = item["args"][0];
		ASSERT_TRUE(a.isMember("min"));
		ASSERT_TRUE(a.isMember("max"));
		EXPECT_NEAR(a["min"].asDouble(), static_cast<double>(c[0]), std::fabs(c[0]) * 1e-5 + 1e-30);
		EXPECT_NEAR(a["max"].asDouble(), static_cast<double>(c[1]), std::fabs(c[1]) * 1e-5 + 1e-30);
	}
}

TEST_F(PaletteArgsEdgeTest, MinGreaterThanMaxIsEmittedAsIs)   // PIN
{
	const ValueParam params[] = { MakeRange("r", 10.0f, 2.5f) };
	const Json::Value item = ItemOf(params, 1);
	EXPECT_DOUBLE_EQ(item["args"][0]["min"].asDouble(), 10.0);
	EXPECT_DOUBLE_EQ(item["args"][0]["max"].asDouble(), 2.5);
}

TEST_F(PaletteArgsEdgeTest, NonFiniteDoesNotAffectOtherParams)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const ValueParam params[] = { MakeRange("bad", nan, 1.0f), MakeRange("good", 1.0f, 2.0f), MakeParam("after", "int", true) };
	const Json::Value item = ItemOf(params, 3);
	ASSERT_EQ(item["args"].size(), 3u);
	EXPECT_FALSE(item["args"][0].isMember("min"));
	EXPECT_DOUBLE_EQ(item["args"][1]["min"].asDouble(), 1.0);
	EXPECT_EQ(item["args"][2]["name"].asString(), "after");
}

////////////////////////////////////////////////////////////////////////////////
// Number formatting
////////////////////////////////////////////////////////////////////////////////

TEST_F(PaletteArgsEdgeTest, NumberFormattingMatchesHelpTextFormatter)
{
	const float cases[] = {
		1e-7f, 1e10f, 0.1f + 0.2f, -0.0f, FLT_MAX, FLT_MIN, std::numeric_limits<float>::denorm_min(),
		0.0f, 1.0f, -1.0f, 100000.0f, 1e6f, 123456789.0f, 0.5f, 10.0f, 3.14159265f, -2.5e-3f };
	for (const float v : cases)
	{
		const ValueParam params[] = { MakeRange("n", v, v) };
		std::string raw;
		ItemOf(params, 1, &raw);
		std::string lo, hi;
		HelpRangeText(v, v, lo, hi);
		EXPECT_NE(raw.find("\"min\":" + lo + ",\"max\":" + hi + "}"), std::string::npos)
			<< "value " << v << " help text '" << lo << "' not found in " << raw;
	}
}

TEST_F(PaletteArgsEdgeTest, NumberTextIsValidJsonNumberForSpecialValues)
{
	struct Case { float v; const char* text; };
	const Case cases[] = { { 1e10f, "1e+10" }, { 0.5f, "0.5" }, { 10.0f, "10" }, { 1e6f, "1e+06" }, { 0.0f, "0" } };
	for (const Case& c : cases)
	{
		const ValueParam params[] = { MakeRange("n", c.v, c.v) };
		std::string raw;
		ItemOf(params, 1, &raw);
		EXPECT_NE(raw.find(std::string("\"min\":") + c.text + ","), std::string::npos) << raw;
	}
}

TEST_F(PaletteArgsEdgeTest, NumbersAreLocaleIndependent)   // FIX: decimal comma from a non-C locale must never leak into JSON
{
	const std::string saved = std::setlocale(LC_ALL, nullptr) ? std::setlocale(LC_ALL, nullptr) : "C";
	const char* candidates[] = { "de_DE.UTF-8", "de_DE", "German_Germany.1252", "German_Germany", "de-DE", "fr_FR.UTF-8", "French_France.1252", "fr-FR" };
	bool switched = false;
	for (const char* c : candidates)
	{
		if (std::setlocale(LC_NUMERIC, c) != nullptr)
		{
			char probe[16];
			std::snprintf(probe, sizeof(probe), "%g", 0.5);
			if (std::strchr(probe, ',') != nullptr) { switched = true; break; }
		}
	}
	if (!switched)
	{
		std::setlocale(LC_ALL, saved.c_str());
		GTEST_SKIP() << "no decimal-comma locale installed on this machine";
	}

	const ValueParam params[] = { MakeRange("n", 0.5f, 1234.5f) };
	std::string raw;
	const Json::Value item = ItemOf(params, 1, &raw);
	std::setlocale(LC_ALL, saved.c_str());
	EXPECT_NE(raw.find("\"min\":0.5,\"max\":1234.5}"), std::string::npos) << raw;
	ASSERT_TRUE(item.isObject());
	EXPECT_DOUBLE_EQ(item["args"][0]["min"].asDouble(), 0.5);
}

////////////////////////////////////////////////////////////////////////////////
// Optional keys
////////////////////////////////////////////////////////////////////////////////

TEST_F(PaletteArgsEdgeTest, HasRangeFalseOmitsMinMaxEvenWhenNonZero)
{
	ValueParam p = MakeParam("r", "float");
	p.hasRange = false;
	p.min = 5.0f;
	p.max = 9.0f;
	const Json::Value item = ItemOf(&p, 1);
	EXPECT_FALSE(item["args"][0].isMember("min"));
	EXPECT_FALSE(item["args"][0].isMember("max"));
}

TEST_F(PaletteArgsEdgeTest, UnitAndDescriptionOnlyWhenNonEmpty)
{
	ValueParam a = MakeParam("a", "int", false, nullptr);
	ValueParam b = MakeParam("b", "int", false, "");
	b.unit = "";
	ValueParam c = MakeParam("c", "int", false, "desc");
	c.unit = "m";
	const ValueParam params[] = { a, b, c };
	const Json::Value item = ItemOf(params, 3);
	EXPECT_FALSE(item["args"][0].isMember("description"));
	EXPECT_FALSE(item["args"][0].isMember("unit"));
	EXPECT_FALSE(item["args"][1].isMember("description"));
	EXPECT_FALSE(item["args"][1].isMember("unit"));
	EXPECT_EQ(item["args"][2]["description"].asString(), "desc");
	EXPECT_EQ(item["args"][2]["unit"].asString(), "m");
}

TEST_F(PaletteArgsEdgeTest, RequiredFlagRoundTrips)
{
	const ValueParam params[] = { MakeParam("req", "int", true), MakeParam("opt", "int", false) };
	const Json::Value item = ItemOf(params, 2);
	EXPECT_TRUE(item["args"][0]["required"].isBool());
	EXPECT_TRUE(item["args"][0]["required"].asBool());
	EXPECT_FALSE(item["args"][1]["required"].asBool());
}

TEST_F(PaletteArgsEdgeTest, NullAndEmptyNameTypeBecomeEmptyStrings)
{
	const ValueParam params[] = { MakeParam(nullptr, nullptr), MakeParam("", ""), MakeParam("n", nullptr), MakeParam(nullptr, "t") };
	const Json::Value item = ItemOf(params, 4);
	ASSERT_EQ(item["args"].size(), 4u);
	for (int i = 0; i < 4; ++i)
	{
		ASSERT_TRUE(item["args"][i]["name"].isString());
		ASSERT_TRUE(item["args"][i]["type"].isString());
	}
	EXPECT_EQ(item["args"][0]["name"].asString(), "");
	EXPECT_EQ(item["args"][0]["type"].asString(), "");
	EXPECT_EQ(item["args"][1]["name"].asString(), "");
	EXPECT_EQ(item["args"][2]["name"].asString(), "n");
	EXPECT_EQ(item["args"][2]["type"].asString(), "");
	EXPECT_EQ(item["args"][3]["type"].asString(), "t");
}

////////////////////////////////////////////////////////////////////////////////
// Enum value tokens (FIX 6)
////////////////////////////////////////////////////////////////////////////////

namespace
{
	std::vector<std::string> ValuesOf(const Json::Value& arg)
	{
		std::vector<std::string> v;
		for (const Json::Value& x : arg["values"]) v.push_back(x.asString());
		return v;
	}
}

TEST_F(PaletteArgsEdgeTest, EmptyEnumTokensAreSkipped)
{
	struct Case { const char* in; std::vector<std::string> expect; };
	const Case cases[] = {
		{ "a|b", { "a", "b" } },
		{ "a||b", { "a", "b" } },       // RED: emitted "" between a and b
		{ "|a", { "a" } },
		{ "a|", { "a" } },
		{ "|a|", { "a" } },
		{ "a|||||b|", { "a", "b" } },
		{ "single", { "single" } },
		{ " | ", { " ", " " } },        // whitespace is a token (only truly empty tokens are skipped)
	};
	for (const Case& c : cases)
	{
		ValueParam p = MakeParam("m", "enum");
		p.enumValues = c.in;
		const Json::Value item = ItemOf(&p, 1);
		ASSERT_TRUE(item.isObject()) << c.in;
		EXPECT_EQ(ValuesOf(item["args"][0]), c.expect) << "input '" << c.in << "'";
	}
}

TEST_F(PaletteArgsEdgeTest, OnlyEmptyTokensOmitsValuesKey)
{
	for (const char* in : { "|", "||", "|||||" })
	{
		ValueParam p = MakeParam("m", "enum");
		p.enumValues = in;
		const Json::Value item = ItemOf(&p, 1);
		ASSERT_TRUE(item.isObject()) << in;
		EXPECT_FALSE(item["args"][0].isMember("values")) << "input '" << in << "'";   // RED: "values":[] / [""]
	}
}

TEST_F(PaletteArgsEdgeTest, NullOrEmptyEnumValuesOmitsValuesKey)
{
	ValueParam a = MakeParam("a", "enum");
	a.enumValues = nullptr;
	ValueParam b = MakeParam("b", "enum");
	b.enumValues = "";
	const ValueParam params[] = { a, b };
	const Json::Value item = ItemOf(params, 2);
	EXPECT_FALSE(item["args"][0].isMember("values"));
	EXPECT_FALSE(item["args"][1].isMember("values"));
}

TEST_F(PaletteArgsEdgeTest, EnumTokensLongerThan127BytesAreCutSilently)   // documented limit
{
	for (size_t len : { size_t(126), size_t(127), size_t(128), size_t(300) })
	{
		const std::string tok(len, 'q');
		const std::string joined = tok + "|short";
		ValueParam p = MakeParam("m", "enum");
		p.enumValues = joined.c_str();
		const Json::Value item = ItemOf(&p, 1);
		const std::vector<std::string> v = ValuesOf(item["args"][0]);
		ASSERT_EQ(v.size(), 2u) << len;
		EXPECT_EQ(v[0].size(), std::min<size_t>(len, 127)) << len;
		EXPECT_EQ(v[0], tok.substr(0, 127));
		EXPECT_EQ(v[1], "short");   // the cut never swallows the next token
	}
}

TEST_F(PaletteArgsEdgeTest, EnumTokenCutNeverSplitsAMultibyteSequence)   // FIX (UTF-8 safe cut)
{
	// 126 ASCII bytes + a 2-byte char: a naive 127-byte cut would leave a lone lead byte.
	const std::string tok = std::string(126, 'a') + "\xC3\xA9";
	ValueParam p = MakeParam("m", "enum");
	p.enumValues = tok.c_str();
	const Json::Value item = ItemOf(&p, 1);
	ASSERT_TRUE(item.isObject());
	const std::string v = ValuesOf(item["args"][0])[0];
	EXPECT_EQ(v, std::string(126, 'a'));

	// 125 ASCII + 3-byte char (snowman): cut would land inside it at 127.
	const std::string tok3 = std::string(125, 'b') + "\xE2\x98\x83";
	p.enumValues = tok3.c_str();
	const Json::Value item3 = ItemOf(&p, 1);
	EXPECT_EQ(ValuesOf(item3["args"][0])[0], std::string(125, 'b'));

	// A char that fits entirely is kept.
	const std::string tokOk = std::string(125, 'c') + "\xC3\xA9";
	p.enumValues = tokOk.c_str();
	const Json::Value itemOk = ItemOf(&p, 1);
	EXPECT_EQ(ValuesOf(itemOk["args"][0])[0], tokOk);
}

TEST_F(PaletteArgsEdgeTest, EnumValuesOnNonEnumTypeStillEmitted)   // PIN
{
	const char* const kTypes[] = { "string", "int", "float", "bool", nullptr, "" };
	for (const char* type : kTypes)
	{
		ValueParam p = MakeParam("m", type);
		p.enumValues = "x|y";
		const Json::Value item = ItemOf(&p, 1);
		ASSERT_TRUE(item.isObject());
		EXPECT_EQ(ValuesOf(item["args"][0]), (std::vector<std::string>{ "x", "y" })) << (type ? type : "(null)");
	}
}

TEST_F(PaletteArgsEdgeTest, ManyEnumValuesAllEmittedInOrder)
{
	std::string joined;
	std::vector<std::string> expect;
	for (int i = 0; i < 100; ++i)
	{
		const std::string t = "v" + std::to_string(i);
		expect.push_back(t);
		joined += (i ? "|" : "") + t;
	}
	ValueParam p = MakeParam("m", "enum");
	p.enumValues = joined.c_str();
	const Json::Value item = ItemOf(&p, 1);
	EXPECT_EQ(ValuesOf(item["args"][0]), expect);
}

////////////////////////////////////////////////////////////////////////////////
// Escaping and UTF-8
////////////////////////////////////////////////////////////////////////////////

TEST_F(PaletteArgsEdgeTest, SpecialCharactersRoundTripInEveryTextField)
{
	const std::vector<std::string> specials = {
		"quote\"inside", "back\\slash", "new\nline", "tab\tchar", "cr\rchar", "bs\bff\f",
		std::string("ctl\x01" "one"), std::string("ctl\x1f" "us"), "trailing\\", "\"", "\\\"", "</script><img onerror=x>", "a&b<c>d",
		std::string("mix\"\\\n\t\x01\x1f" "end") };
	for (const std::string& s : specials)
	{
		ValueParam p = MakeParam(s.c_str(), s.c_str(), true, s.c_str());
		p.unit = s.c_str();
		p.enumValues = s.c_str();   // contains no '|', so a single token
		std::string raw;
		const Json::Value item = ItemOf(&p, 1, &raw);
		ASSERT_TRUE(item.isObject()) << raw;
		const Json::Value& a = item["args"][0];
		EXPECT_EQ(a["name"].asString(), s);
		EXPECT_EQ(a["type"].asString(), s);
		EXPECT_EQ(a["description"].asString(), s);
		EXPECT_EQ(a["unit"].asString(), s);
		ASSERT_EQ(a["values"].size(), 1u);
		EXPECT_EQ(a["values"][0].asString(), s);
		for (const unsigned char ch : raw) EXPECT_GE(ch, 0x20) << "raw control byte in output";
	}
}

TEST_F(PaletteArgsEdgeTest, ControlCharactersUseUnicodeEscapes)
{
	ValueParam p = MakeParam("n", "t", false, "\x01" "\x1f");
	std::string raw;
	ItemOf(&p, 1, &raw);
	EXPECT_NE(raw.find("\"description\":\"\\u0001\\u001f\""), std::string::npos) << raw;
}

TEST_F(PaletteArgsEdgeTest, MultibyteUtf8RoundTrips)
{
	const std::string s = "h\xC3\xA9llo \xE2\x98\x83 \xF0\x9F\x98\x80 \xE6\x97\xA5\xE6\x9C\xAC";
	ValueParam p = MakeParam(s.c_str(), "string", false, s.c_str());
	p.unit = s.c_str();
	p.enumValues = s.c_str();
	const Json::Value item = ItemOf(&p, 1);
	const Json::Value& a = item["args"][0];
	EXPECT_EQ(a["name"].asString(), s);
	EXPECT_EQ(a["description"].asString(), s);
	EXPECT_EQ(a["unit"].asString(), s);
	EXPECT_EQ(a["values"][0].asString(), s);
}

TEST_F(PaletteArgsEdgeTest, InvalidUtf8PassesThroughUnchanged)   // PIN: no validation/replacement; bytes >= 0x80 are copied verbatim
{
	const std::string bad = "bad\xFF\xFE" "x\xC3" "y\x80";
	ValueParam p = MakeParam("n", "string", false, bad.c_str());
	std::string raw;
	std::vector<char> buf(1u << 14, 0);
	Dia::API::UnregisterCommand(StringCRC("zargs.cmd"));
	ASSERT_TRUE(Reg("zargs.cmd", &p, 1));
	ASSERT_TRUE(BuildInto("zargs.cmd", 1, buf.data(), static_cast<unsigned>(buf.size())));
	raw = buf.data();
	EXPECT_NE(raw.find("\"description\":\"" + bad + "\""), std::string::npos);
}

TEST_F(PaletteArgsEdgeTest, DelCharacterPassesThrough)   // PIN: 0x7f is legal in JSON strings
{
	ValueParam p = MakeParam("n", "string", false, "a\x7f" "b");
	const Json::Value item = ItemOf(&p, 1);
	EXPECT_EQ(item["args"][0]["description"].asString(), "a\x7f" "b");
}

////////////////////////////////////////////////////////////////////////////////
// Counts and null pointers
////////////////////////////////////////////////////////////////////////////////

TEST_F(PaletteArgsEdgeTest, ParamCountZeroWithNonNullParamsEmitsNoArgs)
{
	const ValueParam params[] = { MakeParam("a", "int") };
	std::string raw;
	const Json::Value item = ItemOf(params, 0, &raw);
	ASSERT_TRUE(item.isObject());
	EXPECT_FALSE(item.isMember("args")) << raw;
}

TEST_F(PaletteArgsEdgeTest, NonZeroCountWithNullParamsEmitsNoArgs)
{
	std::string raw;
	const Json::Value item = ItemOf(nullptr, 5, &raw);
	ASSERT_TRUE(item.isObject());
	EXPECT_FALSE(item.isMember("args")) << raw;
}

TEST_F(PaletteArgsEdgeTest, HundredParamsAllEmittedInOrder)
{
	std::vector<std::string> names;
	for (int i = 0; i < 100; ++i) names.push_back("p" + std::to_string(i));
	std::vector<ValueParam> params;
	for (const auto& n : names) params.push_back(MakeParam(n.c_str(), "int", (&n - &names[0]) % 2 == 0, "d"));
	const Json::Value item = ItemOf(params.data(), 100);
	ASSERT_EQ(item["args"].size(), 100u);
	for (int i = 0; i < 100; ++i)
	{
		EXPECT_EQ(item["args"][i]["name"].asString(), names[i]);
		EXPECT_EQ(item["args"][i]["required"].asBool(), i % 2 == 0);
	}
}

TEST_F(PaletteArgsEdgeTest, NoArgsCommandWithoutParamsEmitsNoArgs)
{
	ASSERT_TRUE(Reg("zargs.cmd", nullptr, 0, true));
	Json::Value root;
	ASSERT_TRUE(Parse(Build("zargs.cmd", 1), root));
	ASSERT_EQ(root["items"].size(), 1u);
	EXPECT_FALSE(root["items"][0].isMember("args"));
}

TEST_F(PaletteArgsEdgeTest, NoArgsFlagDoesNotSuppressDeclaredParams)   // PIN: noArgs is not consulted by the palette
{
	const ValueParam params[] = { MakeParam("a", "int") };
	ASSERT_TRUE(Reg("zargs.cmd", params, 1, true));
	Json::Value root;
	ASSERT_TRUE(Parse(Build("zargs.cmd", 1), root));
	ASSERT_EQ(root["items"].size(), 1u);
	EXPECT_TRUE(root["items"][0].isMember("args"));
}

////////////////////////////////////////////////////////////////////////////////
// Output buffer behaviour
////////////////////////////////////////////////////////////////////////////////

TEST_F(PaletteArgsEdgeTest, EveryBufferSizeGivesValidJsonAndNeverOverruns)
{
	const ValueParam params[] = {
		MakeParam("target", "string", true, "Entity \"to\" spawn"), MakeRange("radius", 0.5f, 10.0f), MakeParam("mode", "enum") };
	ValueParam withEnum[3] = { params[0], params[1], params[2] };
	withEnum[2].enumValues = "fast|slow";
	ASSERT_TRUE(Reg("zargs.cmd", withEnum, 3));

	const std::string full = Build("zargs.cmd", 1);
	const unsigned L = static_cast<unsigned>(full.size());
	ASSERT_GT(L, 100u);

	for (unsigned size = 0; size <= L + 4; ++size)
	{
		std::vector<char> buf(size + 16, 'X');
		const bool ok = BuildInto("zargs.cmd", 1, buf.data(), size);
		for (unsigned k = size; k < size + 16; ++k) ASSERT_EQ(buf[k], 'X') << "overrun at size " << size;   // nothing written past outSize
		if (size == 0)
		{
			EXPECT_FALSE(ok);
			continue;
		}
		if (!ok)
		{
			EXPECT_EQ(buf[0], '\0') << "size " << size;
			continue;
		}
		const std::string text(buf.data());
		ASSERT_LT(text.size(), static_cast<size_t>(size)) << "no NUL inside buffer, size " << size;
		Json::Value root;
		ASSERT_TRUE(Parse(text, root)) << "size " << size << ": " << text;
		EXPECT_EQ(root["total"].asInt(), 1);   // counts intact even when the item is dropped
		const size_t items = root["items"].size();
		if (size >= L + 1)
		{
			EXPECT_EQ(text, full) << "size " << size;
			EXPECT_EQ(items, 1u);
		}
		else
		{
			EXPECT_EQ(items, 0u) << "size " << size;                         // item rolled back whole
			EXPECT_EQ(text.find("args"), std::string::npos) << text;        // no partial args array
			EXPECT_EQ(text.find("\"name\""), std::string::npos) << text;    // no partial item
		}
	}
}

TEST_F(PaletteArgsEdgeTest, ExactFitAndOneByteShort)
{
	const ValueParam params[] = { MakeParam("a", "int", true, "d") };
	ASSERT_TRUE(Reg("zargs.cmd", params, 1));
	const std::string full = Build("zargs.cmd", 1);
	const unsigned L = static_cast<unsigned>(full.size());

	std::vector<char> exact(L + 1, 0);
	ASSERT_TRUE(BuildInto("zargs.cmd", 1, exact.data(), L + 1));
	EXPECT_EQ(std::string(exact.data()), full);

	std::vector<char> shortBuf(L, 0);
	ASSERT_TRUE(BuildInto("zargs.cmd", 1, shortBuf.data(), L));
	Json::Value root;
	ASSERT_TRUE(Parse(shortBuf.data(), root));
	EXPECT_EQ(root["items"].size(), 0u);
	EXPECT_EQ(root["total"].asInt(), 1);
}

TEST_F(PaletteArgsEdgeTest, TinyBuffersFailCleanly)
{
	const ValueParam params[] = { MakeParam("a", "int") };
	ASSERT_TRUE(Reg("zargs.cmd", params, 1));
	for (unsigned size : { 1u, 2u, 5u, 12u })
	{
		std::vector<char> buf(size + 4, 'X');
		EXPECT_FALSE(BuildInto("zargs.cmd", 1, buf.data(), size)) << size;
		EXPECT_EQ(buf[0], '\0');
		for (unsigned k = size; k < size + 4; ++k) EXPECT_EQ(buf[k], 'X');
	}
	EXPECT_FALSE(BuildInto("zargs.cmd", 1, nullptr, 100));
}

TEST_F(PaletteArgsEdgeTest, HugeArgsOnFirstItemDropsEverythingAfterIt)   // PIN: truncation is a prefix; a later small item is dropped too
{
	const ValueParam small[] = { MakeParam("s", "int") };
	ASSERT_TRUE(Reg("zargs.b_small", small, 1));
	const std::string smallOnly = Build("zargs", 1);
	const unsigned smallLen = static_cast<unsigned>(smallOnly.size());

	const std::string longDesc(400, 'D');
	std::vector<ValueParam> huge;
	for (int i = 0; i < 30; ++i) huge.push_back(MakeParam("hugearg", "string", true, longDesc.c_str()));
	ASSERT_TRUE(Reg("zargs.a_huge", huge.data(), 30));

	std::vector<char> buf(smallLen + 64, 0);
	ASSERT_TRUE(BuildInto("zargs", 1, buf.data(), smallLen + 64));
	Json::Value root;
	ASSERT_TRUE(Parse(buf.data(), root)) << buf.data();
	EXPECT_EQ(root["total"].asInt(), 2);
	EXPECT_EQ(root["items"].size(), 0u);   // a_huge sorts first, does not fit, loop stops

	// With room for both, both are emitted in name order.
	Json::Value both;
	ASSERT_TRUE(Parse(Build("zargs", 1), both));
	ASSERT_EQ(both["items"].size(), 2u);
	EXPECT_EQ(both["items"][0]["name"].asString(), "zargs.a_huge");
	EXPECT_EQ(both["items"][0]["args"].size(), 30u);
	EXPECT_EQ(both["items"][1]["name"].asString(), "zargs.b_small");
}

TEST_F(PaletteArgsEdgeTest, SmallItemsBeforeABigOneAreKept)
{
	const ValueParam small[] = { MakeParam("s", "int") };
	ASSERT_TRUE(Reg("zargs.a_small", small, 1));
	const std::string smallOnly = Build("zargs", 1);

	const std::string longDesc(400, 'D');
	std::vector<ValueParam> huge;
	for (int i = 0; i < 30; ++i) huge.push_back(MakeParam("hugearg", "string", true, longDesc.c_str()));
	ASSERT_TRUE(Reg("zargs.b_huge", huge.data(), 30));

	const unsigned size = static_cast<unsigned>(smallOnly.size()) + 64;
	std::vector<char> buf(size, 0);
	ASSERT_TRUE(BuildInto("zargs", 1, buf.data(), size));
	Json::Value root;
	ASSERT_TRUE(Parse(buf.data(), root));
	ASSERT_EQ(root["items"].size(), 1u);
	EXPECT_EQ(root["items"][0]["name"].asString(), "zargs.a_small");
	EXPECT_EQ(root["items"][0]["args"].size(), 1u);
	EXPECT_EQ(root["total"].asInt(), 2);
}

////////////////////////////////////////////////////////////////////////////////
// PINs on palette semantics
////////////////////////////////////////////////////////////////////////////////

TEST_F(PaletteArgsEdgeTest, JsonCommandsNeverReachThePalette)   // PIN: PaletteIndex::Build only calls ListCommandInfos
{
	static const ValueParam kParams[] = { MakeParam("p", "int", true) };
	Dia::API::CommandInfoJson info;
	info.name = StringCRC("zargs.jsoncmd");
	info.description = "json command";
	info.category = StringCRC("zargs");
	info.owner = "TestPaletteArgsEdge";
	info.params = kParams;
	info.paramCount = 1;
	info.callback = [](const Json::Value&) { return Json::Value(Json::objectValue); };
	ASSERT_TRUE(Dia::API::RegisterCommandJson(info));

	Dia::Core::Containers::DynamicArrayC<Dia::API::CommandInfoJson, 256> jsonInfos;
	Dia::API::ListJsonCommandInfos(jsonInfos);
	EXPECT_EQ(jsonInfos.Size(), 1u);   // it IS registered...

	Json::Value root;
	ASSERT_TRUE(Parse(Build("zargs", 3), root));
	EXPECT_EQ(root["total"].asInt(), 0);   // ...but absent from the palette
	EXPECT_EQ(root["counts"]["commands"].asInt(), 0);
	EXPECT_EQ(root["items"].size(), 0u);
}

TEST_F(PaletteArgsEdgeTest, SearchDoesNotMatchArgumentNames)   // PIN: only name/description/category are searched
{
	const ValueParam params[] = { MakeParam("qzxuniquearg", "string", true, "qzxuniquedesc") };
	ASSERT_TRUE(Reg("zargs.cmd", params, 1));
	Json::Value root;
	ASSERT_TRUE(Parse(Build("qzxuniquearg", 3), root));
	EXPECT_EQ(root["total"].asInt(), 0);
	ASSERT_TRUE(Parse(Build("qzxuniquedesc", 3), root));
	EXPECT_EQ(root["total"].asInt(), 0);
	ASSERT_TRUE(Parse(Build("zargs", 3), root));
	EXPECT_EQ(root["total"].asInt(), 1);
}

TEST_F(PaletteArgsEdgeTest, ParamsPointerIsReadAtBuildJsonTimeNotCopied)   // PIN: lifetime contract, see below
{
	// PaletteIndex::Build stores the caller-owned params POINTER (same contract as CommandInfo).
	// The array must therefore outlive every BuildPaletteResults call made on that index; freeing it
	// after Build is a use-after-free. This test only mutates a still-alive array to prove the pointer
	// (not a copy) is what the JSON is built from.
	ValueParam params[1] = { MakeParam("before", "int") };
	ASSERT_TRUE(Reg("zargs.cmd", params, 1));
	PaletteIndex idx;
	idx.Build(mDomains);
	std::vector<char> buf(1u << 14);
	ASSERT_TRUE(ConsoleJson::BuildPaletteResults(idx, "zargs", 1, buf.data(), static_cast<unsigned>(buf.size())));
	EXPECT_NE(std::string(buf.data()).find("\"name\":\"before\""), std::string::npos);
	params[0].name = "after";
	ASSERT_TRUE(ConsoleJson::BuildPaletteResults(idx, "zargs", 1, buf.data(), static_cast<unsigned>(buf.size())));
	EXPECT_NE(std::string(buf.data()).find("\"name\":\"after\""), std::string::npos);
	EXPECT_EQ(idx.GetItem(0).params, params);
	EXPECT_EQ(idx.GetItem(0).paramCount, 1u);
}

TEST_F(PaletteArgsEdgeTest, TwoFiftySixCommandsWithParamsRespectMaxResults)   // PIN
{
	static const ValueParam kParams[] = { MakeParam("a", "int", true, "d"), MakeRange("b", 0.0f, 1.0f) };
	for (int i = 0; i < 256; ++i)
	{
		char name[32];
		std::snprintf(name, sizeof(name), "zbulk.cmd%03d", i);
		ASSERT_TRUE(Reg(name, kParams, 2)) << name;
	}
	Json::Value root;
	ASSERT_TRUE(Parse(Build("zbulk", 1, 1u << 20), root));
	EXPECT_EQ(root["total"].asInt(), 256);
	EXPECT_EQ(root["counts"]["commands"].asInt(), 256);
	ASSERT_EQ(root["items"].size(), static_cast<size_t>(PaletteIndex::kMaxResults));
	for (const Json::Value& it : root["items"]) EXPECT_EQ(it["args"].size(), 2u);
	EXPECT_EQ(root["items"][0]["name"].asString(), "zbulk.cmd000");
	EXPECT_EQ(root["items"][49]["name"].asString(), "zbulk.cmd049");
}

TEST_F(PaletteArgsEdgeTest, DomainsNeverEmitArgsUnderAnyTypeFilter)   // PIN
{
	mDomains.Register(mDomain);
	const ValueParam params[] = { MakeParam("a", "int", true) };
	ASSERT_TRUE(Reg("zargs.cmd", params, 1));
	for (int mask : { 1, 2, 3 })
	{
		Json::Value root;
		ASSERT_TRUE(Parse(Build("z", mask), root)) << mask;
		for (const Json::Value& it : root["items"])
		{
			if (it["type"].asString() == "domain")
			{
				EXPECT_FALSE(it.isMember("args")) << "mask " << mask;
			}
			else
			{
				EXPECT_TRUE(it.isMember("args")) << "mask " << mask;
			}
		}
		if (mask == 2) EXPECT_EQ(root["items"].size(), 1u);
		if (mask == 1) EXPECT_EQ(root["items"].size(), 1u);
		if (mask == 3) EXPECT_EQ(root["items"].size(), 2u);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Byte stability for commands without params
////////////////////////////////////////////////////////////////////////////////

TEST_F(PaletteArgsEdgeTest, ExistingGoldensContainNoArgsKey)
{
	const std::filesystem::path dir = FindGoldenDir();
	ASSERT_FALSE(dir.empty());
	for (const char* f : { "palette_results_all.json", "palette_results_commands.json", "palette_results_domains.json" })
	{
		std::string text;
		ASSERT_TRUE(ReadFile(dir / f, text)) << f;
		EXPECT_EQ(text.find("\"args\""), std::string::npos) << f;
	}
}

TEST_F(PaletteArgsEdgeTest, LiveCommandsWithoutParamsNeverGainArgs)
{
	ASSERT_TRUE(Reg("debug.layer.enable", nullptr, 0, false, "Enable a debug layer", "debug.layer.enable Entities"));
	ASSERT_TRUE(Reg("debug.stats.reset", nullptr, 0, false, "Reset <b>all</b> stats"));
	ASSERT_TRUE(Reg("sim.pause", nullptr, 0, true, "Say \"args\" here"));
	mDomains.Register(mDomain);
	for (const char* q : { "", "debug", "sim", "args", "zargs" })
	{
		for (int mask : { 1, 2, 3 })
		{
			const std::string raw = Build(q, mask);
			EXPECT_EQ(raw.find("\"args\":"), std::string::npos) << "query '" << q << "' mask " << mask << ": " << raw;
		}
	}
}

TEST_F(PaletteArgsEdgeTest, MixedDocumentGolden)
{
	static const ValueParam kWithParams[] = {
		{ "target", "string", true, "The \"target\" path C:\\x", nullptr, false, 0.0f, 0.0f, nullptr },
		{ "count", "int", false, "Line1\nLine2\t<tab>", "items", true, 0.0f, 100.0f, nullptr },
		{ "mode", "enum", false, nullptr, nullptr, false, 0.0f, 0.0f, "a||b&c|<d>|" },
	};
	static const ValueParam kEscaped[] = {
		{ "ctl\x01\x1f", "st\"r", false, "na\xC3\xAFve \xE2\x98\x83 </script>", "\\", true, -1e-7f, 1e10f, nullptr },
	};
	ASSERT_TRUE(Reg("zmix.a_plain", nullptr, 0, false, "Plain command", "zmix.a_plain 1"));
	ASSERT_TRUE(Reg("zmix.b_params", kWithParams, 3, false, "Has params"));
	ASSERT_TRUE(Reg("zmix.c_noargs", nullptr, 0, true, "Takes no arguments"));
	ASSERT_TRUE(Reg("zmix.d_escaped", kEscaped, 1, false, "Escaped \"chars\" \\ here"));

	const std::string raw = Build("zmix", 1);
	Json::Value root;
	ASSERT_TRUE(Parse(raw, root)) << raw;
	ASSERT_EQ(root["items"].size(), 4u);
	EXPECT_FALSE(root["items"][0].isMember("args"));
	EXPECT_EQ(root["items"][1]["args"].size(), 3u);
	EXPECT_EQ(ValuesOf(root["items"][1]["args"][2]), (std::vector<std::string>{ "a", "b&c", "<d>" }));
	EXPECT_FALSE(root["items"][2].isMember("args"));
	EXPECT_EQ(root["items"][3]["args"][0]["name"].asString(), "ctl\x01\x1f");

	CheckGolden("palette_results_commands_mixed_args.json", raw);
}

////////////////////////////////////////////////////////////////////////////////
// Property test: fixed seed, random ValueParam sets round-trip
////////////////////////////////////////////////////////////////////////////////

TEST_F(PaletteArgsEdgeTest, RandomParamSetsAlwaysProduceParsableRoundTrippingJson)
{
	std::mt19937 rng(1234);
	auto rnd = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };

	const std::vector<std::string> alphabet = {
		"a", "b", "Z", "0", "9", " ", "_", ".", "-", "\"", "\\", "\n", "\t", "\x01", "\x1f", "<", ">", "&", "'", "/",
		"\xC3\xA9", "\xE2\x98\x83", "\xF0\x9F\x98\x80" };
	auto randStr = [&](int maxLen, bool allowPipe) {
		std::string s;
		const int n = rnd(0, maxLen);
		for (int i = 0; i < n; ++i) s += alphabet[rnd(0, static_cast<int>(alphabet.size()) - 1)];
		(void)allowPipe;
		return s;
	};
	const char* types[] = { "string", "int", "float", "bool", "enum", "", "custom" };

	for (int iter = 0; iter < 200; ++iter)
	{
		struct Gen
		{
			std::string name, type, desc, unit, enumRaw;
			std::vector<std::string> tokens;
			ValueParam p;
		};
		const int count = rnd(0, 6);
		std::vector<Gen> gens(count);
		for (Gen& g : gens)
		{
			g.name = randStr(12, false);
			g.type = types[rnd(0, 6)];
			g.desc = randStr(20, false);
			g.unit = randStr(4, false);
			const int nTok = rnd(0, 4);
			for (int t = 0; t < nTok; ++t)
			{
				std::string tok = randStr(8, false);
				if (tok.empty()) tok = "t";
				g.tokens.push_back(tok);
			}
			for (size_t t = 0; t < g.tokens.size(); ++t)
			{
				for (int extra = rnd(0, 2); extra > 0; --extra) g.enumRaw += "|";   // random empty tokens
				if (t > 0) g.enumRaw += "|";
				g.enumRaw += g.tokens[t];
			}
			if (rnd(0, 3) == 0) g.enumRaw += "|";
		}
		std::vector<ValueParam> params(count);
		std::vector<float> mins(count), maxs(count);
		for (int i = 0; i < count; ++i)
		{
			Gen& g = gens[i];
			ValueParam& p = params[i];
			p.name = g.name.c_str();
			p.type = g.type.c_str();
			p.required = rnd(0, 1) == 1;
			p.description = rnd(0, 4) == 0 ? nullptr : g.desc.c_str();
			p.unit = rnd(0, 4) == 0 ? nullptr : g.unit.c_str();
			p.hasRange = rnd(0, 1) == 1;
			const double mant = std::uniform_real_distribution<double>(-1.0, 1.0)(rng);
			p.min = static_cast<float>(mant * std::pow(10.0, rnd(-8, 8)));
			p.max = static_cast<float>(std::uniform_real_distribution<double>(-1.0, 1.0)(rng) * std::pow(10.0, rnd(-8, 8)));
			p.enumValues = rnd(0, 5) == 0 ? nullptr : g.enumRaw.c_str();
		}

		std::string raw;
		const Json::Value item = ItemOf(params.data(), static_cast<unsigned>(count), &raw);
		ASSERT_TRUE(item.isObject()) << "iter " << iter << ": " << raw;
		if (count == 0)
		{
			EXPECT_FALSE(item.isMember("args")) << iter;
			continue;
		}
		ASSERT_EQ(item["args"].size(), static_cast<size_t>(count)) << iter;
		for (int i = 0; i < count; ++i)
		{
			const ValueParam& p = params[i];
			const Json::Value& a = item["args"][i];
			EXPECT_EQ(a["name"].asString(), gens[i].name) << iter;
			EXPECT_EQ(a["type"].asString(), gens[i].type) << iter;
			EXPECT_EQ(a["required"].asBool(), p.required) << iter;
			const bool hasDesc = p.description != nullptr && p.description[0] != '\0';
			EXPECT_EQ(a.isMember("description"), hasDesc) << iter;
			if (hasDesc) EXPECT_EQ(a["description"].asString(), gens[i].desc) << iter;
			const bool hasUnit = p.unit != nullptr && p.unit[0] != '\0';
			EXPECT_EQ(a.isMember("unit"), hasUnit) << iter;
			if (hasUnit) EXPECT_EQ(a["unit"].asString(), gens[i].unit) << iter;
			EXPECT_EQ(a.isMember("min"), p.hasRange) << iter;
			EXPECT_EQ(a.isMember("max"), p.hasRange) << iter;
			if (p.hasRange)
			{
				EXPECT_NEAR(a["min"].asDouble(), static_cast<double>(p.min), std::fabs(p.min) * 1e-5 + 1e-30) << iter;
				EXPECT_NEAR(a["max"].asDouble(), static_cast<double>(p.max), std::fabs(p.max) * 1e-5 + 1e-30) << iter;
			}
			const bool hasVals = p.enumValues != nullptr && !gens[i].tokens.empty();
			EXPECT_EQ(a.isMember("values"), hasVals) << iter << " raw '" << gens[i].enumRaw << "'";
			if (hasVals) EXPECT_EQ(ValuesOf(a), gens[i].tokens) << iter;
		}
	}
}

#endif // DIA_DEBUG
