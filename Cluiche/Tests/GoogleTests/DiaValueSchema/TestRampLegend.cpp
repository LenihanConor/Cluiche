#include <cstring>
#include <gtest/gtest.h>
#include <DiaValueSchema/RampLegend.h>
#include <DiaValueSchema/RampSample.h>
#include <DiaValueSchema/RampLoader.h>
#include <DiaValueSchema/RampRegistry.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace Dia::ValueSchema;
using Dia::ValueSchema::Testing::MakeValueInfo;
namespace fs = std::filesystem;

namespace
{
	std::string ReadShippedRamps()
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

	struct Fixture
	{
		ValueSchemaRegistry	mSchema;
		RampRegistry		mRamps;
		ValueSchemaService*	mService = nullptr;

		Fixture()
		{
			const std::string text = ReadShippedRamps();
			RampLoadResult r = RampLoader::LoadFromText(text.c_str(), "ramps.json");
			EXPECT_TRUE(r.mOk);
			for (unsigned i = 0; i < r.mRamps.Size(); ++i) { mRamps.Register(r.mRamps[i], true, "ramps.json"); }
			mSchema.Publish();
			mRamps.Publish();
			mService = new ValueSchemaService(mSchema, &mRamps);
		}
		~Fixture() { delete mService; }
	};

	ValueInfo Entry(const char* key, double mn, double mx, const char* unit = "")
	{
		ValueInfo info = MakeValueInfo("test", key);
		info.mFullKey = (std::string("test/") + key).c_str();
		info.mHasMin = true; info.mHasMax = true;
		info.mMin = mn; info.mMax = mx;
		info.mUnit = Dia::Core::Containers::String32("%s", unit);	// "%s": the const char* ctor is printf-style
		return info;
	}

	std::vector<float> Ticks(const LegendData& l)
	{
		return std::vector<float>(l.mTicks, l.mTicks + l.mTickCount);
	}

	void ExpectTicks(double mn, double mx, std::vector<float> expected)
	{
		Fixture f;
		RampResolver res(*f.mService);
		const LegendData l = BuildLegend(res, Entry("t", mn, mx), false, 0, 0);
		ASSERT_EQ(expected.size(), l.mTickCount) << mn << ".." << mx;
		for (size_t i = 0; i < expected.size(); ++i)
		{
			EXPECT_NEAR(expected[i], l.mTicks[i], std::fabs(expected[i]) * 1e-6f + 1e-12f) << mn << ".." << mx << " tick " << i;
		}
	}
}

TEST(DiaValueSchemaRampLegend, TicksZeroToOne)			{ ExpectTicks(0, 1, { 0, 0.5f, 1 }); }
TEST(DiaValueSchemaRampLegend, TicksZeroToHundred)		{ ExpectTicks(0, 100, { 0, 50, 100 }); }
TEST(DiaValueSchemaRampLegend, TicksSpanningZero)		{ ExpectTicks(-1, 1, { -1, -0.5f, 0, 0.5f, 1 }); }
TEST(DiaValueSchemaRampLegend, TicksSymmetricWide)		{ ExpectTicks(-50, 50, { -50, 0, 50 }); }
TEST(DiaValueSchemaRampLegend, TicksTinyRange)			{ ExpectTicks(0, 0.001, { 0, 0.0005f, 0.001f }); }
TEST(DiaValueSchemaRampLegend, TicksMicroRange)			{ ExpectTicks(0, 1e-6, { 0, 5e-7f, 1e-6f }); }
TEST(DiaValueSchemaRampLegend, TicksLargeRange)			{ ExpectTicks(0, 1e6, { 0, 500000, 1000000 }); }
TEST(DiaValueSchemaRampLegend, TicksSevenGivesTwos)		{ ExpectTicks(0, 7, { 0, 2, 4, 6, 7 }); }
TEST(DiaValueSchemaRampLegend, TicksOffsetRange)		{ ExpectTicks(3, 17, { 3, 5, 10, 15, 17 }); }
TEST(DiaValueSchemaRampLegend, TicksNegativeRange)		{ ExpectTicks(-10, -2, { -10, -8, -6, -4, -2 }); }
TEST(DiaValueSchemaRampLegend, TicksFloatNoise)			{ ExpectTicks(-0.3, 0.3, { -0.3f, -0.2f, 0, 0.2f, 0.3f }); }
TEST(DiaValueSchemaRampLegend, TicksFractionalOffset)	{ ExpectTicks(0.1, 0.7, { 0.1f, 0.2f, 0.4f, 0.6f, 0.7f }); }
TEST(DiaValueSchemaRampLegend, TicksSingleWhenEqual)	{ ExpectTicks(5, 5, { 5 }); }
TEST(DiaValueSchemaRampLegend, TicksSingleWhenInverted)	{ ExpectTicks(7, 3, { 7 }); }

TEST(DiaValueSchemaRampLegend, TicksNeverExceedFive)
{
	for (int span = 1; span < 400; span += 7)
	{
		float t[kMaxLegendTicks];
		const unsigned n = BuildTicks(-3.7f, -3.7f + static_cast<float>(span) * 0.37f, t);
		EXPECT_LE(n, kMaxLegendTicks);
		EXPECT_GE(n, 2u);
		for (unsigned i = 1; i < n; ++i) { EXPECT_LT(t[i - 1], t[i]); }
	}
}

TEST(DiaValueSchemaRampLegend, TicksNoFloatNoiseExactFloats)
{
	float t[kMaxLegendTicks];
	ASSERT_EQ(5u, BuildTicks(-0.3f, 0.3f, t));
	EXPECT_EQ(0.2f, t[3]);
	EXPECT_EQ(-0.2f, t[1]);
	EXPECT_EQ(0.0f, t[2]);
}

TEST(DiaValueSchemaRampLegend, TicksNonFiniteGivesNone)
{
	float t[kMaxLegendTicks];
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();
	EXPECT_EQ(0u, BuildTicks(nan, 1.0f, t));
	EXPECT_EQ(0u, BuildTicks(0.0f, nan, t));
	EXPECT_EQ(0u, BuildTicks(0.0f, inf, t));
	EXPECT_EQ(0u, BuildTicks(-inf, 1.0f, t));
}

TEST(DiaValueSchemaRampLegend, TicksEndsAreExactlyAsGiven)
{
	float t[kMaxLegendTicks];
	const unsigned n = BuildTicks(0.123f, 9.87f, t);
	ASSERT_GE(n, 2u);
	EXPECT_EQ(0.123f, t[0]);
	EXPECT_EQ(9.87f, t[n - 1]);
}

TEST(DiaValueSchemaRampLegend, FloatLegendCarriesRampRangeUnit)
{
	Fixture f;
	RampResolver res(*f.mService);
	ValueInfo info = Entry("a", 0, 100, "%");
	info.mUi["ramp"] = "sequential.green";
	const LegendData l = BuildLegend(res, info, false, 0, 0);
	EXPECT_FALSE(l.mIsCategory);
	EXPECT_EQ(0u, l.mSwatches.Size());
	EXPECT_STREQ("sequential.green", l.mRamp.mRamp.mName.AsCStr());
	EXPECT_EQ(0.0f, l.mMin);
	EXPECT_EQ(100.0f, l.mMax);
	EXPECT_STREQ("%", l.mUnit.AsCStr());
	EXPECT_EQ(3u, l.mTickCount);
	EXPECT_FALSE(l.mHasSeen);
}

TEST(DiaValueSchemaRampLegend, NoRangeUsesZeroToOne)
{
	Fixture f;
	RampResolver res(*f.mService);
	ValueInfo info = MakeValueInfo("test", "norange");
	info.mFullKey = "test/norange";
	const LegendData l = BuildLegend(res, info, false, 0, 0);
	EXPECT_EQ(0.0f, l.mMin);
	EXPECT_EQ(1.0f, l.mMax);
	EXPECT_EQ((std::vector<float>{ 0, 0.5f, 1 }), Ticks(l));
}

TEST(DiaValueSchemaRampLegend, IntEntryBehavesLikeFloat)
{
	Fixture f;
	RampResolver res(*f.mService);
	ValueInfo info = Entry("n", 0, 7, "days");
	info.mType = ValueType::Int;
	const LegendData l = BuildLegend(res, info, false, 0, 0);
	EXPECT_FALSE(l.mIsCategory);
	EXPECT_EQ(5u, l.mTickCount);
	EXPECT_STREQ("days", l.mUnit.AsCStr());
}

TEST(DiaValueSchemaRampLegend, SeenRangePassesThrough)
{
	Fixture f;
	RampResolver res(*f.mService);
	const LegendData l = BuildLegend(res, Entry("a", 0, 10), true, 2.0f, 8.0f);
	EXPECT_TRUE(l.mHasSeen);
	EXPECT_EQ(2.0f, l.mSeenMin);
	EXPECT_EQ(8.0f, l.mSeenMax);
}

TEST(DiaValueSchemaRampLegend, SeenRangeIsSwappedWhenInverted)
{
	Fixture f;
	RampResolver res(*f.mService);
	const LegendData l = BuildLegend(res, Entry("a", 0, 10), true, 8.0f, 2.0f);
	EXPECT_TRUE(l.mHasSeen);
	EXPECT_EQ(2.0f, l.mSeenMin);
	EXPECT_EQ(8.0f, l.mSeenMax);
}

TEST(DiaValueSchemaRampLegend, SeenRangeIgnoredWhenNotRequestedOrNonFinite)
{
	Fixture f;
	RampResolver res(*f.mService);
	EXPECT_FALSE(BuildLegend(res, Entry("a", 0, 10), false, 2.0f, 8.0f).mHasSeen);
	EXPECT_FALSE(BuildLegend(res, Entry("a", 0, 10), true, std::numeric_limits<float>::quiet_NaN(), 8.0f).mHasSeen);
}

namespace
{
	ValueInfo EnumInfo(unsigned count, unsigned givenMask)
	{
		ValueInfo info = MakeValueInfo("test", "e");
		info.mFullKey = "test/e";
		info.mType = ValueType::Enum;
		info.mEnumValues.Reserve(count);
		for (unsigned i = 0; i < count; ++i)
		{
			EnumValue v;
			v.mId = ("id" + std::to_string(i)).c_str();
			v.mLabel = ("Label " + std::to_string(i)).c_str();
			if (givenMask & (1u << i)) { v.mColour = "#102030"; v.mHasColour = true; }
			info.mEnumValues.Add(v);
		}
		return info;
	}
}

TEST(DiaValueSchemaRampLegend, EnumGivesSwatchesInSchemaOrder)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ValueInfo info = EnumInfo(4, 0b0010);
	const LegendData l = BuildLegend(res, info, false, 0, 0);
	const ResolvedPalette pal = res.ResolvePalette(info);
	EXPECT_TRUE(l.mIsCategory);
	EXPECT_EQ(0u, l.mTickCount);
	ASSERT_EQ(4u, l.mSwatches.Size());
	for (unsigned i = 0; i < 4; ++i)
	{
		EXPECT_STREQ(("id" + std::to_string(i)).c_str(), l.mSwatches[i].mId.AsCStr());
		EXPECT_STREQ(("Label " + std::to_string(i)).c_str(), l.mSwatches[i].mLabel.AsCStr());
		EXPECT_EQ(pal.mEntries[i].mRgba8, l.mSwatches[i].mRgba8);
	}
	EXPECT_EQ(0x102030FFu, l.mSwatches[1].mRgba8);
	EXPECT_EQ(0xE9EA86FFu, l.mSwatches[0].mRgba8);
}

TEST(DiaValueSchemaRampLegend, EnumEmptyLabelFallsBackToId)
{
	Fixture f;
	RampResolver res(*f.mService);
	ValueInfo info = EnumInfo(1, 0);
	info.mEnumValues[0].mLabel = "";
	const LegendData l = BuildLegend(res, info, false, 0, 0);
	ASSERT_EQ(1u, l.mSwatches.Size());
	EXPECT_STREQ("id0", l.mSwatches[0].mLabel.AsCStr());
}

TEST(DiaValueSchemaRampLegend, NonMatchingTypesGiveEmptyLegend)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ValueType types[] = { ValueType::Bool, ValueType::String, ValueType::Ref, ValueType::RefList, ValueType::Map };
	for (ValueType t : types)
	{
		ValueInfo info = Entry("x", 0, 10, "u");
		info.mType = t;
		const LegendData l = BuildLegend(res, info, true, 1.0f, 2.0f);
		EXPECT_EQ(0u, l.mTickCount);
		EXPECT_EQ(0u, l.mSwatches.Size());
		EXPECT_FALSE(l.mIsCategory);
	}
}

TEST(DiaValueSchemaRampLegend, LegendIsCopyable)
{
	Fixture f;
	RampResolver res(*f.mService);
	LegendData a = BuildLegend(res, EnumInfo(3, 0), false, 0, 0);
	LegendData b = a;
	EXPECT_EQ(3u, b.mSwatches.Size());
	LegendData empty;
	b = empty;
	EXPECT_EQ(0u, b.mSwatches.Size());
}
