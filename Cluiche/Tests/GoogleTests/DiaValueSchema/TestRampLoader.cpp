#include <gtest/gtest.h>
#include <DiaValueSchema/RampData.h>
#include <DiaValueSchema/RampLoader.h>
#include <string>

using namespace Dia::ValueSchema;
using Dia::Core::StringCRC;

namespace
{
	const char* kGood = R"({"id":"good.one","kind":"continuous","stops":[{"t":0.0,"colour":"#000000"},{"t":1.0,"colour":"#ffffff"}]})";

	std::string File(const std::string& ramps)
	{
		return std::string(R"({"format":"dia.ramps","version":1,"ramps":[)") + ramps + "]}";
	}

	// One bad ramp followed by a good one: the bad one is skipped, the good one loads.
	void ExpectSkipped(const char* badRamp, const char* idInError)
	{
		const std::string text = File(std::string(badRamp) + "," + kGood);
		RampLoadResult r = RampLoader::LoadFromText(text.c_str(), "t.diaramps");
		EXPECT_TRUE(r.mOk) << badRamp;
		ASSERT_EQ(r.mRamps.Size(), 1u) << badRamp;
		EXPECT_STREQ(r.mRamps[0].mName.AsCStr(), "good.one");
		EXPECT_EQ(r.mRampsRejected, 1u) << badRamp;
		ASSERT_EQ(r.mRampErrors.Size(), 1u) << badRamp;
		if (idInError) { EXPECT_NE(std::string(r.mRampErrors[0].AsCStr()).find(idInError), std::string::npos) << r.mRampErrors[0].AsCStr(); }
	}

	std::string Stops(int n)
	{
		std::string s = R"({"id":"many","kind":"continuous","stops":[)";
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

	std::string Colours(int n)
	{
		std::string s = R"({"id":"cats","kind":"categorical","colours":[)";
		for (int i = 0; i < n; ++i) { if (i) { s += ","; } s += "\"#0a0b0c\""; }
		return s + "]}";
	}
}

TEST(DiaValueSchemaRampHex, ParsesValid)
{
	uint32_t c = 0;
	EXPECT_TRUE(ParseHexColour("#f7fcf5", c));
	EXPECT_EQ(c, 0xf7fcf5FFu);
	EXPECT_TRUE(ParseHexColour("#A6CEE3", c));
	EXPECT_EQ(c, 0xA6CEE3FFu);
	EXPECT_TRUE(ParseHexColour("#000000", c));
	EXPECT_EQ(c, 0x000000FFu);
}

TEST(DiaValueSchemaRampHex, RejectsInvalid)
{
	uint32_t c = 0;
	EXPECT_FALSE(ParseHexColour(nullptr, c));
	EXPECT_FALSE(ParseHexColour("", c));
	EXPECT_FALSE(ParseHexColour("a6cee3", c));
	EXPECT_FALSE(ParseHexColour("#abc", c));
	EXPECT_FALSE(ParseHexColour("#a6cee3ff", c));
	EXPECT_FALSE(ParseHexColour("#a6cee", c));
	EXPECT_FALSE(ParseHexColour("#gggggg", c));
	EXPECT_FALSE(ParseHexColour("#a6cee3 ", c));
}

TEST(DiaValueSchemaRampLoader, LoadsContinuousAndCategorical)
{
	const std::string text = File(
		R"({"id":"sequential.green","kind":"continuous","stops":[{"t":0.0,"colour":"#f7fcf5"},{"t":0.5,"colour":"#74c476"},{"t":1.0,"colour":"#00441B"}]},)"
		R"({"id":"categorical.12","kind":"categorical","colours":["#a6cee3","#1f78b4"]})");
	RampLoadResult r = RampLoader::LoadFromText(text.c_str(), "t.diaramps");
	ASSERT_TRUE(r.mOk);
	EXPECT_EQ(r.mRampsRejected, 0u);
	ASSERT_EQ(r.mRamps.Size(), 2u);

	const RampData& a = r.mRamps[0];
	EXPECT_STREQ(a.mName.AsCStr(), "sequential.green");
	EXPECT_EQ(a.mId, StringCRC("sequential.green"));
	EXPECT_EQ(a.mKind, RampKind::Continuous);
	ASSERT_EQ(a.mStopCount, 3u);
	EXPECT_FLOAT_EQ(a.mStops[0].mT, 0.0f);
	EXPECT_EQ(a.mStops[0].mRgba8, 0xf7fcf5FFu);
	EXPECT_FLOAT_EQ(a.mStops[1].mT, 0.5f);
	EXPECT_EQ(a.mStops[1].mRgba8, 0x74c476FFu);
	EXPECT_FLOAT_EQ(a.mStops[2].mT, 1.0f);
	EXPECT_EQ(a.mStops[2].mRgba8, 0x00441BFFu);

	const RampData& b = r.mRamps[1];
	EXPECT_EQ(b.mId, StringCRC("categorical.12"));
	EXPECT_EQ(b.mKind, RampKind::Categorical);
	ASSERT_EQ(b.mColourCount, 2u);
	EXPECT_EQ(b.mColours[0], 0xa6cee3FFu);
	EXPECT_EQ(b.mColours[1], 0x1f78b4FFu);
}

TEST(DiaValueSchemaRampLoader, EmptyRampsArrayIsOk)
{
	RampLoadResult r = RampLoader::LoadFromText(R"({"format":"dia.ramps","version":1,"ramps":[]})", "t.diaramps");
	EXPECT_TRUE(r.mOk);
	EXPECT_EQ(r.mRamps.Size(), 0u);
}

TEST(DiaValueSchemaRampLoader, RejectsWholeFile)
{
	const char* bad[] = {
		R"({"format":"dia.other","version":1,"ramps":[]})",
		R"({"version":1,"ramps":[]})",
		R"({"format":"dia.ramps","version":2,"ramps":[]})",
		R"({"format":"dia.ramps","version":"1","ramps":[]})",
		R"({"format":"dia.ramps","ramps":[]})",
		R"([1,2])",
		R"({"format":"dia.ramps","version":1,"ramps":{}})",
		R"({"format":"dia.ramps","version":1})",
		R"({"format":"dia.ramps","version":1,"ramps":[],"extra":1})",
	};
	for (const char* t : bad)
	{
		RampLoadResult r = RampLoader::LoadFromText(t, "t.diaramps");
		EXPECT_FALSE(r.mOk) << t;
		EXPECT_EQ(r.mRamps.Size(), 0u) << t;
		EXPECT_GT(strlen(r.mFileReason.AsCStr()), 0u) << t;
		EXPECT_STREQ(r.mFile.AsCStr(), "t.diaramps");
	}
}

TEST(DiaValueSchemaRampLoader, WholeFileRejectionDropsRampsAlreadyParsed)
{
	const std::string text = std::string(R"({"format":"dia.ramps","version":1,"ramps":[)") + kGood + R"(],"zzz":true})";
	RampLoadResult r = RampLoader::LoadFromText(text.c_str(), "t.diaramps");
	EXPECT_FALSE(r.mOk);
	EXPECT_EQ(r.mRamps.Size(), 0u);
}

TEST(DiaValueSchemaRampLoader, RejectsUnparsableJson)
{
	RampLoadResult r = RampLoader::LoadFromText("{ not json", "t.diaramps");
	EXPECT_FALSE(r.mOk);
	EXPECT_EQ(r.mRamps.Size(), 0u);
}

TEST(DiaValueSchemaRampLoader, SkipsBadColour)
{
	ExpectSkipped(R"({"id":"bad.colour","kind":"continuous","stops":[{"t":0,"colour":"#12345"},{"t":1,"colour":"#ffffff"}]})", "bad.colour");
	ExpectSkipped(R"({"id":"bad.cat","kind":"categorical","colours":["#a6cee3","blue"]})", "bad.cat");
}

TEST(DiaValueSchemaRampLoader, SkipsNonIncreasingT)
{
	ExpectSkipped(R"({"id":"dec","kind":"continuous","stops":[{"t":0,"colour":"#000000"},{"t":0.7,"colour":"#111111"},{"t":0.3,"colour":"#222222"},{"t":1,"colour":"#ffffff"}]})", "dec");
}

TEST(DiaValueSchemaRampLoader, SkipsEqualT)
{
	ExpectSkipped(R"({"id":"eq","kind":"continuous","stops":[{"t":0,"colour":"#000000"},{"t":0.5,"colour":"#111111"},{"t":0.5,"colour":"#222222"},{"t":1,"colour":"#ffffff"}]})", "eq");
}

TEST(DiaValueSchemaRampLoader, SkipsFirstTNotZero)
{
	ExpectSkipped(R"({"id":"first","kind":"continuous","stops":[{"t":0.1,"colour":"#000000"},{"t":1,"colour":"#ffffff"}]})", "first");
}

TEST(DiaValueSchemaRampLoader, SkipsLastTNotOne)
{
	ExpectSkipped(R"({"id":"last","kind":"continuous","stops":[{"t":0,"colour":"#000000"},{"t":0.9,"colour":"#ffffff"}]})", "last");
}

TEST(DiaValueSchemaRampLoader, SkipsStopCountOutOfRange)
{
	ExpectSkipped(R"({"id":"one","kind":"continuous","stops":[{"t":0,"colour":"#000000"}]})", "one");
	ExpectSkipped(Stops(17).c_str(), "many");
}

TEST(DiaValueSchemaRampLoader, SkipsColourCountOutOfRange)
{
	ExpectSkipped(R"({"id":"onecat","kind":"categorical","colours":["#000000"]})", "onecat");
	ExpectSkipped(Colours(33).c_str(), "cats");
}

TEST(DiaValueSchemaRampLoader, SkipsUnknownKind)
{
	ExpectSkipped(R"({"id":"weird","kind":"diverging","stops":[]})", "weird");
	ExpectSkipped(R"({"id":"nokind","stops":[]})", "nokind");
}

TEST(DiaValueSchemaRampLoader, SkipsBadId)
{
	ExpectSkipped(R"({"id":"Upper.Case","kind":"categorical","colours":["#000000","#ffffff"]})", "Upper.Case");
	ExpectSkipped(R"({"id":"has space","kind":"categorical","colours":["#000000","#ffffff"]})", "has space");
	ExpectSkipped(R"({"id":"dot..dot","kind":"categorical","colours":["#000000","#ffffff"]})", "dot..dot");
	ExpectSkipped(R"({"id":".lead","kind":"categorical","colours":["#000000","#ffffff"]})", ".lead");
	ExpectSkipped(R"({"id":"","kind":"categorical","colours":["#000000","#ffffff"]})", nullptr);
	ExpectSkipped(R"({"kind":"categorical","colours":["#000000","#ffffff"]})", nullptr);
	ExpectSkipped(R"({"id":5,"kind":"categorical","colours":["#000000","#ffffff"]})", nullptr);
	const std::string longId = std::string(R"({"id":")") + std::string(64, 'a') + R"(","kind":"categorical","colours":["#000000","#ffffff"]})";
	ExpectSkipped(longId.c_str(), nullptr);
}

TEST(DiaValueSchemaRampLoader, SkipsDuplicateIdInFile)
{
	const std::string text = File(std::string(kGood) + "," + kGood);
	RampLoadResult r = RampLoader::LoadFromText(text.c_str(), "t.diaramps");
	EXPECT_TRUE(r.mOk);
	EXPECT_EQ(r.mRamps.Size(), 1u);
	EXPECT_EQ(r.mRampsRejected, 1u);
	ASSERT_EQ(r.mRampErrors.Size(), 1u);
	EXPECT_NE(std::string(r.mRampErrors[0].AsCStr()).find("good.one"), std::string::npos);
}

TEST(DiaValueSchemaRampLoader, SkipsWrongJsonTypes)
{
	ExpectSkipped(R"("just a string")", nullptr);
	ExpectSkipped(R"({"id":"t1","kind":"continuous","stops":"nope"})", "t1");
	ExpectSkipped(R"({"id":"t2","kind":"continuous","stops":[5,6]})", "t2");
	ExpectSkipped(R"({"id":"t3","kind":"continuous","stops":[{"t":"0","colour":"#000000"},{"t":1,"colour":"#ffffff"}]})", "t3");
	ExpectSkipped(R"({"id":"t4","kind":"continuous","stops":[{"t":0,"colour":5},{"t":1,"colour":"#ffffff"}]})", "t4");
	ExpectSkipped(R"({"id":"t5","kind":"categorical","colours":"nope"})", "t5");
	ExpectSkipped(R"({"id":"t6","kind":"categorical","colours":[1,2]})", "t6");
	ExpectSkipped(R"({"id":"t7","kind":7,"stops":[]})", "t7");
	ExpectSkipped(R"({"id":"t8","kind":"continuous","stops":[{"colour":"#000000"},{"t":1,"colour":"#ffffff"}]})", "t8");
}

TEST(DiaValueSchemaRampLoader, SkipsUnknownRampKey)
{
	ExpectSkipped(R"({"id":"extra","kind":"categorical","colours":["#000000","#ffffff"],"label":"x"})", "extra");
	ExpectSkipped(R"({"id":"extra2","kind":"continuous","stops":[{"t":0,"colour":"#000000","a":1},{"t":1,"colour":"#ffffff"}]})", "extra2");
	// Wrong-kind payload key: stops on a categorical ramp.
	ExpectSkipped(R"({"id":"mixed","kind":"categorical","colours":["#000000","#ffffff"],"stops":[]})", "mixed");
}

TEST(DiaValueSchemaRampLoader, AcceptsBoundaryCounts)
{
	const std::string two = File(Stops(2));
	RampLoadResult r2 = RampLoader::LoadFromText(two.c_str(), "t.diaramps");
	ASSERT_TRUE(r2.mOk);
	ASSERT_EQ(r2.mRamps.Size(), 1u);
	EXPECT_EQ(r2.mRamps[0].mStopCount, 2u);

	const std::string sixteen = File(Stops(16));
	RampLoadResult r16 = RampLoader::LoadFromText(sixteen.c_str(), "t.diaramps");
	ASSERT_TRUE(r16.mOk);
	ASSERT_EQ(r16.mRamps.Size(), 1u);
	EXPECT_EQ(r16.mRamps[0].mStopCount, 16u);
	EXPECT_FLOAT_EQ(r16.mRamps[0].mStops[15].mT, 1.0f);

	const std::string c2 = File(Colours(2));
	RampLoadResult rc2 = RampLoader::LoadFromText(c2.c_str(), "t.diaramps");
	ASSERT_EQ(rc2.mRamps.Size(), 1u);
	EXPECT_EQ(rc2.mRamps[0].mColourCount, 2u);

	const std::string c32 = File(Colours(32));
	RampLoadResult rc32 = RampLoader::LoadFromText(c32.c_str(), "t.diaramps");
	ASSERT_TRUE(rc32.mOk);
	ASSERT_EQ(rc32.mRamps.Size(), 1u);
	EXPECT_EQ(rc32.mRamps[0].mColourCount, 32u);
	EXPECT_EQ(rc32.mRamps[0].mColours[31], 0x0a0b0cFFu);
}

TEST(DiaValueSchemaRampLoader, MaxLengthIdAccepted)
{
	const std::string id(63, 'a');
	const std::string text = File(std::string(R"({"id":")") + id + R"(","kind":"categorical","colours":["#000000","#ffffff"]})");
	RampLoadResult r = RampLoader::LoadFromText(text.c_str(), "t.diaramps");
	ASSERT_TRUE(r.mOk);
	ASSERT_EQ(r.mRamps.Size(), 1u);
	EXPECT_STREQ(r.mRamps[0].mName.AsCStr(), id.c_str());
}
