#include <gtest/gtest.h>
#include <DiaValueSchema/RampData.h>
#include <DiaValueSchema/RampLoader.h>
#include <DiaValueSchema/Testing/ColourMath.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Shipped-ramp property tests (Feature 5 task 2). Dia/DiaValueSchema/Data/ramps.json is located by walking up
// from this source file (__FILE__).
//
// Chosen constants (measured on the shipped file):
//   categorical.12 min pairwise Delta-E76: 46.1 (normal vision), 27.7 (deuteranopia), 28.1 (protanopia).
//   Sequential direction: all five run light (t=0) -> dark (t=1); L* strictly decreasing.
//   sequential.terrain: exempt from monotonic lightness; starts with a blue water colour.

using namespace Dia::ValueSchema;
using namespace Dia::ValueSchema::Testing;
namespace fs = std::filesystem;

namespace
{
	constexpr double kMinCategoricalDeltaE = 40.0;		// normal vision (measured 46.1)
	constexpr double kMinCategoricalCvdDeltaE = 25.0;	// deuteranopia / protanopia (measured 27.7 min)
	constexpr double kDivergingSymmetryTolerance = 1.0;	// L* units
	constexpr double kDivergingMidpointMinL = 90.0;

	std::string ReadRampsFile()
	{
		std::error_code ec;
		fs::path dir = fs::absolute(fs::path(__FILE__), ec).parent_path();
		while (!dir.empty())
		{
			const fs::path candidate = dir / "Dia" / "DiaValueSchema" / "Data" / "ramps.json";
			if (fs::exists(candidate, ec))
			{
				std::ifstream in(candidate, std::ios::binary);
				std::stringstream ss;
				ss << in.rdbuf();
				return ss.str();
			}
			const fs::path parent = dir.parent_path();
			if (parent == dir) { break; }
			dir = parent;
		}
		return std::string();
	}

	const RampLoadResult& Shipped()
	{
		static const RampLoadResult sResult = []
		{
			const std::string text = ReadRampsFile();
			return RampLoader::LoadFromText(text.c_str(), "ramps.json");
		}();
		return sResult;
	}

	const RampData* Find(const char* id)
	{
		const RampLoadResult& r = Shipped();
		for (unsigned i = 0; i < r.mRamps.Size(); ++i)
		{
			if (std::string(r.mRamps[i].mName.AsCStr()) == id) { return &r.mRamps[i]; }
		}
		return nullptr;
	}

	double StopL(const RampData& r, unsigned i) { return Rgba8ToLab(r.mStops[i].mRgba8).L; }
}

// ---- ColourMath unit tests ----

TEST(DiaValueSchemaColourMath, WhiteBlackAndGrey)
{
	EXPECT_NEAR(Rgba8ToLab(0xFFFFFFFF).L, 100.0, 0.05);
	EXPECT_NEAR(Rgba8ToLab(0x000000FF).L, 0.0, 0.001);
	EXPECT_NEAR(Rgba8ToLab(0x777777FF).L, 50.0, 0.5);
	EXPECT_NEAR(Rgba8ToLab(0x777777FF).a, 0.0, 0.05);
	EXPECT_NEAR(Rgba8ToLab(0x777777FF).b, 0.0, 0.05);
}

TEST(DiaValueSchemaColourMath, DeltaE76)
{
	EXPECT_NEAR(DeltaE76(Rgba8ToLab(0xFFFFFFFF), Rgba8ToLab(0x000000FF)), 100.0, 0.05);
	EXPECT_DOUBLE_EQ(DeltaE76(Rgba8ToLab(0x123456FF), Rgba8ToLab(0x123456FF)), 0.0);
}

TEST(DiaValueSchemaColourMath, CvdKeepsGreyAndCollapsesRedGreen)
{
	// Neutral greys are (nearly) unchanged by the Machado matrices (rows sum to ~1).
	EXPECT_NEAR(Rgba8ToLabCvd(0x808080FF, CvdType::Deuteranopia).L, Rgba8ToLab(0x808080FF).L, 1.0);
	EXPECT_NEAR(Rgba8ToLabCvd(0x808080FF, CvdType::Protanopia).L, Rgba8ToLab(0x808080FF).L, 1.0);
	// Pure red and green are far apart normally and much closer when simulated.
	const double normal = DeltaE76(Rgba8ToLab(0xFF0000FF), Rgba8ToLab(0x00FF00FF));
	EXPECT_LT(DeltaE76(Rgba8ToLabCvd(0xFF0000FF, CvdType::Deuteranopia), Rgba8ToLabCvd(0x00FF00FF, CvdType::Deuteranopia)), normal * 0.6);
	EXPECT_LT(DeltaE76(Rgba8ToLabCvd(0xFF0000FF, CvdType::Protanopia), Rgba8ToLabCvd(0x00FF00FF, CvdType::Protanopia)), normal * 0.6);
}

// ---- Shipped ramps ----

TEST(DiaValueSchemaShippedRamps, LoadsWithExactlyTheEightIds)
{
	ASSERT_FALSE(ReadRampsFile().empty()) << "could not locate Dia/DiaValueSchema/Data/ramps.json from " << __FILE__;
	const RampLoadResult& r = Shipped();
	ASSERT_TRUE(r.mOk) << r.mFileReason.AsCStr();
	EXPECT_EQ(r.mRampsRejected, 0u);
	for (unsigned i = 0; i < r.mRampErrors.Size(); ++i) { ADD_FAILURE() << r.mRampErrors[i].AsCStr(); }
	ASSERT_EQ(r.mRamps.Size(), 8u);

	const char* ids[8] = { "sequential.green", "sequential.blue", "sequential.heat", "sequential.grey",
		"sequential.purple", "sequential.terrain", "diverging.blue_red", "categorical.12" };
	for (unsigned i = 0; i < 8; ++i)
	{
		EXPECT_STREQ(r.mRamps[i].mName.AsCStr(), ids[i]);
		const RampData* ramp = Find(ids[i]);
		ASSERT_NE(ramp, nullptr) << ids[i];
		EXPECT_EQ(ramp->mId, Dia::Core::StringCRC(ids[i]));
		if (std::string(ids[i]) == "categorical.12")
		{
			EXPECT_EQ(ramp->mKind, RampKind::Categorical);
			EXPECT_EQ(ramp->mColourCount, 12u);
		}
		else
		{
			EXPECT_EQ(ramp->mKind, RampKind::Continuous) << ids[i];
			EXPECT_GE(ramp->mStopCount, 2u) << ids[i];
			EXPECT_LE(ramp->mStopCount, kMaxRampStops) << ids[i];
			EXPECT_FLOAT_EQ(ramp->mStops[0].mT, 0.0f) << ids[i];
			EXPECT_FLOAT_EQ(ramp->mStops[ramp->mStopCount - 1].mT, 1.0f) << ids[i];
		}
	}
}

TEST(DiaValueSchemaShippedRamps, SequentialRampsHaveMonotonicLightness)
{
	// Direction: every sequential ramp runs light (t=0) to dark (t=1), so L* strictly decreases.
	const char* ids[5] = { "sequential.green", "sequential.blue", "sequential.heat", "sequential.grey", "sequential.purple" };
	for (const char* id : ids)
	{
		const RampData* r = Find(id);
		ASSERT_NE(r, nullptr) << id;
		for (unsigned i = 1; i < r->mStopCount; ++i)
		{
			EXPECT_LT(StopL(*r, i), StopL(*r, i - 1)) << id << " stop " << i;
		}
	}
}

TEST(DiaValueSchemaShippedRamps, TerrainStartsWithWaterAndHasEnoughStops)
{
	const RampData* r = Find("sequential.terrain");
	ASSERT_NE(r, nullptr);
	EXPECT_GE(r->mStopCount, 4u);
	const Rgb first = UnpackRgba8(r->mStops[0].mRgba8);
	EXPECT_GT(first.b, first.r);
	EXPECT_GT(first.b, first.g);
}

TEST(DiaValueSchemaShippedRamps, DivergingIsLightnessSymmetricAboutLightMidpoint)
{
	const RampData* r = Find("diverging.blue_red");
	ASSERT_NE(r, nullptr);
	const unsigned n = r->mStopCount;
	ASSERT_EQ(n % 2, 1u);
	const unsigned mid = n / 2;
	EXPECT_FLOAT_EQ(r->mStops[mid].mT, 0.5f);
	EXPECT_GE(StopL(*r, mid), kDivergingMidpointMinL);
	for (unsigned i = 0; i < mid; ++i)
	{
		const unsigned j = n - 1 - i;
		EXPECT_NEAR(r->mStops[i].mT + r->mStops[j].mT, 1.0f, 1e-5) << "stop " << i;
		EXPECT_NEAR(StopL(*r, i), StopL(*r, j), kDivergingSymmetryTolerance) << "stop " << i;
	}
	const Rgb lo = UnpackRgba8(r->mStops[0].mRgba8);
	const Rgb hi = UnpackRgba8(r->mStops[n - 1].mRgba8);
	EXPECT_GT(lo.b, lo.r);		// blue end
	EXPECT_GT(hi.r, hi.b);		// red end
}

TEST(DiaValueSchemaShippedRamps, Categorical12IsPairwiseDistinct)
{
	const RampData* r = Find("categorical.12");
	ASSERT_NE(r, nullptr);
	ASSERT_EQ(r->mColourCount, 12u);

	double minNormal = 1e9, minDeut = 1e9, minProt = 1e9;
	for (unsigned i = 0; i < r->mColourCount; ++i)
	{
		for (unsigned j = i + 1; j < r->mColourCount; ++j)
		{
			minNormal = std::min(minNormal, DeltaE76(Rgba8ToLab(r->mColours[i]), Rgba8ToLab(r->mColours[j])));
			minDeut = std::min(minDeut, DeltaE76(Rgba8ToLabCvd(r->mColours[i], CvdType::Deuteranopia), Rgba8ToLabCvd(r->mColours[j], CvdType::Deuteranopia)));
			minProt = std::min(minProt, DeltaE76(Rgba8ToLabCvd(r->mColours[i], CvdType::Protanopia), Rgba8ToLabCvd(r->mColours[j], CvdType::Protanopia)));
		}
	}
	RecordProperty("minNormal", std::to_string(minNormal));
	RecordProperty("minDeuteranopia", std::to_string(minDeut));
	RecordProperty("minProtanopia", std::to_string(minProt));
	EXPECT_GE(minNormal, kMinCategoricalDeltaE) << "measured " << minNormal;
	EXPECT_GE(minDeut, kMinCategoricalCvdDeltaE) << "measured " << minDeut;
	EXPECT_GE(minProt, kMinCategoricalCvdDeltaE) << "measured " << minProt;
}
