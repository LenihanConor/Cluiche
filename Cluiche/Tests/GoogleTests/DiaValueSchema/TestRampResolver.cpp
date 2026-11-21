#include <gtest/gtest.h>
#include <DiaValueSchema/RampResolver.h>
#include <DiaValueSchema/RampSample.h>
#include <DiaValueSchema/RampLoader.h>
#include <DiaValueSchema/RampRegistry.h>
#include <DiaValueSchema/CategoricalColours.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

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

	// Fixture: real shipped ramps + empty published schema registry.
	struct Fixture
	{
		ValueSchemaRegistry	mSchema;
		RampRegistry		mRamps;
		ValueSchemaService*	mService = nullptr;

		explicit Fixture(bool withRamps = true)
		{
			if (withRamps)
			{
				const std::string text = ReadShippedRamps();
				RampLoadResult r = RampLoader::LoadFromText(text.c_str(), "ramps.json");
				EXPECT_TRUE(r.mOk);
				for (unsigned i = 0; i < r.mRamps.Size(); ++i) { mRamps.Register(r.mRamps[i], true, "ramps.json"); }
			}
			mSchema.Publish();
			mRamps.Publish();
			mService = new ValueSchemaService(mSchema, &mRamps);
		}
		~Fixture() { delete mService; }
	};

	ValueInfo FloatEntry(const char* key, const char* ramp = nullptr)
	{
		ValueInfo info = MakeValueInfo("test", key);
		info.mFullKey = (std::string("test/") + key).c_str();
		if (ramp) { info.mUi["ramp"] = ramp; }
		return info;
	}

	ValueInfo EnumEntry(const char* key, unsigned count, unsigned givenMask = 0)
	{
		ValueInfo info = MakeValueInfo("test", key);
		info.mFullKey = (std::string("test/") + key).c_str();
		info.mType = ValueType::Enum;
		info.mEnumValues.Reserve(count);
		for (unsigned i = 0; i < count; ++i)
		{
			EnumValue v;
			v.mId = ("v" + std::to_string(i)).c_str();
			if (givenMask & (1u << i)) { v.mColour = "#102030"; v.mHasColour = true; }
			info.mEnumValues.Add(v);
		}
		return info;
	}
}

TEST(DiaValueSchemaRampResolver, NamedRampIsResolved)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ResolvedRamp r = res.ResolveRamp(FloatEntry("a", "sequential.green"));
	EXPECT_FALSE(r.mFellBack);
	EXPECT_STREQ("sequential.green", r.mRamp.mName.AsCStr());
	EXPECT_EQ(0xF7FCF5FFu, SampleRamp(r.mRamp, 0.0f));
	EXPECT_EQ(0x00441BFFu, SampleRamp(r.mRamp, 1.0f));
	EXPECT_EQ(0u, res.UnknownRampWarningCount());
}

TEST(DiaValueSchemaRampResolver, AbsentRampIsGreyWithoutWarning)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ResolvedRamp r = res.ResolveRamp(FloatEntry("a"));
	EXPECT_STREQ("sequential.grey", r.mRamp.mName.AsCStr());
	EXPECT_FALSE(r.mFellBack);
	EXPECT_EQ(0u, res.UnknownRampWarningCount());
	EXPECT_EQ(0u, res.FallbackCount());
}

TEST(DiaValueSchemaRampResolver, UnknownRampFallsBackAndWarnsOncePerKey)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ValueInfo a = FloatEntry("a", "nope.nothing");
	for (int i = 0; i < 20; ++i)
	{
		const ResolvedRamp r = res.ResolveRamp(a);
		EXPECT_TRUE(r.mFellBack);
		EXPECT_STREQ("sequential.grey", r.mRamp.mName.AsCStr());
	}
	EXPECT_EQ(1u, res.UnknownRampWarningCount());
	res.ResolveRamp(FloatEntry("b", "nope.nothing"));
	EXPECT_EQ(2u, res.UnknownRampWarningCount());
	EXPECT_EQ(21u, res.FallbackCount());
}

TEST(DiaValueSchemaRampResolver, CategoricalRampOnFloatEntryFallsBack)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ResolvedRamp r = res.ResolveRamp(FloatEntry("a", "categorical.12"));
	EXPECT_TRUE(r.mFellBack);
	EXPECT_STREQ("sequential.grey", r.mRamp.mName.AsCStr());
	EXPECT_EQ(1u, res.UnknownRampWarningCount());
}

TEST(DiaValueSchemaRampResolver, TwoResolversDoNotShareWarnState)
{
	Fixture f;
	RampResolver r1(*f.mService);
	RampResolver r2(*f.mService);
	const ValueInfo a = FloatEntry("a", "nope");
	r1.ResolveRamp(a);
	r2.ResolveRamp(a);
	EXPECT_EQ(1u, r1.UnknownRampWarningCount());
	EXPECT_EQ(1u, r2.UnknownRampWarningCount());
}

TEST(DiaValueSchemaRampResolver, RangePassesThrough)
{
	Fixture f;
	RampResolver res(*f.mService);
	ValueInfo info = FloatEntry("a", "sequential.blue");
	info.mHasMin = true; info.mMin = -2.0;
	info.mHasMax = true; info.mMax = 6.0;
	const ResolvedRamp r = res.ResolveRamp(info);
	EXPECT_TRUE(r.mHasRange);
	EXPECT_FLOAT_EQ(-2.0f, r.mMin);
	EXPECT_FLOAT_EQ(6.0f, r.mMax);
	EXPECT_FLOAT_EQ(0.5f, RampT(r, 2.0f));
}

TEST(DiaValueSchemaRampResolver, MissingBoundMeansNoRangeAndUnitInterval)
{
	Fixture f;
	RampResolver res(*f.mService);
	ValueInfo info = FloatEntry("a");
	info.mHasMin = true; info.mMin = 5.0;
	const ResolvedRamp r = res.ResolveRamp(info);
	EXPECT_FALSE(r.mHasRange);
	EXPECT_FLOAT_EQ(0.0f, r.mMin);
	EXPECT_FLOAT_EQ(1.0f, r.mMax);
}

TEST(DiaValueSchemaRampResolver, RampTClampsAndHandlesDegenerateRange)
{
	ResolvedRamp r;
	r.mMin = 0.0f; r.mMax = 10.0f;
	EXPECT_FLOAT_EQ(0.0f, RampT(r, -5.0f));
	EXPECT_FLOAT_EQ(1.0f, RampT(r, 50.0f));
	EXPECT_FLOAT_EQ(0.25f, RampT(r, 2.5f));
	EXPECT_FLOAT_EQ(0.0f, RampT(r, std::numeric_limits<float>::quiet_NaN()));
	r.mMax = 0.0f;
	EXPECT_FLOAT_EQ(0.0f, RampT(r, 3.0f));
}

TEST(DiaValueSchemaRampResolver, PaletteUsesGivenColours)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ResolvedPalette p = res.ResolvePalette(EnumEntry("e", 3, 0b111));
	ASSERT_EQ(3u, p.mEntries.Size());
	EXPECT_EQ(0x102030FFu, p.mEntries[0].mRgba8);
	EXPECT_EQ(0x102030FFu, p.mEntries[2].mRgba8);
	EXPECT_STREQ("v1", p.mEntries[1].mId.AsCStr());
	EXPECT_FALSE(p.mWrapped);
	EXPECT_EQ(0x808080FFu, p.mFallbackRgba8);
}

TEST(DiaValueSchemaRampResolver, PaletteAssignsFromCategoricalByOrder)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ResolvedPalette p = res.ResolvePalette(EnumEntry("e", 3));
	ASSERT_EQ(3u, p.mEntries.Size());
	EXPECT_EQ(0xE9EA86FFu, p.mEntries[0].mRgba8);
	EXPECT_EQ(0xC826D4FFu, p.mEntries[1].mRgba8);
	EXPECT_EQ(0x6BC4E9FFu, p.mEntries[2].mRgba8);
}

TEST(DiaValueSchemaRampResolver, MixedGivenAndAssignedIndexByValueOrder)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ResolvedPalette p = res.ResolvePalette(EnumEntry("e", 3, 0b001));	// only value 0 has a colour
	ASSERT_EQ(3u, p.mEntries.Size());
	EXPECT_EQ(0x102030FFu, p.mEntries[0].mRgba8);
	EXPECT_EQ(0xC826D4FFu, p.mEntries[1].mRgba8);	// index 1, not 0
	EXPECT_EQ(0x6BC4E9FFu, p.mEntries[2].mRgba8);
}

TEST(DiaValueSchemaRampResolver, ThirteenColourlessValuesWrapAndWarnOncePerEntry)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ValueInfo e = EnumEntry("e", 13);
	const ResolvedPalette p = res.ResolvePalette(e);
	ASSERT_EQ(13u, p.mEntries.Size());
	EXPECT_TRUE(p.mWrapped);
	EXPECT_EQ(p.mEntries[0].mRgba8, p.mEntries[12].mRgba8);
	EXPECT_EQ(0x211EEBFFu, p.mEntries[11].mRgba8);
	res.ResolvePalette(e);
	EXPECT_EQ(1u, res.WrapWarningCount());
	res.ResolvePalette(EnumEntry("e2", 13));
	EXPECT_EQ(2u, res.WrapWarningCount());
}

TEST(DiaValueSchemaRampResolver, TwelveColourlessValuesDoNotWrap)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ResolvedPalette p = res.ResolvePalette(EnumEntry("e", 12));
	EXPECT_FALSE(p.mWrapped);
	EXPECT_EQ(0u, res.WrapWarningCount());
}

TEST(DiaValueSchemaRampResolver, NonEnumEntryGivesEmptyPalette)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ResolvedPalette p = res.ResolvePalette(FloatEntry("a"));
	EXPECT_EQ(0u, p.mEntries.Size());
	EXPECT_FALSE(p.mWrapped);
}

TEST(DiaValueSchemaRampResolver, PaletteWithoutRampsUsesBuiltInCategoricalList)
{
	Fixture f(false);
	RampResolver res(*f.mService);
	const ResolvedPalette p = res.ResolvePalette(EnumEntry("e", 2));
	ASSERT_EQ(2u, p.mEntries.Size());
	uint32_t expect = 0;
	ASSERT_TRUE(ParseHexColour(GetCategoricalColour(0), expect));
	EXPECT_EQ(expect, p.mEntries[0].mRgba8);
}

TEST(DiaValueSchemaRampResolver, DeltaRampDefaultsAndIsSymmetric)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ResolvedRamp r = res.ResolveDeltaRamp(FloatEntry("a"), 4.0f);
	EXPECT_STREQ("diverging.blue_red", r.mRamp.mName.AsCStr());
	EXPECT_FLOAT_EQ(-4.0f, r.mMin);
	EXPECT_FLOAT_EQ(4.0f, r.mMax);
	EXPECT_FLOAT_EQ(0.5f, RampT(r, 0.0f));
	EXPECT_EQ(0xF7F7F7FFu, SampleRamp(r.mRamp, RampT(r, 0.0f)));
	EXPECT_EQ(0u, res.UnknownRampWarningCount());
}

TEST(DiaValueSchemaRampResolver, DeltaRampOverrideIsUsed)
{
	Fixture f;
	RampResolver res(*f.mService);
	ValueInfo info = FloatEntry("a");
	info.mUi["delta_ramp"] = "sequential.heat";
	const ResolvedRamp r = res.ResolveDeltaRamp(info, 1.0f);
	EXPECT_STREQ("sequential.heat", r.mRamp.mName.AsCStr());
	EXPECT_FALSE(r.mFellBack);
}

TEST(DiaValueSchemaRampResolver, UnknownDeltaRampWarnsOnce)
{
	Fixture f;
	RampResolver res(*f.mService);
	ValueInfo info = FloatEntry("a");
	info.mUi["delta_ramp"] = "bogus";
	for (int i = 0; i < 5; ++i)
	{
		const ResolvedRamp r = res.ResolveDeltaRamp(info, 1.0f);
		EXPECT_TRUE(r.mFellBack);
		EXPECT_STREQ("diverging.blue_red", r.mRamp.mName.AsCStr());
	}
	EXPECT_EQ(1u, res.UnknownRampWarningCount());
}

TEST(DiaValueSchemaRampResolver, DeltaRampBadMaxAbsUsesOne)
{
	Fixture f;
	RampResolver res(*f.mService);
	const ValueInfo info = FloatEntry("a");
	ResolvedRamp r = res.ResolveDeltaRamp(info, 0.0f);
	EXPECT_FLOAT_EQ(-1.0f, r.mMin);
	EXPECT_FLOAT_EQ(1.0f, r.mMax);
	r = res.ResolveDeltaRamp(info, std::numeric_limits<float>::quiet_NaN());
	EXPECT_FLOAT_EQ(-1.0f, r.mMin);
	r = res.ResolveDeltaRamp(info, -3.0f);
	EXPECT_FLOAT_EQ(1.0f, r.mMax);
}

TEST(DiaValueSchemaRampResolver, ServiceWithoutRampsUsesBuiltInFallback)
{
	Fixture f(false);
	RampResolver res(*f.mService);
	const ResolvedRamp r = res.ResolveRamp(FloatEntry("a"));
	EXPECT_TRUE(r.mFellBack);
	EXPECT_EQ(0x000000FFu, SampleRamp(r.mRamp, 0.0f));	// built-in black -> white
	EXPECT_EQ(0xFFFFFFFFu, SampleRamp(r.mRamp, 1.0f));
	const ResolvedRamp d = res.ResolveDeltaRamp(FloatEntry("a"), 1.0f);
	EXPECT_TRUE(d.mFellBack);
	EXPECT_EQ(2u, res.FallbackCount());
}
