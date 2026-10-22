#include <gtest/gtest.h>
#include <DiaValueSchema/RampData.h>
#include <DiaValueSchema/RampLoader.h>
#include "ValueSchemaTestSupport.h"
#include <cmath>
#include <cstring>
#include <random>
#include <string>
#include <type_traits>

// RampLoader / ParseHexColour / RampData exhaustive tests.
//
// PINs: ramp stops whose t values differ as doubles but are equal as floats are accepted (stored equal); a "-0.0"
// first stop is accepted; ramps with both `stops` and `colours` are rejected as unknown keys.

using namespace Dia::ValueSchema;
using Dia::Core::StringCRC;

namespace
{
	const char* kGood = R"({"id":"good.one","kind":"continuous","stops":[{"t":0.0,"colour":"#000000"},{"t":1.0,"colour":"#ffffff"}]})";
	const char* kCat = R"("kind":"categorical","colours":["#000000","#ffffff"])";

	std::string File(const std::string& ramps)
	{
		return std::string(R"({"format":"dia.ramps","version":1,"ramps":[)") + ramps + "]}";
	}

	RampLoadResult Load(const std::string& text, const char* file = "edge.diaramps") { return RampLoader::LoadFromText(text.c_str(), file); }

	std::string StopsRamp(const std::string& id, int n)
	{
		std::string s = "{\"id\":\"" + id + R"(","kind":"continuous","stops":[)";
		for (int i = 0; i < n; ++i)
		{
			if (i) { s += ","; }
			char buf[64];
			const double t = (n == 1) ? 0.0 : double(i) / double(n - 1);
			snprintf(buf, sizeof(buf), R"({"t":%.6f,"colour":"#102030"})", t);
			s += buf;
		}
		return s + "]}";
	}

	std::string ColoursRamp(const std::string& id, int n)
	{
		std::string s = "{\"id\":\"" + id + R"(","kind":"categorical","colours":[)";
		for (int i = 0; i < n; ++i) { if (i) { s += ","; } s += "\"#0a0b0c\""; }
		return s + "]}";
	}

	std::string CatRamp(const std::string& id) { return "{\"id\":" + Json::valueToQuotedString(id.c_str()) + "," + kCat + "}"; }

	bool Mentions(const RampLoadResult& r, unsigned index, const char* needle) { return VsTest::Contains(r.mRampErrors[index].AsCStr(), needle); }

	std::string RampsJson()
	{
		const auto p = VsTest::FindUpwards(__FILE__, "Dia/DiaValueSchema/Data/ramps.json");
		std::string text;
		if (!p.empty()) { VsTest::ReadFileBytes(p, text); }
		return text;
	}
}

// ---- ParseHexColour ------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaParseHex, RejectsMalformedAndLeavesOutParamUntouched)
{
	const char* bad[] = { "", "#", "#1", "#12345", "#1234567", "#12345678", "123456", "#12345g", "#gggggg", "# 12345", "#12 456", " #123456", "#123456 ", "#123456\n",
		"#12345\xE9", "0x123456", "#-12345", "#+12345", "##12345" };
	for (const char* s : bad)
	{
		SCOPED_TRACE(s);
		uint32_t out = 0xDEADBEEF;
		EXPECT_FALSE(ParseHexColour(s, out));
		EXPECT_EQ(out, 0xDEADBEEFu);
	}
	uint32_t out = 0xDEADBEEF;
	EXPECT_FALSE(ParseHexColour(nullptr, out));
	EXPECT_EQ(out, 0xDEADBEEFu);
}

TEST(DiaValueSchemaParseHex, EmbeddedNulCutsTheString)
{
	uint32_t out = 0xDEADBEEF;
	EXPECT_FALSE(ParseHexColour("#12\0" "3456", out));	// the C string is "#12"
	EXPECT_EQ(out, 0xDEADBEEFu);
	EXPECT_TRUE(ParseHexColour("#123456\0" "78", out));	// the C string is "#123456"
	EXPECT_EQ(out, 0x123456FFu);
}

TEST(DiaValueSchemaParseHex, AcceptsMixedCaseAndPacksRgbaWithOpaqueAlpha)
{
	uint32_t out = 0;
	ASSERT_TRUE(ParseHexColour("#aBcDeF", out));
	EXPECT_EQ(out, 0xABCDEFFFu);
	ASSERT_TRUE(ParseHexColour("#000000", out));
	EXPECT_EQ(out, 0x000000FFu);
	ASSERT_TRUE(ParseHexColour("#FFFFFF", out));
	EXPECT_EQ(out, 0xFFFFFFFFu);
	ASSERT_TRUE(ParseHexColour("#ffffff", out));
	EXPECT_EQ(out, 0xFFFFFFFFu);
}

TEST(DiaValueSchemaParseHex, PropertyRoundTripBothCases)
{
	std::mt19937 rng(1234);
	for (int i = 0; i < 20000; ++i)
	{
		const uint32_t rgb = rng() & 0xFFFFFFu;
		char lower[16], upper[16];
		snprintf(lower, sizeof(lower), "#%06x", rgb);
		snprintf(upper, sizeof(upper), "#%06X", rgb);
		uint32_t a = 0, b = 0;
		ASSERT_TRUE(ParseHexColour(lower, a)) << lower;
		ASSERT_TRUE(ParseHexColour(upper, b)) << upper;
		ASSERT_EQ(a, (rgb << 8) | 0xFFu);
		ASSERT_EQ(a, b);
	}
}

// ---- RampData ------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampData, DefaultsAndConstants)
{
	RampData d;
	EXPECT_EQ(d.mStopCount, 0u);
	EXPECT_EQ(d.mColourCount, 0u);
	EXPECT_EQ(d.mKind, RampKind::Continuous);
	EXPECT_EQ(kMaxRampStops, 16u);
	EXPECT_EQ(kMaxCategoricalColours, 32u);
	for (const RampStop& s : d.mStops) { EXPECT_EQ(s.mT, 0.0f); EXPECT_EQ(s.mRgba8, 0u); }
	for (uint32_t c : d.mColours) { EXPECT_EQ(c, 0u); }
	EXPECT_STREQ(d.mName.AsCStr(), "");
}

TEST(DiaValueSchemaRampData, SizeIsSmallAndCopyIsIndependent)
{
	EXPECT_EQ(sizeof(RampStop), 8u);
	EXPECT_LT(sizeof(RampData), 512u) << "plain data, cheap to copy";
	EXPECT_TRUE(std::is_copy_constructible<RampData>::value);

	RampLoadResult r = Load(File(std::string(kGood)));
	ASSERT_EQ(r.mRamps.Size(), 1u);
	RampData copy = r.mRamps[0];
	EXPECT_EQ(memcmp(&copy.mStops, &r.mRamps[0].mStops, sizeof(copy.mStops)), 0);
	copy.mStops[0].mRgba8 = 0x11223344u;
	copy.mName = "changed";
	EXPECT_EQ(r.mRamps[0].mStops[0].mRgba8, 0x000000FFu);
	EXPECT_STREQ(r.mRamps[0].mName.AsCStr(), "good.one");
	RampData assigned;
	assigned = r.mRamps[0];
	EXPECT_EQ(assigned.mId.Value(), StringCRC("good.one").Value());
}

// ---- counts --------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampEdge, StopCountBoundaries)
{
	for (int n : { 0, 1, 2, 16, 17, 100 })
	{
		SCOPED_TRACE(n);
		const RampLoadResult r = Load(File(StopsRamp("s", n)));
		ASSERT_TRUE(r.mOk);
		const bool valid = (n >= 2 && n <= 16);
		EXPECT_EQ(r.mRamps.Size(), valid ? 1u : 0u);
		EXPECT_EQ(r.mRampsRejected, valid ? 0u : 1u);
		if (valid) { EXPECT_EQ(r.mRamps[0].mStopCount, static_cast<unsigned>(n)); }
		else { EXPECT_TRUE(Mentions(r, 0, "2 to 16 stops")) << r.mRampErrors[0].AsCStr(); }
	}
}

TEST(DiaValueSchemaRampEdge, ColourCountBoundaries)
{
	for (int n : { 0, 1, 2, 32, 33, 100 })
	{
		SCOPED_TRACE(n);
		const RampLoadResult r = Load(File(ColoursRamp("c", n)));
		ASSERT_TRUE(r.mOk);
		const bool valid = (n >= 2 && n <= 32);
		EXPECT_EQ(r.mRamps.Size(), valid ? 1u : 0u);
		if (valid)
		{
			EXPECT_EQ(r.mRamps[0].mColourCount, static_cast<unsigned>(n));
			EXPECT_EQ(r.mRamps[0].mColours[n - 1], 0x0A0B0CFFu);
			EXPECT_EQ(r.mRamps[0].mKind, RampKind::Categorical);
		}
		else { EXPECT_TRUE(Mentions(r, 0, "2 to 32 colours")) << r.mRampErrors[0].AsCStr(); }
	}
}

// ---- t values ------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampEdge, PinTValuesDistinctAsDoublesButEqualAsFloatsAreAccepted)
{
	const std::string ramp = R"({"id":"near","kind":"continuous","stops":[{"t":0,"colour":"#000000"},{"t":0.5,"colour":"#111111"},{"t":0.5000000000000001,"colour":"#222222"},{"t":1,"colour":"#ffffff"}]})";
	const RampLoadResult r = Load(File(ramp));
	ASSERT_TRUE(r.mOk);
	ASSERT_EQ(r.mRamps.Size(), 1u);
	EXPECT_EQ(r.mRamps[0].mStops[1].mT, r.mRamps[0].mStops[2].mT);
}

TEST(DiaValueSchemaRampEdge, EqualAndDecreasingTAreRejected)
{
	const RampLoadResult r = Load(File(
		R"({"id":"eq","kind":"continuous","stops":[{"t":0,"colour":"#000000"},{"t":0.5,"colour":"#111111"},{"t":0.5,"colour":"#222222"},{"t":1,"colour":"#ffffff"}]},)"
		R"({"id":"dec","kind":"continuous","stops":[{"t":0,"colour":"#000000"},{"t":0.7,"colour":"#111111"},{"t":0.4,"colour":"#222222"},{"t":1,"colour":"#ffffff"}]})"));
	ASSERT_TRUE(r.mOk);
	EXPECT_EQ(r.mRamps.Size(), 0u);
	EXPECT_EQ(r.mRampsRejected, 2u);
	EXPECT_TRUE(Mentions(r, 0, "strictly increasing"));
	EXPECT_TRUE(Mentions(r, 1, "strictly increasing"));
}

TEST(DiaValueSchemaRampEdge, PinNegativeZeroFirstStopIsAccepted)
{
	const RampLoadResult r = Load(File(R"({"id":"nz","kind":"continuous","stops":[{"t":-0.0,"colour":"#000000"},{"t":1,"colour":"#ffffff"}]})"));
	ASSERT_TRUE(r.mOk);
	ASSERT_EQ(r.mRamps.Size(), 1u);
	EXPECT_EQ(r.mRamps[0].mStops[0].mT, 0.0f);
}

TEST(DiaValueSchemaRampEdge, TOutsideUnitRangeAndNonFiniteRejected)
{
	for (const char* firstT : { "-0.1", "-1", "1e-300", "NaN" })
	{
		SCOPED_TRACE(firstT);
		const std::string ramp = std::string(R"({"id":"x","kind":"continuous","stops":[{"t":)") + firstT + R"(,"colour":"#000000"},{"t":1,"colour":"#ffffff"}]})";
		const RampLoadResult r = Load(File(ramp));
		if (r.mOk) { EXPECT_EQ(r.mRamps.Size(), 0u); }
	}
	const RampLoadResult over = Load(File(R"({"id":"x","kind":"continuous","stops":[{"t":0,"colour":"#000000"},{"t":1.0000001,"colour":"#ffffff"}]})"));
	ASSERT_TRUE(over.mOk);
	EXPECT_EQ(over.mRamps.Size(), 0u);
	const RampLoadResult tooBig = Load(File(R"({"id":"x","kind":"continuous","stops":[{"t":0,"colour":"#000000"},{"t":2,"colour":"#ffffff"}]})"));
	EXPECT_EQ(tooBig.mRamps.Size(), 0u);
}

// ---- ids -----------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampEdge, IdForms)
{
	struct IdCase { std::string mId; bool mValid; };
	const IdCase cases[] = {
		{ "a", true }, { "a.b.c", true }, { "a_b.c9", true }, { "9.0", true }, { "_", true },
		{ "a..b", false }, { ".a", false }, { "a.", false }, { ".", false }, { "..", false },
		{ "Upper", false }, { "aB", false }, { "a-b", false }, { "a b", false }, { "a/b", false },
		{ "caf\xC3\xA9", false }, { "\xE2\x82\xAC", false }, { "a\tb", false },
		{ std::string(63, 'a'), true }, { std::string(64, 'a'), false },
		{ std::string(31, 'a') + "." + std::string(31, 'b'), true },
	};
	for (const IdCase& c : cases)
	{
		SCOPED_TRACE(c.mId);
		const RampLoadResult r = Load(File(CatRamp(c.mId)));
		ASSERT_TRUE(r.mOk);
		EXPECT_EQ(r.mRamps.Size(), c.mValid ? 1u : 0u);
		EXPECT_EQ(r.mRampsRejected, c.mValid ? 0u : 1u);
		if (c.mValid) { EXPECT_STREQ(r.mRamps[0].mName.AsCStr(), c.mId.c_str()); EXPECT_EQ(r.mRamps[0].mId.Value(), StringCRC(c.mId.c_str()).Value()); }
	}
}

TEST(DiaValueSchemaRampEdge, EmptyIdUsesTheIndexInTheDiagnostic)
{
	const RampLoadResult r = Load(File(std::string(kGood) + "," + CatRamp("")));
	ASSERT_EQ(r.mRampErrors.Size(), 1u);
	EXPECT_TRUE(Mentions(r, 0, "#1")) << r.mRampErrors[0].AsCStr();
}

TEST(DiaValueSchemaRampEdge, IdLongerThanHundredCharsUsesTheIndexNotTheId)
{
	const std::string id101(101, 'a');
	const std::string id100(100, 'a');
	const RampLoadResult r = Load(File(CatRamp(id101) + "," + CatRamp(id100)));
	ASSERT_TRUE(r.mOk);
	ASSERT_EQ(r.mRampErrors.Size(), 2u);
	EXPECT_TRUE(Mentions(r, 0, "'#0'")) << r.mRampErrors[0].AsCStr();
	EXPECT_FALSE(Mentions(r, 0, id101.c_str()));
	EXPECT_TRUE(Mentions(r, 1, id100.c_str())) << "100 characters are still echoed";
	EXPECT_LT(strlen(r.mRampErrors[1].AsCStr()), 512u);
}

TEST(DiaValueSchemaRampEdge, DuplicateIdAfterInvalidFirstLoadsTheSecond)
{
	const std::string badFirst = R"({"id":"dup","kind":"categorical","colours":["#000000"]})";
	const std::string goodSecond = CatRamp("dup");
	const RampLoadResult r = Load(File(badFirst + "," + goodSecond));
	ASSERT_TRUE(r.mOk);
	ASSERT_EQ(r.mRamps.Size(), 1u);
	EXPECT_EQ(r.mRampsRejected, 1u);

	const RampLoadResult dup = Load(File(goodSecond + "," + goodSecond + "," + goodSecond));
	ASSERT_EQ(dup.mRamps.Size(), 1u);
	EXPECT_EQ(dup.mRampsRejected, 2u);
	EXPECT_TRUE(Mentions(dup, 0, "duplicate id"));
}

TEST(DiaValueSchemaRampEdge, MoreThanSixteenRejectedRampsAreAllReported)
{
	std::string ramps;
	for (int i = 0; i < 40; ++i) { ramps += "{\"id\":\"Bad" + std::to_string(i) + "\"," + kCat + "},"; }
	ramps += kGood;
	const RampLoadResult r = Load(File(ramps));
	ASSERT_TRUE(r.mOk);
	EXPECT_EQ(r.mRampsRejected, 40u);
	EXPECT_EQ(r.mRampErrors.Size(), 40u);
	ASSERT_EQ(r.mRamps.Size(), 1u);
	EXPECT_STREQ(r.mRamps[0].mName.AsCStr(), "good.one");
}

TEST(DiaValueSchemaRampEdge, ManyValidRampsKeepFileOrder)
{
	std::string ramps;
	for (int i = 0; i < 500; ++i) { if (i) { ramps += ","; } ramps += CatRamp("r" + std::to_string(i)); }
	const RampLoadResult r = Load(File(ramps));
	ASSERT_TRUE(r.mOk);
	ASSERT_EQ(r.mRamps.Size(), 500u);
	EXPECT_STREQ(r.mRamps[499].mName.AsCStr(), "r499");
	EXPECT_STREQ(r.mRamps[0].mName.AsCStr(), "r0");
}

// ---- shape errors --------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampEdge, StopsAndColoursTogetherAreRejected)
{
	const RampLoadResult a = Load(File(R"({"id":"both","kind":"continuous","stops":[{"t":0,"colour":"#000000"},{"t":1,"colour":"#ffffff"}],"colours":["#000000","#ffffff"]})"));
	ASSERT_TRUE(a.mOk);
	EXPECT_EQ(a.mRamps.Size(), 0u);
	EXPECT_TRUE(Mentions(a, 0, "unknown key 'colours'"));
	const RampLoadResult b = Load(File(R"({"id":"both","kind":"categorical","colours":["#000000","#ffffff"],"stops":[{"t":0,"colour":"#000000"},{"t":1,"colour":"#ffffff"}]})"));
	EXPECT_EQ(b.mRamps.Size(), 0u);
	EXPECT_TRUE(Mentions(b, 0, "unknown key 'stops'"));
}

TEST(DiaValueSchemaRampEdge, UnknownStopKeyAndNullColourAreRejected)
{
	const RampLoadResult r = Load(File(
		R"({"id":"a","kind":"continuous","stops":[{"t":0,"colour":"#000000","extra":1},{"t":1,"colour":"#ffffff"}]},)"
		R"({"id":"b","kind":"continuous","stops":[{"t":0,"colour":null},{"t":1,"colour":"#ffffff"}]},)"
		R"({"id":"c","kind":"categorical","colours":["#000000",null]},)"
		R"({"id":"d","kind":"continuous","stops":[{"t":0,"colour":"#000000"},{"colour":"#ffffff"}]},)"
		R"({"id":"e","kind":"continuous","stops":[{"t":"0","colour":"#000000"},{"t":1,"colour":"#ffffff"}]},)"
		R"({"id":"f","kind":"continuous","stops":[0,1]})"));
	ASSERT_TRUE(r.mOk);
	EXPECT_EQ(r.mRamps.Size(), 0u);
	EXPECT_EQ(r.mRampsRejected, 6u);
	EXPECT_TRUE(Mentions(r, 0, "unknown key 'extra'"));
	EXPECT_TRUE(Mentions(r, 1, "must be a string"));
	EXPECT_TRUE(Mentions(r, 2, "must be a string"));
}

TEST(DiaValueSchemaRampEdge, ShortHexAndRgbaColoursAreRejected)
{
	for (const char* colour : { "#fff", "#12345678", "ffffff", "#GGGGGG", "" })
	{
		SCOPED_TRACE(colour);
		const std::string ramp = std::string(R"({"id":"x","kind":"categorical","colours":["#000000",")") + colour + "\"]}";
		const RampLoadResult r = Load(File(ramp));
		ASSERT_TRUE(r.mOk);
		EXPECT_EQ(r.mRamps.Size(), 0u);
	}
}

// ---- file-level text -----------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampEdge, Utf8BomIsAccepted)
{
	const RampLoadResult r = Load(std::string("\xEF\xBB\xBF") + File(kGood));
	EXPECT_TRUE(r.mOk) << r.mFileReason.AsCStr();
	EXPECT_EQ(r.mRamps.Size(), 1u);
}

TEST(DiaValueSchemaRampEdge, BomOnlyAndDoubleBomAreRejected)
{
	EXPECT_FALSE(Load("\xEF\xBB\xBF").mOk);
	EXPECT_FALSE(Load(std::string("\xEF\xBB\xBF\xEF\xBB\xBF") + File(kGood)).mOk);
}

TEST(DiaValueSchemaRampEdge, CrLfLoads)
{
	Json::Value doc;
	Json::Reader().parse(File(kGood), doc);
	const std::string pretty = Json::StyledWriter().write(doc);
	std::string crlf;
	for (char c : pretty) { if (c == '\n') { crlf += "\r\n"; } else { crlf += c; } }
	const RampLoadResult r = Load(crlf);
	EXPECT_TRUE(r.mOk) << r.mFileReason.AsCStr();
	EXPECT_EQ(r.mRamps.Size(), 1u);
}

TEST(DiaValueSchemaRampEdge, EmptyAndNonRampDocumentsAreRejected)
{
	for (const char* d : { "", "  ", "{}", "[]", "null", "\"x\"", "{\"format\":\"dia.ramps\"}", "{\"format\":\"dia.ramps\",\"version\":1}", "{" })
	{
		SCOPED_TRACE(d);
		const RampLoadResult r = Load(d);
		EXPECT_FALSE(r.mOk);
		EXPECT_EQ(r.mRamps.Size(), 0u);
		EXPECT_GT(strlen(r.mFileReason.AsCStr()), 0u);
	}
}

TEST(DiaValueSchemaRampEdge, VersionVariants)
{
	for (const char* v : { "2", "0", "-1", "1.5", "\"1\"", "4294967297", "null", "true" })
	{
		SCOPED_TRACE(v);
		const std::string text = std::string(R"({"format":"dia.ramps","version":)") + v + R"(,"ramps":[]})";
		EXPECT_FALSE(Load(text).mOk);
	}
	EXPECT_TRUE(Load(R"({"format":"dia.ramps","version":1,"ramps":[]})").mOk);
	EXPECT_TRUE(Load(R"({"format":"dia.ramps","version":1.0,"ramps":[]})").mOk);	// PIN: integral doubles pass isInt()
}

TEST(DiaValueSchemaRampEdge, ExcessiveNestingIsRejectedCleanly)
{
	std::string text = R"({"format":"dia.ramps","version":1,"ramps":[{"id":"x","kind":"categorical","colours":["#000000","#ffffff"],"x":)";
	for (int i = 0; i < 100000; ++i) { text += "["; }
	for (int i = 0; i < 100000; ++i) { text += "]"; }
	text += "}]}";
	const RampLoadResult r = Load(text);
	EXPECT_FALSE(r.mOk);
	EXPECT_TRUE(VsTest::Contains(r.mFileReason.AsCStr(), "unparsable JSON")) << r.mFileReason.AsCStr();
	EXPECT_TRUE(Load(File(kGood)).mOk);
}

TEST(DiaValueSchemaRampEdge, FileNameLongerThanCapacityFallsBackAndStillLoads)
{
	VsTest::AssertCounter asserts;
	for (size_t n : { 255u, 256u, 300u, 5000u })
	{
		SCOPED_TRACE(n);
		const std::string name(n, 'f');
		const RampLoadResult r = Load(File(kGood), name.c_str());
		ASSERT_TRUE(r.mOk);
		if (n <= 255) { EXPECT_STREQ(r.mFile.AsCStr(), name.c_str()); }
		else { EXPECT_STREQ(r.mFile.AsCStr(), "<file name too long>"); }
	}
	const RampLoadResult bad = Load("nope", std::string(300, 'f').c_str());
	EXPECT_FALSE(bad.mOk);
	EXPECT_STREQ(bad.mFile.AsCStr(), "<file name too long>");
	EXPECT_EQ(asserts.Count(), 0);
}

TEST(DiaValueSchemaRampEdge, NullFileNameAndNullTextAreHandled)
{
	// Hardening: previously strlen(nullptr) / std::string(nullptr) crashed.
	const RampLoadResult nullName = RampLoader::LoadFromText(File(kGood).c_str(), nullptr);
	EXPECT_TRUE(nullName.mOk);
	const RampLoadResult nullText = RampLoader::LoadFromText(nullptr, "x");
	EXPECT_FALSE(nullText.mOk);
	EXPECT_FALSE(RampLoader::LoadFromText(nullptr, nullptr).mOk);
}

TEST(DiaValueSchemaRampEdge, FormatSpecifiersInIdsAndFileNamesAreInert)
{
	VsTest::AssertCounter asserts;
	const RampLoadResult r = Load(File("{\"id\":\"%s%n%d\",\"kind\":\"categorical\",\"colours\":[\"#000000\",\"#ffffff\"]}"), "%s%n.diaramps");
	ASSERT_TRUE(r.mOk);
	ASSERT_EQ(r.mRampErrors.Size(), 1u);
	EXPECT_TRUE(Mentions(r, 0, "%s%n%d"));
	EXPECT_TRUE(Mentions(r, 0, "%s%n.diaramps"));
	EXPECT_EQ(asserts.Count(), 0);
}

TEST(DiaValueSchemaRampEdge, HugeUnknownRampKeyGivesBoundedDiagnostic)
{
	VsTest::AssertCounter asserts;
	const std::string ramp = "{\"id\":\"x\"," + std::string(kCat) + ",\"" + std::string(5000, 'k') + "\":1}";
	const RampLoadResult r = Load(File(ramp), std::string(255, 'f').c_str());
	ASSERT_TRUE(r.mOk);
	ASSERT_EQ(r.mRampErrors.Size(), 1u);
	EXPECT_LE(strlen(r.mRampErrors[0].AsCStr()), 511u);
	EXPECT_EQ(asserts.Count(), 0);
}

// ---- fuzz-lite -----------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampEdge, ShippedRampsLoadCleanly)
{
	const std::string text = RampsJson();
	ASSERT_FALSE(text.empty()) << "Dia/DiaValueSchema/Data/ramps.json not found";
	const RampLoadResult r = Load(text);
	ASSERT_TRUE(r.mOk) << r.mFileReason.AsCStr();
	EXPECT_EQ(r.mRampsRejected, 0u);
	EXPECT_GE(r.mRamps.Size(), 8u);
}

TEST(DiaValueSchemaRampEdge, TwoThousandSeededMutationsOfShippedRampsNeverCrashAndKeepInvariants)
{
	const std::string text = RampsJson();
	ASSERT_FALSE(text.empty());
	std::mt19937 rng(1234);
	unsigned accepted = 0;
	for (int iter = 0; iter < 2000; ++iter)
	{
		std::string s = text;
		const unsigned ops = 1 + rng() % 3;
		for (unsigned k = 0; k < ops && !s.empty(); ++k)
		{
			const size_t pos = rng() % s.size();
			switch (rng() % 6)
			{
			case 0:	s[pos] = static_cast<char>(s[pos] ^ (1 << (rng() % 8))); break;
			case 1:	s.erase(pos, 1 + rng() % 8); break;
			case 2:	s.insert(pos, 1, static_cast<char>(rng() % 256)); break;
			case 3:	s.resize(pos); break;
			case 4:	s.insert(pos, s.substr(pos, 1 + rng() % 32)); break;
			default: s[pos] = "{}[]\",:0-.eE\\ntf#"[rng() % 17]; break;
			}
		}
		const RampLoadResult r = Load(s);
		if (!r.mOk)
		{
			ASSERT_EQ(r.mRamps.Size(), 0u) << iter;
			ASSERT_GT(strlen(r.mFileReason.AsCStr()), 0u) << iter;
			continue;
		}
		++accepted;
		ASSERT_EQ(r.mRampErrors.Size(), r.mRampsRejected) << iter;
		for (unsigned i = 0; i < r.mRamps.Size(); ++i)
		{
			const RampData& d = r.mRamps[i];
			ASSERT_GT(strlen(d.mName.AsCStr()), 0u) << iter;
			ASSERT_EQ(d.mId.Value(), StringCRC(d.mName.AsCStr()).Value()) << iter;
			if (d.mKind == RampKind::Continuous)
			{
				ASSERT_GE(d.mStopCount, 2u); ASSERT_LE(d.mStopCount, kMaxRampStops);
				ASSERT_EQ(d.mStops[0].mT, 0.0f); ASSERT_EQ(d.mStops[d.mStopCount - 1].mT, 1.0f);
				for (unsigned s2 = 1; s2 < d.mStopCount; ++s2) { ASSERT_GE(d.mStops[s2].mT, d.mStops[s2 - 1].mT) << iter; }
				for (unsigned s2 = 0; s2 < d.mStopCount; ++s2) { ASSERT_EQ(d.mStops[s2].mRgba8 & 0xFFu, 0xFFu) << iter; }
				ASSERT_EQ(d.mColourCount, 0u);
			}
			else
			{
				ASSERT_GE(d.mColourCount, 2u); ASSERT_LE(d.mColourCount, kMaxCategoricalColours);
				for (unsigned c = 0; c < d.mColourCount; ++c) { ASSERT_EQ(d.mColours[c] & 0xFFu, 0xFFu) << iter; }
				ASSERT_EQ(d.mStopCount, 0u);
			}
		}
	}
	EXPECT_GT(accepted, 100u);
}
