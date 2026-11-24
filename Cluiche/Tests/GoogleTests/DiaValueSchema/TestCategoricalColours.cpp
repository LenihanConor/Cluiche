#include <gtest/gtest.h>
#include <DiaValueSchema/CategoricalColours.h>
#include <DiaValueSchema/RampData.h>
#include <DiaValueSchema/RampLoader.h>
#include "ValueSchemaTestSupport.h"
#include <climits>
#include <cstring>
#include <set>
#include <string>

using namespace Dia::ValueSchema;

TEST(DiaValueSchemaCategoricalColours, TwelveValidDistinctHexColours)
{
	EXPECT_EQ(kCategoricalColourCount, 12u);
	std::set<uint32_t> seen;
	for (unsigned i = 0; i < kCategoricalColourCount; ++i)
	{
		const char* c = GetCategoricalColour(i);
		ASSERT_NE(c, nullptr);
		ASSERT_EQ(strlen(c), 7u) << i;
		uint32_t rgba = 0;
		ASSERT_TRUE(ParseHexColour(c, rgba)) << c;
		seen.insert(rgba);
		// Loader documentation promises "#RRGGBB" with upper-case digits.
		for (size_t k = 1; k < 7; ++k) { EXPECT_FALSE(c[k] >= 'a' && c[k] <= 'f') << c; }
	}
	EXPECT_EQ(seen.size(), 12u);
}

TEST(DiaValueSchemaCategoricalColours, IndexWrapsModuloTwelve)
{
	for (unsigned i = 0; i < 12; ++i) { EXPECT_STREQ(GetCategoricalColour(i + 12), GetCategoricalColour(i)); }
	EXPECT_STREQ(GetCategoricalColour(12), GetCategoricalColour(0));
	EXPECT_STREQ(GetCategoricalColour(13), GetCategoricalColour(1));
	EXPECT_STREQ(GetCategoricalColour(UINT_MAX), GetCategoricalColour(UINT_MAX % 12));
	EXPECT_STREQ(GetCategoricalColour(UINT_MAX), GetCategoricalColour(3));
	EXPECT_STREQ(GetCategoricalColour(1200000), GetCategoricalColour(0));
}

TEST(DiaValueSchemaCategoricalColours, ReturnedPointersAreStable)
{
	EXPECT_EQ(GetCategoricalColour(5), GetCategoricalColour(5));
	EXPECT_EQ(GetCategoricalColour(5), GetCategoricalColour(17));
}

TEST(DiaValueSchemaCategoricalColours, PinBuiltInListDiffersFromShippedCategorical12)
{
	// PIN: CategoricalColours.h says Feature 5's `categorical.12` ramp "replaces" the built-in list, but the two are
	// different palettes today (the loader still assigns enum colours from the built-in list). If someone aligns them,
	// this test must be turned into an equality check of the first 12 colours.
	const auto path = VsTest::FindUpwards(__FILE__, "Dia/DiaValueSchema/Data/ramps.json");
	ASSERT_FALSE(path.empty()) << "ramps.json not found";
	std::string text;
	ASSERT_TRUE(VsTest::ReadFileBytes(path, text));
	const RampLoadResult r = RampLoader::LoadFromText(text.c_str(), "ramps.json");
	ASSERT_TRUE(r.mOk) << r.mFileReason.AsCStr();

	const RampData* cat = nullptr;
	for (unsigned i = 0; i < r.mRamps.Size(); ++i)
	{
		if (strcmp(r.mRamps[i].mName.AsCStr(), "categorical.12") == 0) { cat = &r.mRamps[i]; }
	}
	ASSERT_NE(cat, nullptr);
	ASSERT_EQ(cat->mKind, RampKind::Categorical);
	ASSERT_EQ(cat->mColourCount, kCategoricalColourCount);	// same size as the built-in list
	unsigned equal = 0;
	for (unsigned i = 0; i < kCategoricalColourCount; ++i)
	{
		uint32_t builtIn = 0;
		ASSERT_TRUE(ParseHexColour(GetCategoricalColour(i), builtIn));
		if (cat->mColours[i] == builtIn) { ++equal; }
	}
	EXPECT_LT(equal, kCategoricalColourCount) << "built-in list and shipped categorical.12 are now identical: convert this PIN to an equality test";
	EXPECT_EQ(equal, 0u);
}
