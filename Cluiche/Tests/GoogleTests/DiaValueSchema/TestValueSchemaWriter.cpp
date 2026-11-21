#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaWriter.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <initializer_list>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstring>

// WriteDefaultsInPlace (parameter-files AC 22-25): only `default` tokens of named entries change.

using namespace Dia::ValueSchema;

namespace
{
	std::string Text(const ValueList<char>& out)
	{
		if (out.Size() == 0) { return std::string(); }
		return std::string(&out[0u], out.Size() - 1);	// trailing NUL excluded
	}

	DefaultValue Num(const char* key, double v) { DefaultValue d; d.mKey = key; d.mKind = DefaultKind::Float; d.mNumber = v; return d; }
	DefaultValue F32(const char* key, float v) { DefaultValue d; d.mKey = key; d.mKind = DefaultKind::Float32; d.mNumber = v; return d; }
	DefaultValue Int(const char* key, long long v) { DefaultValue d; d.mKey = key; d.mKind = DefaultKind::Int; d.mNumber = static_cast<double>(v); return d; }
	DefaultValue Bool(const char* key, bool v) { DefaultValue d; d.mKey = key; d.mKind = DefaultKind::Bool; d.mNumber = v ? 1.0 : 0.0; return d; }
	DefaultValue Str(const char* key, const char* v) { DefaultValue d; d.mKey = key; d.mKind = DefaultKind::String; d.mString = v; return d; }
	DefaultValue MapNum(const char* key, const char* mapKey, double v) { DefaultValue d = Num(key, v); d.mMapKey = mapKey; return d; }

	const char* kSample =
		"{\n"
		"  \"format\": \"dia.value_schema\",\n"
		"  \"version\": 1,\n"
		"  \"for\": \"t\",\n"
		"  \"entries\": [\n"
		"    { \"key\": \"a\", \"kind\": \"parameter\", \"label\": \"A\", \"description\": \"has default: 1 and {braces}\", \"type\": \"float\", \"min\": 0, \"max\": 10, \"default\": 0.5 },\n"
		"    {\n"
		"      \"key\": \"n\", \"kind\": \"parameter\", \"label\": \"N\", \"description\": \"D\", \"type\": \"int\",\n"
		"      \"default\"  :  3\n"
		"    },\n"
		"    { \"key\": \"b\", \"kind\": \"parameter\", \"label\": \"B\", \"description\": \"D\", \"type\": \"bool\", \"default\": true },\n"
		"    { \"key\": \"e\", \"kind\": \"parameter\", \"label\": \"E\", \"description\": \"D\", \"type\": \"enum\", \"values\": [{\"id\":\"X\",\"label\":\"X\"},{\"id\":\"Y\",\"label\":\"Y\"}], \"default\": \"X\" },\n"
		"    { \"key\": \"m\", \"kind\": \"parameter\", \"label\": \"M\", \"description\": \"D\", \"type\": \"map\", \"keyRef\": \"cow.biomes/biomes[].id\", \"valueType\": \"float\",\n"
		"      \"default\": { \"Desert\": 0.2,   \"Tundra\": 1 } },\n"
		"    { \"key\": \"nodef\", \"kind\": \"parameter\", \"label\": \"Z\", \"description\": \"D\", \"type\": \"float\" },\n"
		"    { \"key\": \"last\", \"kind\": \"parameter\", \"label\": \"L\", \"description\": \"D\", \"type\": \"float\", \"default\": -2.5 }\n"
		"  ]\n"
		"}\n";

	bool Write(const char* text, std::initializer_list<DefaultValue> vals, ValueList<char>& out, WriteError& err)
	{
		return WriteDefaultsInPlace(text, vals.begin(), static_cast<uint32_t>(vals.size()), out, err);
	}

	std::string Replace(std::string s, const std::string& from, const std::string& to)
	{
		const size_t at = s.find(from);
		EXPECT_NE(at, std::string::npos) << from;
		if (at != std::string::npos) { s.replace(at, from.size(), to); }
		return s;
	}
}

TEST(DiaValueSchemaWriter, UnchangedValuesRoundTripByteIdentical)
{
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(kSample, { Num("a", 0.5), Int("n", 3), Bool("b", true), Str("e", "X"),
		MapNum("m", "Desert", 0.2), MapNum("m", "Tundra", 1.0), Num("last", -2.5) }, out, err))
		<< err.mKey.AsCStr() << ": " << err.mReason.AsCStr();
	EXPECT_EQ(Text(out), std::string(kSample));
}

TEST(DiaValueSchemaWriter, NoValuesRoundTrips)
{
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(WriteDefaultsInPlace(kSample, nullptr, 0, out, err));
	EXPECT_EQ(Text(out), std::string(kSample));
}

TEST(DiaValueSchemaWriter, OnlyNamedDefaultTokensChange)
{
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(kSample, { Num("a", 7.25), Int("n", -4), Bool("b", false), Str("e", "Y"), Num("last", 0.0) }, out, err))
		<< err.mReason.AsCStr();
	std::string want = kSample;
	want = Replace(want, "\"default\": 0.5 }", "\"default\": 7.25 }");
	want = Replace(want, "\"default\"  :  3", "\"default\"  :  -4");
	want = Replace(want, "\"default\": true }", "\"default\": false }");
	want = Replace(want, "\"default\": \"X\" }", "\"default\": \"Y\" }");
	want = Replace(want, "\"default\": -2.5 }", "\"default\": 0.0 }");
	EXPECT_EQ(Text(out), want);
}

TEST(DiaValueSchemaWriter, MapDefaultsRewrittenPerKey)
{
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(kSample, { MapNum("m", "Tundra", 0.75) }, out, err)) << err.mReason.AsCStr();
	EXPECT_EQ(Text(out), Replace(kSample, "\"Tundra\": 1 }", "\"Tundra\": 0.75 }"));

	ASSERT_TRUE(Write(kSample, { MapNum("m", "Desert", 0.125), MapNum("m", "Tundra", 0.0) }, out, err));
	std::string want = Replace(kSample, "\"Desert\": 0.2,", "\"Desert\": 0.125,");
	want = Replace(want, "\"Tundra\": 1 }", "\"Tundra\": 0.0 }");
	EXPECT_EQ(Text(out), want);
}

TEST(DiaValueSchemaWriter, EntryWithNoDefaultFailsNamingKeyAndOutIsEmpty)
{
	ValueList<char> out; WriteError err;
	out.Reserve(4); out.Add('x');
	EXPECT_FALSE(Write(kSample, { Num("a", 1.0), Num("nodef", 2.0) }, out, err));
	EXPECT_STREQ(err.mKey.AsCStr(), "nodef");
	EXPECT_EQ(out.Size(), 0u);
}

TEST(DiaValueSchemaWriter, UnknownEntryFailsNamingKeyAndOutIsEmpty)
{
	ValueList<char> out; WriteError err;
	EXPECT_FALSE(Write(kSample, { Num("ghost", 1.0) }, out, err));
	EXPECT_STREQ(err.mKey.AsCStr(), "ghost");
	EXPECT_EQ(out.Size(), 0u);
}

TEST(DiaValueSchemaWriter, UnknownMapKeyFailsNamingEntryAndMapKey)
{
	ValueList<char> out; WriteError err;
	EXPECT_FALSE(Write(kSample, { MapNum("m", "Swamp", 1.0) }, out, err));
	EXPECT_NE(std::string(err.mKey.AsCStr()).find("m"), std::string::npos);
	EXPECT_NE(std::string(err.mKey.AsCStr()).find("Swamp"), std::string::npos);
	EXPECT_EQ(out.Size(), 0u);
}

TEST(DiaValueSchemaWriter, MapValueOnScalarAndScalarOnMapFail)
{
	ValueList<char> out; WriteError err;
	EXPECT_FALSE(Write(kSample, { MapNum("a", "Desert", 1.0) }, out, err));
	EXPECT_STREQ(err.mKey.AsCStr(), "a.Desert");
	EXPECT_FALSE(Write(kSample, { Num("m", 1.0) }, out, err));
	EXPECT_STREQ(err.mKey.AsCStr(), "m");
	EXPECT_EQ(out.Size(), 0u);
}

TEST(DiaValueSchemaWriter, DuplicateTargetFails)
{
	ValueList<char> out; WriteError err;
	EXPECT_FALSE(Write(kSample, { Num("a", 1.0), Num("a", 2.0) }, out, err));
	EXPECT_STREQ(err.mKey.AsCStr(), "a");
	EXPECT_EQ(out.Size(), 0u);
}

TEST(DiaValueSchemaWriter, UnparsableTextFailsWithEmptyOut)
{
	ValueList<char> out; WriteError err;
	EXPECT_FALSE(WriteDefaultsInPlace("{ \"entries\": [ { \"key\": ", nullptr, 0, out, err));
	EXPECT_EQ(out.Size(), 0u);
	EXPECT_FALSE(WriteDefaultsInPlace(nullptr, nullptr, 0, out, err));
	EXPECT_FALSE(WriteDefaultsInPlace("", nullptr, 0, out, err));
}

TEST(DiaValueSchemaWriter, NonFiniteNumberFailsNamingKey)
{
	ValueList<char> out; WriteError err;
	EXPECT_FALSE(Write(kSample, { Num("a", std::nan("")) }, out, err));
	EXPECT_STREQ(err.mKey.AsCStr(), "a");
	EXPECT_FALSE(Write(kSample, { Num("a", INFINITY) }, out, err));
	EXPECT_EQ(out.Size(), 0u);
}

TEST(DiaValueSchemaWriter, NumberFormattingRoundTripsExactly)
{
	const double doubles[] = { 0.1, -0.1, 1.0 / 3.0, 1e-300, 1.7976931348623157e308, 123456789.125, -7.0, 5e-324 };
	for (double d : doubles)
	{
		ValueList<char> out; WriteError err;
		ASSERT_TRUE(Write(kSample, { Num("last", d) }, out, err));
		const std::string t = Text(out);
		const size_t at = t.rfind("\"default\": ");
		ASSERT_NE(at, std::string::npos);
		EXPECT_EQ(strtod(t.c_str() + at + 11, nullptr), d) << t.substr(at, 40);
	}
}

TEST(DiaValueSchemaWriter, Float32UsesShortestFormThatRoundTripsAsFloat)
{
	const float fs[] = { 0.1f, 0.3f, 1.0f / 3.0f, 16777216.0f, 1e-30f };
	for (float f : fs)
	{
		ValueList<char> out; WriteError err;
		ASSERT_TRUE(Write(kSample, { F32("last", f) }, out, err));
		const std::string t = Text(out);
		const size_t at = t.rfind("\"default\": ");
		ASSERT_NE(at, std::string::npos);
		EXPECT_EQ(strtof(t.c_str() + at + 11, nullptr), f) << t.substr(at, 40);
	}
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(kSample, { F32("last", 0.1f) }, out, err));
	EXPECT_NE(Text(out).find("\"default\": 0.1 }"), std::string::npos);	// not 0.10000000149011612
}

TEST(DiaValueSchemaWriter, NegativeZeroIsWrittenAndDistinctFromZero)
{
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(kSample, { Num("last", -0.0) }, out, err));
	EXPECT_NE(Text(out).find("\"default\": -0"), std::string::npos);
	// Unchanged -0.0 stays byte-identical; changing 0.0 to -0.0 is a change.
	const std::string src = Replace(kSample, "\"default\": -2.5 }", "\"default\": -0.0 }");
	ASSERT_TRUE(Write(src.c_str(), { Num("last", -0.0) }, out, err));
	EXPECT_EQ(Text(out), src);
	const std::string pos = Replace(kSample, "\"default\": -2.5 }", "\"default\": 0.0 }");
	ASSERT_TRUE(Write(pos.c_str(), { Num("last", -0.0) }, out, err));
	EXPECT_NE(Text(out), pos);
}

TEST(DiaValueSchemaWriter, IntAndFloatFormatting)
{
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(kSample, { Int("n", 12), Num("a", 2.0) }, out, err));
	const std::string t = Text(out);
	EXPECT_NE(t.find("\"default\"  :  12\n"), std::string::npos);	// int has no decimal point
	EXPECT_NE(t.find("\"default\": 2.0 }"), std::string::npos);		// float keeps a decimal point
	ASSERT_TRUE(Write(kSample, { Int("n", -9000000000LL) }, out, err));
	EXPECT_NE(Text(out).find("-9000000000"), std::string::npos);
}

TEST(DiaValueSchemaWriter, EquivalentSpellingsAreKeptWhenValueIsUnchanged)
{
	// "5e-1" for 0.5, "3.0" for an int 3, "-2.50": same value, so the original token survives.
	std::string src = Replace(kSample, "\"default\": 0.5 }", "\"default\": 5e-1 }");
	src = Replace(src, "\"default\"  :  3", "\"default\"  :  3.0");
	src = Replace(src, "\"default\": -2.5 }", "\"default\": -2.50 }");
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(src.c_str(), { Num("a", 0.5), Int("n", 3), Num("last", -2.5) }, out, err));
	EXPECT_EQ(Text(out), src);
}

TEST(DiaValueSchemaWriter, StringValueWithDefaultTextAndBraces)
{
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(kSample, { Str("e", "Y \"default\": {x} \\ \n") }, out, err));
	const std::string t = Text(out);
	EXPECT_NE(t.find("\"default\": \"Y \\\"default\\\": {x} \\\\ \\n\" }"), std::string::npos) << t;
	// The result is still a document the scan accepts, and an unchanged write keeps it as is.
	ValueList<char> out2;
	ASSERT_TRUE(Write(t.c_str(), { Str("e", "Y \"default\": {x} \\ \n") }, out2, err));
	EXPECT_EQ(Text(out2), t);
}

TEST(DiaValueSchemaWriter, DefaultInsideDescriptionIsNotTouched)
{
	const std::string src =
		"{\"entries\":[{\"key\":\"a\",\"description\":\"\\\"default\\\": 9, \\\"key\\\": \\\"b\\\"\",\"type\":\"float\",\"default\":1}]}";
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(src.c_str(), { Num("a", 4.0) }, out, err));
	EXPECT_EQ(Text(out), Replace(src, "\"default\":1}", "\"default\":4.0}"));
	EXPECT_FALSE(Write(src.c_str(), { Num("b", 4.0) }, out, err));
}

TEST(DiaValueSchemaWriter, DefaultInNestedBlockIsNotTheEntryDefault)
{
	const std::string src =
		"{\"entries\":[{\"key\":\"a\",\"ui\":{\"default\":7,\"nested\":{\"default\":8}},\"type\":\"float\",\"default\":1}]}";
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(src.c_str(), { Num("a", 4.0) }, out, err));
	EXPECT_EQ(Text(out), Replace(src, "\"default\":1}", "\"default\":4.0}"));
}

TEST(DiaValueSchemaWriter, NestedObjectDefaultOnlyKeysOfRequestedMapChange)
{
	const std::string src =
		"{\"entries\":[{\"key\":\"m\",\"type\":\"map\",\"default\":{\"A\":{\"x\":1},\"B\":2}}]}";
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(src.c_str(), { MapNum("m", "B", 5.0) }, out, err));
	EXPECT_EQ(Text(out), Replace(src, "\"B\":2}", "\"B\":5.0}"));
}

TEST(DiaValueSchemaWriter, CrlfLineEndingsArePreserved)
{
	std::string src;
	for (const char* p = kSample; *p; ++p) { if (*p == '\n') { src += "\r\n"; } else { src += *p; } }
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(src.c_str(), { Num("a", 0.5), Int("n", 3) }, out, err));
	EXPECT_EQ(Text(out), src);
	ASSERT_TRUE(Write(src.c_str(), { Int("n", 4) }, out, err));
	EXPECT_EQ(Text(out), Replace(src, "\"default\"  :  3", "\"default\"  :  4"));
}

TEST(DiaValueSchemaWriter, MinifiedInputAndLastEntry)
{
	const std::string src =
		"{\"format\":\"dia.value_schema\",\"entries\":[{\"key\":\"a\",\"default\":1},{\"key\":\"z\",\"type\":\"float\",\"default\":2}]}";
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(src.c_str(), { Num("z", 3.5) }, out, err));
	EXPECT_EQ(Text(out), Replace(src, "\"default\":2}", "\"default\":3.5}"));
	ASSERT_TRUE(Write(src.c_str(), { Num("a", 1.0), Num("z", 2.0) }, out, err));
	EXPECT_EQ(Text(out), src);
}

TEST(DiaValueSchemaWriter, DefaultIsFoundWhenKeyComesAfterDefault)
{
	const std::string src = "{\"entries\":[{\"default\":1,\"key\":\"a\"}]}";
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(src.c_str(), { Num("a", 9.0) }, out, err));
	EXPECT_EQ(Text(out), "{\"entries\":[{\"default\":9.0,\"key\":\"a\"}]}");
}

TEST(DiaValueSchemaWriter, CommentsAndTopLevelKeyOrderPreserved)
{
	const std::string src =
		"// header\n{ /* c */ \"entries\" : [ // e\n { \"key\":\"a\", /* mid */ \"default\" : /* v */ 1 /* after */ } ], \"for\":\"t\" }\n";
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(src.c_str(), { Num("a", 2.0) }, out, err)) << err.mReason.AsCStr();
	EXPECT_EQ(Text(out), Replace(src, "1 /* after */", "2.0 /* after */"));
}

TEST(DiaValueSchemaWriter, WrittenSidecarStillLoadsWithNewDefaults)
{
	ValueList<char> out; WriteError err;
	ASSERT_TRUE(Write(kSample, { Num("a", 7.25), Int("n", 4), Bool("b", false), Str("e", "Y"), MapNum("m", "Desert", 0.5) }, out, err));
	const LoadResult r = ValueSchemaLoader::LoadFromText(Text(out).c_str(), "w.schema.json");
	ASSERT_TRUE(r.mOk) << r.mFailure.mReason.AsCStr();
	EXPECT_DOUBLE_EQ(r.mEntries[0].mDefaultNumber, 7.25);
}
