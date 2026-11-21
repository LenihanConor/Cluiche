////////////////////////////////////////////////////////////////////////////////
// Filename: TestEditorActionParamJsonEdge.cpp
// Description: Edge tests for EditorActionParamToJson (ValueParam -> manifest JSON).
// Spec: docs/specs/applications/dia/systems/diavalueschema/code-side-descriptions.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaAPI/CommandRegistry/ValueParam.h>
#include <DiaEditor/EditorAPI/EditorActionManifest.h>
#include <DiaCore/Json/external/json/json.h>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <type_traits>

using namespace Dia::Editor;

static_assert(std::is_same<EditorActionParam, Dia::API::ValueParam>::value, "EditorActionParam must alias ValueParam");

namespace
{
	std::string Fast(const Json::Value& v)
	{
		Json::FastWriter w;
		std::string s = w.write(v);
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
		return s;
	}

	// Writes with FastWriter and re-parses; fails the test if the text is not valid JSON.
	Json::Value RoundTrip(const Json::Value& v)
	{
		const std::string text = Fast(v);
		Json::Reader reader;
		Json::Value parsed;
		EXPECT_TRUE(reader.parse(text, parsed)) << text;
		return parsed;
	}

	std::set<std::string> Keys(const Json::Value& v)
	{
		std::set<std::string> k;
		for (const std::string& m : v.getMemberNames()) k.insert(m);
		return k;
	}

	EditorActionParam Base()
	{
		EditorActionParam p;
		p.name = "n";
		p.type = "int";
		p.required = true;
		p.description = "d";
		return p;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Null / empty fields
////////////////////////////////////////////////////////////////////////////////
TEST(EditorActionParamJsonEdge, NullNameEmitsEmptyString)
{
	EditorActionParam p = Base();
	p.name = nullptr;
	const Json::Value j = EditorActionParamToJson(p);
	ASSERT_TRUE(j.isMember("name"));
	EXPECT_TRUE(j["name"].isString());
	EXPECT_EQ("", j["name"].asString());
}

TEST(EditorActionParamJsonEdge, AllNullFieldsStillValidJson)
{
	EditorActionParam p;  // every pointer null
	const Json::Value j = RoundTrip(EditorActionParamToJson(p));
	EXPECT_EQ("", j["name"].asString());
	EXPECT_EQ("string", j["type"].asString());   // null type defaults to "string"
	EXPECT_FALSE(j["required"].asBool());
	EXPECT_EQ("", j["description"].asString());
	EXPECT_EQ((std::set<std::string>{ "name", "type", "required", "description" }), Keys(j));
}

TEST(EditorActionParamJsonEdge, EmptyStringFieldsPreserved)
{
	EditorActionParam p = Base();
	p.name = "";
	p.type = "";
	p.description = "";
	const Json::Value j = RoundTrip(EditorActionParamToJson(p));
	EXPECT_EQ("", j["name"].asString());
	EXPECT_EQ("", j["type"].asString());          // empty (non-null) type is kept, not defaulted
	EXPECT_EQ("", j["description"].asString());
}

TEST(EditorActionParamJsonEdge, EmptyUnitIsOmitted)
{
	EditorActionParam p = Base();
	p.unit = "";
	EXPECT_FALSE(EditorActionParamToJson(p).isMember("unit"));
	p.unit = "m";
	EXPECT_EQ("m", EditorActionParamToJson(p)["unit"].asString());
}

// PIN: enumValues are emitted as the raw string whenever non-null, including "".
// (The palette emits a split array and omits empty values.)
TEST(EditorActionParamJsonEdge, PinValuesAreRawStringEvenWhenEmpty)
{
	EditorActionParam p = Base();
	p.type = "enum";
	p.enumValues = "a|b|c";
	Json::Value j = EditorActionParamToJson(p);
	ASSERT_TRUE(j["values"].isString());
	EXPECT_EQ("a|b|c", j["values"].asString());

	p.enumValues = "";
	j = EditorActionParamToJson(p);
	ASSERT_TRUE(j.isMember("values"));
	EXPECT_TRUE(j["values"].isString());
	EXPECT_EQ("", j["values"].asString());

	p.enumValues = nullptr;
	EXPECT_FALSE(EditorActionParamToJson(p).isMember("values"));
}

////////////////////////////////////////////////////////////////////////////////
// Range: non-finite / negative zero
////////////////////////////////////////////////////////////////////////////////
TEST(EditorActionParamJsonEdge, NonFiniteRangeIsOmittedAndJsonStaysValid)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();
	const float pairs[][2] = {
		{ nan, nan }, { nan, 1.0f }, { 0.0f, nan },
		{ inf, inf }, { -inf, inf }, { 0.0f, inf }, { -inf, 0.0f },
	};
	for (const auto& pr : pairs)
	{
		EditorActionParam p = Base();
		p.hasRange = true;
		p.min = pr[0];
		p.max = pr[1];
		const Json::Value direct = EditorActionParamToJson(p);
		EXPECT_FALSE(direct.isMember("min")) << pr[0] << ".." << pr[1];
		EXPECT_FALSE(direct.isMember("max")) << pr[0] << ".." << pr[1];

		const Json::Value parsed = RoundTrip(direct);
		EXPECT_FALSE(parsed.isMember("min"));
		EXPECT_FALSE(parsed.isMember("max"));
		EXPECT_EQ("n", parsed["name"].asString());
	}
}

TEST(EditorActionParamJsonEdge, NegativeZeroRangeIsEmittedAndRoundTrips)
{
	EditorActionParam p = Base();
	p.hasRange = true;
	p.min = -0.0f;
	p.max = 0.0f;
	const Json::Value parsed = RoundTrip(EditorActionParamToJson(p));
	ASSERT_TRUE(parsed.isMember("min"));
	ASSERT_TRUE(parsed.isMember("max"));
	EXPECT_EQ(0.0, parsed["min"].asDouble());
	EXPECT_EQ(0.0, parsed["max"].asDouble());
}

TEST(EditorActionParamJsonEdge, FiniteExtremeRangeRoundTrips)
{
	EditorActionParam p = Base();
	p.hasRange = true;
	p.min = -std::numeric_limits<float>::max();
	p.max = std::numeric_limits<float>::max();
	const Json::Value parsed = RoundTrip(EditorActionParamToJson(p));
	ASSERT_TRUE(parsed["min"].isNumeric());
	ASSERT_TRUE(parsed["max"].isNumeric());
	EXPECT_FLOAT_EQ(p.min, (float)parsed["min"].asDouble());
	EXPECT_FLOAT_EQ(p.max, (float)parsed["max"].asDouble());

	p.min = std::numeric_limits<float>::denorm_min();
	p.max = 5.0f;   // min <= max is not required; both are emitted verbatim
	const Json::Value parsed2 = RoundTrip(EditorActionParamToJson(p));
	EXPECT_TRUE(parsed2.isMember("min"));
	p.min = 9.0f;
	p.max = 1.0f;
	const Json::Value parsed3 = RoundTrip(EditorActionParamToJson(p));
	EXPECT_FLOAT_EQ(9.0f, (float)parsed3["min"].asDouble());
	EXPECT_FLOAT_EQ(1.0f, (float)parsed3["max"].asDouble());
}

TEST(EditorActionParamJsonEdge, RangeNotEmittedWithoutHasRangeEvenIfNonFinite)
{
	EditorActionParam p = Base();
	p.hasRange = false;
	p.min = std::numeric_limits<float>::quiet_NaN();
	p.max = std::numeric_limits<float>::infinity();
	const Json::Value j = RoundTrip(EditorActionParamToJson(p));
	EXPECT_FALSE(j.isMember("min"));
	EXPECT_FALSE(j.isMember("max"));
}

////////////////////////////////////////////////////////////////////////////////
// Special characters
////////////////////////////////////////////////////////////////////////////////
TEST(EditorActionParamJsonEdge, SpecialCharactersRoundTrip)
{
	const char* samples[] = {
		"quote\"inside",
		"back\\slash",
		"tab\there\nnewline\rcr",
		"ctrl\x01\x1f end",
		"slash/and</script>",
		"utf8 caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80",
	};
	for (const char* s : samples)
	{
		EditorActionParam p = Base();
		p.name = s;
		p.description = s;
		p.unit = s;
		p.enumValues = s;
		const Json::Value parsed = RoundTrip(EditorActionParamToJson(p));
		EXPECT_EQ(std::string(s), parsed["name"].asString());
		EXPECT_EQ(std::string(s), parsed["description"].asString());
		EXPECT_EQ(std::string(s), parsed["unit"].asString());
		EXPECT_EQ(std::string(s), parsed["values"].asString());
	}
}

TEST(EditorActionParamJsonEdge, LongStringsRoundTrip)
{
	const std::string longText(5000, 'x');
	EditorActionParam p = Base();
	p.description = longText.c_str();
	const Json::Value parsed = RoundTrip(EditorActionParamToJson(p));
	EXPECT_EQ(longText, parsed["description"].asString());
}

////////////////////////////////////////////////////////////////////////////////
// Legacy forms and key sets
////////////////////////////////////////////////////////////////////////////////
TEST(EditorActionParamJsonEdge, LegacyAggregateAndMemberAssignGiveSameJson)
{
	EditorActionParam agg = { "kind", "enum", true, "desc" };
	EditorActionParam mem;
	mem.name = "kind"; mem.type = "enum"; mem.required = true; mem.description = "desc";
	EXPECT_EQ(Fast(EditorActionParamToJson(mem)), Fast(EditorActionParamToJson(agg)));

	EditorActionParam two = { "n", "int" };
	const Json::Value j = EditorActionParamToJson(two);
	EXPECT_FALSE(j["required"].asBool());
	EXPECT_EQ("", j["description"].asString());
}

TEST(EditorActionParamJsonEdge, MinimalAndFullKeySets)
{
	const EditorActionParam minimal = Base();
	EXPECT_EQ((std::set<std::string>{ "name", "type", "required", "description" }),
	          Keys(EditorActionParamToJson(minimal)));

	EditorActionParam full = Base();
	full.unit = "m";
	full.hasRange = true;
	full.min = 1.0f;
	full.max = 2.0f;
	full.enumValues = "a|b";
	EXPECT_EQ((std::set<std::string>{ "name", "type", "required", "description", "unit", "min", "max", "values" }),
	          Keys(EditorActionParamToJson(full)));
}

// PIN: the editor manifest and the VisualDebugger palette ("args" in ConsoleJson) use the same
// key vocabulary {name,type,required,description,unit,min,max,values}, but diverge in:
//   - description: manifest ALWAYS emits it (empty string when missing); palette omits when empty.
//   - type: manifest defaults a null type to "string"; palette passes the type through.
//   - values: manifest emits the raw "a|b" string whenever non-null; palette emits a split array
//     and omits empty values.
// The palette side is DIA_DEBUG-only code in DiaVisualDebugger, so this test pins the manifest
// half of the divergence only.
TEST(EditorActionParamJsonEdge, PinManifestVsPaletteSchemaDivergence)
{
	EditorActionParam p = Base();
	p.description = nullptr;
	p.type = nullptr;
	p.enumValues = "a|b";
	const Json::Value j = EditorActionParamToJson(p);
	EXPECT_TRUE(j.isMember("description"));               // palette would omit
	EXPECT_EQ("string", j["type"].asString());             // palette would not default
	EXPECT_TRUE(j["values"].isString());                   // palette: array ["a","b"]
	EXPECT_FALSE(j["values"].isArray());

	const std::set<std::string> paletteVocabulary = { "name", "type", "required", "description", "unit", "min", "max", "values" };
	EditorActionParam full = Base();
	full.unit = "m"; full.hasRange = true; full.min = 0; full.max = 1; full.enumValues = "x";
	for (const std::string& k : Keys(EditorActionParamToJson(full)))
		EXPECT_EQ(1u, paletteVocabulary.count(k)) << k;
}

TEST(EditorActionParamJsonEdge, RequiredFlagBothValues)
{
	EditorActionParam p = Base();
	p.required = false;
	EXPECT_FALSE(EditorActionParamToJson(p)["required"].asBool());
	p.required = true;
	EXPECT_TRUE(EditorActionParamToJson(p)["required"].asBool());
	EXPECT_TRUE(EditorActionParamToJson(p)["required"].isBool());
}

TEST(EditorActionParamJsonEdge, ResultIsAlwaysAnObject)
{
	EditorActionParam p;
	EXPECT_TRUE(EditorActionParamToJson(p).isObject());
	EXPECT_TRUE(EditorActionParamToJson(Base()).isObject());
}
