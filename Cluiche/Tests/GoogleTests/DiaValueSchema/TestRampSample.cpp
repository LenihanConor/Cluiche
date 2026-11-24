#include <gtest/gtest.h>
#include <DiaValueSchema/RampSample.h>
#include <DiaValueSchema/RampLoader.h>
#include <limits>
#include <string>

using namespace Dia::ValueSchema;

namespace
{
	RampData Load(const char* rampJson)
	{
		const std::string text = std::string(R"({"format":"dia.ramps","version":1,"ramps":[)") + rampJson + "]}";
		RampLoadResult r = RampLoader::LoadFromText(text.c_str(), "t.json");
		EXPECT_TRUE(r.mOk);
		EXPECT_EQ(1u, r.mRamps.Size());
		return r.mRamps[0];
	}

	RampData TwoStop()		// #000000 -> #ffffff
	{
		return Load(R"({"id":"t.two","kind":"continuous","stops":[{"t":0.0,"colour":"#000000"},{"t":1.0,"colour":"#ffffff"}]})");
	}
	RampData ThreeStop()	// stops at 0, 0.5, 1
	{
		return Load(R"({"id":"t.three","kind":"continuous","stops":[{"t":0.0,"colour":"#000000"},{"t":0.5,"colour":"#0a141e"},{"t":1.0,"colour":"#ffffff"}]})");
	}
	RampData Categorical()
	{
		return Load(R"({"id":"t.cat","kind":"categorical","colours":["#ff0000","#00ff00","#0000ff"]})");
	}
}

TEST(DiaValueSchemaRampSample, EndpointsAreExact)
{
	const RampData r = TwoStop();
	EXPECT_EQ(0x000000FFu, SampleRamp(r, 0.0f));
	EXPECT_EQ(0xFFFFFFFFu, SampleRamp(r, 1.0f));
}

TEST(DiaValueSchemaRampSample, MidpointRoundsToNearest)
{
	const RampData r = TwoStop();
	EXPECT_EQ(0x808080FFu, SampleRamp(r, 0.5f));	// 127.5 -> 128
	EXPECT_EQ(0x404040FFu, SampleRamp(r, 0.25f));	// 63.75 -> 64
}

TEST(DiaValueSchemaRampSample, ExactAtInteriorStop)
{
	const RampData r = ThreeStop();
	EXPECT_EQ(0x0A141EFFu, SampleRamp(r, 0.5f));
}

TEST(DiaValueSchemaRampSample, MidpointBetweenInteriorStops)
{
	const RampData r = ThreeStop();
	EXPECT_EQ(0x050A0FFFu, SampleRamp(r, 0.25f));	// (0+10)/2, (0+20)/2, (0+30)/2
	// t=0.75: 10 + 245/2 = 132.5 -> 133 (0x85); 20 + 235/2 = 137.5 -> 138 (0x8A); 30 + 225/2 = 142.5 -> 143 (0x8F)
	EXPECT_EQ(0x858A8FFFu, SampleRamp(r, 0.75f));
}

TEST(DiaValueSchemaRampSample, TClampedBelowZeroAndAboveOne)
{
	const RampData r = TwoStop();
	EXPECT_EQ(0x000000FFu, SampleRamp(r, -3.0f));
	EXPECT_EQ(0xFFFFFFFFu, SampleRamp(r, 7.0f));
}

TEST(DiaValueSchemaRampSample, NaNTreatedAsZero)
{
	const RampData r = TwoStop();
	EXPECT_EQ(0x000000FFu, SampleRamp(r, std::numeric_limits<float>::quiet_NaN()));
}

TEST(DiaValueSchemaRampSample, AlphaAlwaysOpaque)
{
	const RampData r = ThreeStop();
	for (float t = 0.0f; t <= 1.0f; t += 0.1f) { EXPECT_EQ(0xFFu, SampleRamp(r, t) & 0xFFu); }
}

TEST(DiaValueSchemaRampSample, CategoricalRampSamplesToFirstColour)
{
	const RampData r = Categorical();
	EXPECT_EQ(0xFF0000FFu, SampleRamp(r, 0.0f));
	EXPECT_EQ(0xFF0000FFu, SampleRamp(r, 0.9f));
}

TEST(DiaValueSchemaRampSample, RampColourAtWrapsModuloCount)
{
	const RampData r = Categorical();
	EXPECT_EQ(0xFF0000FFu, RampColourAt(r, 0));
	EXPECT_EQ(0x00FF00FFu, RampColourAt(r, 1));
	EXPECT_EQ(0x0000FFFFu, RampColourAt(r, 2));
	EXPECT_EQ(0xFF0000FFu, RampColourAt(r, 3));
	EXPECT_EQ(0x00FF00FFu, RampColourAt(r, 4));
}

TEST(DiaValueSchemaRampSample, RampColourAtOnContinuousReturnsFirstStop)
{
	EXPECT_EQ(0x000000FFu, RampColourAt(TwoStop(), 5));
}

TEST(DiaValueSchemaRampSample, EmptyRampReturnsBlack)
{
	RampData r;
	EXPECT_EQ(0x000000FFu, SampleRamp(r, 0.5f));
	EXPECT_EQ(0x000000FFu, RampColourAt(r, 1));
}
