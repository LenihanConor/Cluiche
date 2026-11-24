#include <gtest/gtest.h>
#include "RampTestSupport.h"
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

// Exhaustive edge/property/golden tests for BuildTicks and BuildLegend (Feature 5 hardening).
//
// PINNED behaviour:
//  - Int entries get the same (possibly fractional) ticks as Float entries: a 0..1 Int range shows 0, 0.5, 1.
//  - The seen range is passed through even when it lies outside [min,max].
//  - Non-float/int/enum types give an empty legend and ignore hasSeen.

using namespace Dia::ValueSchema;
using VsTest::RampFixture;
using VsTest::FloatEntry;
using VsTest::EnumEntry;

namespace
{
	constexpr float kInf = std::numeric_limits<float>::infinity();
	constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

	std::vector<float> Ticks(float mn, float mx)
	{
		float t[kMaxLegendTicks];
		const unsigned n = BuildTicks(mn, mx, t);
		return std::vector<float>(t, t + n);
	}

	ValueInfo Range(double mn, double mx, const char* ramp = nullptr)
	{
		ValueInfo info = FloatEntry("r", ramp);
		info.mHasMin = true; info.mMin = mn;
		info.mHasMax = true; info.mMax = mx;
		return info;
	}

	std::string Describe(const std::vector<float>& v)
	{
		std::string s;
		char buf[64];
		for (float f : v) { snprintf(buf, sizeof(buf), "%.9g ", f); s += buf; }
		return s;
	}

	// Properties every non-degenerate tick set must have. Returns false (with a gtest failure) when violated.
	void ExpectWellFormed(float mn, float mx, const std::vector<float>& t)
	{
		const std::string ctx = "range " + std::to_string(mn) + ".." + std::to_string(mx) + " ticks " + Describe(t);
		ASSERT_GE(t.size(), 2u) << ctx;
		ASSERT_LE(t.size(), static_cast<size_t>(kMaxLegendTicks)) << ctx;
		EXPECT_LE(t.size() - 2, 3u) << ctx;
		EXPECT_EQ(mn, t.front()) << ctx;
		EXPECT_EQ(mx, t.back()) << ctx;
		for (size_t i = 0; i < t.size(); ++i)
		{
			EXPECT_FALSE(std::isnan(t[i])) << ctx;
			EXPECT_GE(t[i], mn) << ctx;
			EXPECT_LE(t[i], mx) << ctx;
			if (i > 0) { EXPECT_LT(t[i - 1], t[i]) << ctx; }
		}
	}

	// Interior ticks (>= 2 of them) must be equally spaced by {1,2,5} x 10^n and lie on multiples of that step.
	void ExpectNiceSpacing(float mn, float mx, const std::vector<float>& t)
	{
		if (t.size() < 4) { return; }
		const std::string ctx = "range " + std::to_string(mn) + ".." + std::to_string(mx) + " ticks " + Describe(t);
		const double step = static_cast<double>(t[2]) - static_cast<double>(t[1]);
		const double maxAbs = std::max(std::fabs(static_cast<double>(mn)), std::fabs(static_cast<double>(mx)));
		const double tol = 8.0 * FLT_EPSILON * maxAbs + 1e-5 * step;
		for (size_t i = 2; i + 1 < t.size(); ++i)
		{
			EXPECT_NEAR(step, static_cast<double>(t[i]) - static_cast<double>(t[i - 1]), tol) << ctx;
		}
		// The measured step carries float rounding; snap it to the nearest {1,2,5} x 10^n before checking the multiples.
		const double relErr = tol / step + 1e-3;
		const int e = static_cast<int>(std::floor(std::log10(step) + 1e-9));
		const double mant = step / std::pow(10.0, e);
		double snapped = 0.0;
		for (double c : { 1.0, 2.0, 5.0, 10.0 })
		{
			if (std::fabs(mant - c) <= c * relErr) { snapped = c * std::pow(10.0, e); }
		}
		ASSERT_GT(snapped, 0.0) << ctx << " step " << step << " mant " << mant;
		for (size_t i = 1; i + 1 < t.size(); ++i)
		{
			const double k = static_cast<double>(t[i]) / snapped;
			EXPECT_NEAR(k, std::round(k), tol / snapped + 1e-3) << ctx << " tick " << i;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------
// BuildTicks: degenerate and extreme inputs
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampTicksEdge, ZeroToZeroIsSingleTick)
{
	EXPECT_EQ((std::vector<float>{ 0.0f }), Ticks(0.0f, 0.0f));
}

TEST(DiaValueSchemaRampTicksEdge, NegativeZeroToZeroIsSingleTick)
{
	const std::vector<float> t = Ticks(-0.0f, 0.0f);
	ASSERT_EQ(1u, t.size());
	EXPECT_EQ(0.0f, t[0]);
	EXPECT_EQ(1u, Ticks(0.0f, -0.0f).size());
}

TEST(DiaValueSchemaRampTicksEdge, InvertedRangeIsSingleTickAtMin)
{
	EXPECT_EQ((std::vector<float>{ 5.0f }), Ticks(5.0f, -5.0f));
	EXPECT_EQ((std::vector<float>{ 1e-20f }), Ticks(1e-20f, 1e-30f));
	EXPECT_EQ((std::vector<float>{ FLT_MAX }), Ticks(FLT_MAX, -FLT_MAX));
}

TEST(DiaValueSchemaRampTicksEdge, FullFloatRange)
{
	const std::vector<float> t = Ticks(-FLT_MAX, FLT_MAX);
	ExpectWellFormed(-FLT_MAX, FLT_MAX, t);
	EXPECT_EQ((std::vector<float>{ -FLT_MAX, -2.0e38f, 0.0f, 2.0e38f, FLT_MAX }), t);
}

TEST(DiaValueSchemaRampTicksEdge, PositiveHalfOfFloatRange)
{
	ExpectWellFormed(0.0f, FLT_MAX, Ticks(0.0f, FLT_MAX));
	ExpectWellFormed(-FLT_MAX, 0.0f, Ticks(-FLT_MAX, 0.0f));
}

TEST(DiaValueSchemaRampTicksEdge, ExtremeButFiniteBoundsNeverCrash)
{
	const float vals[] = { -FLT_MAX, -1e38f, -1e30f, -1.0f, -FLT_MIN, 0.0f, FLT_MIN, 1.0f, 1e30f, 1e38f, FLT_MAX };
	for (float a : vals)
	{
		for (float b : vals)
		{
			if (!(b > a)) { continue; }
			ExpectWellFormed(a, b, Ticks(a, b));
		}
	}
}

TEST(DiaValueSchemaRampTicksEdge, SubnormalRange)
{
	const float a = 1e-40f, b = 2e-40f;		// both subnormal
	ASSERT_LT(a, FLT_MIN);
	ExpectWellFormed(a, b, Ticks(a, b));
}

TEST(DiaValueSchemaRampTicksEdge, SmallestSubnormalSteps)
{
	const float d = std::numeric_limits<float>::denorm_min();
	for (int span = 1; span <= 40; ++span)
	{
		const float mx = d * static_cast<float>(span);
		ExpectWellFormed(0.0f, mx, Ticks(0.0f, mx));
	}
}

TEST(DiaValueSchemaRampTicksEdge, OneUlpRangeHasNoDuplicateTicks)
{
	const float a = 1.0f, b = std::nextafter(1.0f, 2.0f);
	const std::vector<float> t = Ticks(a, b);
	ExpectWellFormed(a, b, t);
	EXPECT_EQ((std::vector<float>{ a, b }), t);
}

TEST(DiaValueSchemaRampTicksEdge, OneUlpRangesAtManyMagnitudes)
{
	for (float a : { 1e-30f, 1e-10f, 0.001f, 0.3f, 1.0f, 3.0f, 100.0f, 12345.678f, 1e7f, 1.6e7f, 1e20f, -1.0f, -1e7f, -1e-10f })
	{
		const float b = std::nextafter(a, kInf);
		ExpectWellFormed(a, b, Ticks(a, b));
	}
}

TEST(DiaValueSchemaRampTicksEdge, TenMillionPlusTwenty)
{
	const std::vector<float> t = Ticks(1e7f, 1e7f + 20.0f);
	ExpectWellFormed(1e7f, 1e7f + 20.0f, t);
	EXPECT_EQ((std::vector<float>{ 1e7f, 1e7f + 5.0f, 1e7f + 10.0f, 1e7f + 15.0f, 1e7f + 20.0f }), t);
}

TEST(DiaValueSchemaRampTicksEdge, FloatIntegerPrecisionBoundary)
{
	// 16777217 is not representable as a float: the interior multiple must not collapse onto an end tick.
	const std::vector<float> t = Ticks(16777216.0f, 16777218.0f);
	ExpectWellFormed(16777216.0f, 16777218.0f, t);
	EXPECT_EQ((std::vector<float>{ 16777216.0f, 16777218.0f }), t);
}

TEST(DiaValueSchemaRampTicksEdge, NonFiniteEndsGiveNoTicks)
{
	for (float a : { kNaN, kInf, -kInf, 0.0f })
	{
		for (float b : { kNaN, kInf, -kInf, 1.0f })
		{
			const bool bothFinite = std::isfinite(a) && std::isfinite(b);
			if (bothFinite) { continue; }
			EXPECT_EQ(0u, Ticks(a, b).size()) << a << ".." << b;
		}
	}
}

TEST(DiaValueSchemaRampTicksEdge, EnormousDynamicRange)
{
	const std::vector<float> t = Ticks(1e-30f, 1e30f);
	ExpectWellFormed(1e-30f, 1e30f, t);
	EXPECT_EQ((std::vector<float>{ 1e-30f, 5e29f, 1e30f }), t);
}

TEST(DiaValueSchemaRampTicksEdge, NegativeEnormousDynamicRange)
{
	ExpectWellFormed(-1e30f, -1e-30f, Ticks(-1e30f, -1e-30f));
	ExpectWellFormed(-1e30f, 1e-30f, Ticks(-1e30f, 1e-30f));
}

TEST(DiaValueSchemaRampTicksEdge, DocumentedExamplesStillHold)
{
	EXPECT_EQ((std::vector<float>{ 0, 0.5f, 1 }), Ticks(0, 1));
	EXPECT_EQ((std::vector<float>{ 0, 2, 4, 6, 7 }), Ticks(0, 7));
	EXPECT_EQ((std::vector<float>{ -1, -0.5f, 0, 0.5f, 1 }), Ticks(-1, 1));
	EXPECT_EQ((std::vector<float>{ 3, 5, 10, 15, 17 }), Ticks(3, 17));
}

TEST(DiaValueSchemaRampTicksEdge, IsDeterministic)
{
	for (float a : { 0.0f, -3.3f, 17.0f })
	{
		EXPECT_EQ(Ticks(a, a + 41.7f), Ticks(a, a + 41.7f));
	}
}

// ---------------------------------------------------------------------------------------------------------
// BuildTicks: properties
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampTicksEdgeProperty, RandomLogUniformRangesAreWellFormedAndNice)
{
	std::mt19937 rng(1234);
	std::uniform_real_distribution<double> exp10(-20.0, 20.0);
	std::uniform_real_distribution<double> relWidthExp(-5.0, 2.0);
	std::uniform_int_distribution<int> sign(0, 1);
	for (int i = 0; i < 4000; ++i)
	{
		const double base = (sign(rng) ? 1.0 : -1.0) * std::pow(10.0, exp10(rng));
		const double width = std::fabs(base) * std::pow(10.0, relWidthExp(rng));
		const float mn = static_cast<float>(base);
		const float mx = static_cast<float>(base + width);
		if (!(mx > mn)) { continue; }
		const std::vector<float> t = Ticks(mn, mx);
		ExpectWellFormed(mn, mx, t);
		ExpectNiceSpacing(mn, mx, t);
		ASSERT_EQ(t, Ticks(mn, mx)) << "not deterministic";
		if (::testing::Test::HasFailure()) { FAIL() << "first failing range " << mn << ".." << mx; }
	}
}

TEST(DiaValueSchemaRampTicksEdgeProperty, RandomIndependentEndsLogUniform)
{
	std::mt19937 rng(1234);
	std::uniform_real_distribution<double> exp10(-15.0, 15.0);
	std::uniform_int_distribution<int> sign(0, 1);
	for (int i = 0; i < 4000; ++i)
	{
		float a = static_cast<float>((sign(rng) ? 1.0 : -1.0) * std::pow(10.0, exp10(rng)));
		float b = static_cast<float>((sign(rng) ? 1.0 : -1.0) * std::pow(10.0, exp10(rng)));
		if (b < a) { std::swap(a, b); }
		if (!(b > a)) { continue; }
		const std::vector<float> t = Ticks(a, b);
		ExpectWellFormed(a, b, t);
		ExpectNiceSpacing(a, b, t);
		if (::testing::Test::HasFailure()) { FAIL() << "first failing range " << a << ".." << b; }
	}
}

TEST(DiaValueSchemaRampTicksEdgeProperty, SymmetricRangesGiveSymmetricTicks)
{
	std::mt19937 rng(1234);
	std::uniform_real_distribution<double> exp10(-30.0, 30.0);
	for (int i = 0; i < 2000; ++i)
	{
		const float a = static_cast<float>(std::pow(10.0, exp10(rng)) * (1.0 + static_cast<double>(rng() % 1000) / 333.0));
		const std::vector<float> t = Ticks(-a, a);
		ASSERT_GE(t.size(), 3u);
		for (size_t k = 0; k < t.size(); ++k) { EXPECT_EQ(t[k], -t[t.size() - 1 - k]) << a << " k=" << k; }
		if (::testing::Test::HasFailure()) { FAIL() << "first failing a " << a; }
	}
}

TEST(DiaValueSchemaRampTicksEdgeProperty, SweepOfIntegerRangesHasOneToThreeInterior)
{
	for (int lo = -30; lo <= 30; lo += 3)
	{
		for (int width = 1; width <= 400; width += 13)
		{
			const float a = static_cast<float>(lo), b = static_cast<float>(lo + width);
			const std::vector<float> t = Ticks(a, b);
			ExpectWellFormed(a, b, t);
			ExpectNiceSpacing(a, b, t);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------
// BuildLegend: float / int
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampLegendEdge, IntEntryGetsFractionalTicks)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = Range(0, 1);
	info.mType = ValueType::Int;
	const LegendData l = BuildLegend(res, info, false, 0, 0);
	EXPECT_EQ((std::vector<float>{ 0.0f, 0.5f, 1.0f }), std::vector<float>(l.mTicks, l.mTicks + l.mTickCount));
}

TEST(DiaValueSchemaRampLegendEdge, SeenRangeWithEqualEnds)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const LegendData l = BuildLegend(res, Range(0, 10), true, 4.0f, 4.0f);
	EXPECT_TRUE(l.mHasSeen);
	EXPECT_EQ(4.0f, l.mSeenMin);
	EXPECT_EQ(4.0f, l.mSeenMax);
}

TEST(DiaValueSchemaRampLegendEdge, SeenRangeNonFiniteEitherEndMeansNoSeen)
{
	RampFixture f;
	RampResolver res(f.Svc());
	for (float bad : { kNaN, kInf, -kInf })
	{
		const LegendData a = BuildLegend(res, Range(0, 10), true, bad, 5.0f);
		const LegendData b = BuildLegend(res, Range(0, 10), true, 5.0f, bad);
		EXPECT_FALSE(a.mHasSeen);
		EXPECT_FALSE(b.mHasSeen);
		EXPECT_EQ(0.0f, a.mSeenMin);
		EXPECT_EQ(0.0f, a.mSeenMax);
	}
}

TEST(DiaValueSchemaRampLegendEdge, SeenRangeOutsideValueRangeIsPassedThrough)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const LegendData l = BuildLegend(res, Range(0, 10), true, -100.0f, 100.0f);
	EXPECT_TRUE(l.mHasSeen);
	EXPECT_EQ(-100.0f, l.mSeenMin);
	EXPECT_EQ(100.0f, l.mSeenMax);
	const LegendData swapped = BuildLegend(res, Range(0, 10), true, 100.0f, -100.0f);
	EXPECT_EQ(-100.0f, swapped.mSeenMin);
	EXPECT_EQ(100.0f, swapped.mSeenMax);
}

TEST(DiaValueSchemaRampLegendEdge, SeenRangeNegativeZero)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const LegendData l = BuildLegend(res, Range(0, 10), true, 0.0f, -0.0f);
	EXPECT_TRUE(l.mHasSeen);
	EXPECT_EQ(0.0f, l.mSeenMin);
	EXPECT_EQ(0.0f, l.mSeenMax);
}

TEST(DiaValueSchemaRampLegendEdge, UnitOfExactlyThirtyOneCharsIsKept)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = Range(0, 10);
	const std::string unit(31, 'u');
	SetText(info.mUnit, unit.c_str());
	const LegendData l = BuildLegend(res, info, false, 0, 0);
	EXPECT_STREQ(unit.c_str(), l.mUnit.AsCStr());
}

TEST(DiaValueSchemaRampLegendEdge, UnitWithFormatTokensIsKeptVerbatim)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = Range(0, 10);
	SetText(info.mUnit, "%s%d%n%");
	const LegendData l = BuildLegend(res, info, false, 0, 0);
	EXPECT_STREQ("%s%d%n%", l.mUnit.AsCStr());
}

TEST(DiaValueSchemaRampLegendEdge, FloatLegendFlagsFallbackRamp)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const LegendData l = BuildLegend(res, Range(0, 10, "nope"), false, 0, 0);
	EXPECT_TRUE(l.mRamp.mFellBack);
	EXPECT_STREQ("sequential.grey", l.mRamp.mRamp.mName.AsCStr());
	EXPECT_EQ(3u, l.mTickCount);
}

TEST(DiaValueSchemaRampLegendEdge, FloatLegendWithRangeBeyondFloatHasFiniteTicks)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const LegendData l = BuildLegend(res, Range(-1e300, 1e300), false, 0, 0);
	EXPECT_EQ(-FLT_MAX, l.mMin);
	EXPECT_EQ(FLT_MAX, l.mMax);
	ASSERT_GE(l.mTickCount, 2u);
	for (unsigned i = 0; i < l.mTickCount; ++i) { EXPECT_TRUE(std::isfinite(l.mTicks[i])); }
}

TEST(DiaValueSchemaRampLegendEdge, InvertedEntryRangeGivesSingleTick)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const LegendData l = BuildLegend(res, Range(10, 0), false, 0, 0);
	ASSERT_EQ(1u, l.mTickCount);
	EXPECT_EQ(10.0f, l.mTicks[0]);
}

TEST(DiaValueSchemaRampLegendEdge, LegendMinMaxMirrorRampRange)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const LegendData l = BuildLegend(res, Range(-2, 6, "sequential.blue"), false, 0, 0);
	EXPECT_EQ(l.mRamp.mMin, l.mMin);
	EXPECT_EQ(l.mRamp.mMax, l.mMax);
	EXPECT_TRUE(l.mRamp.mHasRange);
}

// ---------------------------------------------------------------------------------------------------------
// BuildLegend: other types
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampLegendEdge, NonRampTypesGiveFullyEmptyLegends)
{
	RampFixture f;
	RampResolver res(f.Svc());
	for (ValueType t : { ValueType::Bool, ValueType::String, ValueType::Ref, ValueType::RefList, ValueType::Map })
	{
		ValueInfo info = Range(0, 10);
		SetText(info.mUnit, "u");
		info.mType = t;
		const LegendData l = BuildLegend(res, info, true, 1.0f, 2.0f);
		EXPECT_EQ(0u, l.mTickCount);
		EXPECT_EQ(0u, l.mSwatches.Size());
		EXPECT_FALSE(l.mIsCategory);
		EXPECT_FALSE(l.mHasSeen);
		EXPECT_STREQ("", l.mUnit.AsCStr());
		EXPECT_EQ(0u, l.mRamp.mRamp.mStopCount);
		EXPECT_FALSE(l.mRamp.mFellBack);
	}
	EXPECT_EQ(0u, res.FallbackCount());
}

TEST(DiaValueSchemaRampLegendEdge, EnumWithZeroValues)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const LegendData l = BuildLegend(res, EnumEntry("e", 0), false, 0, 0);
	EXPECT_TRUE(l.mIsCategory);
	EXPECT_EQ(0u, l.mSwatches.Size());
	EXPECT_EQ(0u, l.mTickCount);
}

TEST(DiaValueSchemaRampLegendEdge, EnumWithOneValueIdOnly)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const LegendData l = BuildLegend(res, EnumEntry("e", 1), false, 0, 0);
	ASSERT_EQ(1u, l.mSwatches.Size());
	EXPECT_STREQ("v0", l.mSwatches[0].mId.AsCStr());
	EXPECT_STREQ("v0", l.mSwatches[0].mLabel.AsCStr());
}

TEST(DiaValueSchemaRampLegendEdge, EnumThousandValuesWithMaxLengthLabels)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = EnumEntry("e", 1000);
	for (unsigned i = 0; i < 1000; ++i)
	{
		std::string label = std::to_string(i) + ":";
		label.resize(127, 'L');
		SetText(info.mEnumValues[i].mLabel, label.c_str());
	}
	const LegendData l = BuildLegend(res, info, false, 0, 0);
	ASSERT_EQ(1000u, l.mSwatches.Size());
	for (unsigned i = 0; i < 1000; ++i)
	{
		ASSERT_EQ(127u, strlen(l.mSwatches[i].mLabel.AsCStr())) << i;
		EXPECT_EQ(0, strncmp(l.mSwatches[i].mLabel.AsCStr(), (std::to_string(i) + ":").c_str(), std::to_string(i).size() + 1)) << i;
	}
}

TEST(DiaValueSchemaRampLegendEdge, EnumThousandValuesIdOnly)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const LegendData l = BuildLegend(res, EnumEntry("e", 1000), false, 0, 0);
	ASSERT_EQ(1000u, l.mSwatches.Size());
	for (unsigned i = 0; i < 1000; ++i) { EXPECT_STREQ(l.mSwatches[i].mId.AsCStr(), l.mSwatches[i].mLabel.AsCStr()); }
}

TEST(DiaValueSchemaRampLegendEdge, EnumLongestIdAsLabel)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = EnumEntry("e", 1);
	const std::string id(63, 'i');
	SetText(info.mEnumValues[0].mId, id.c_str());
	const LegendData l = BuildLegend(res, info, false, 0, 0);
	EXPECT_STREQ(id.c_str(), l.mSwatches[0].mLabel.AsCStr());
	EXPECT_STREQ(id.c_str(), l.mSwatches[0].mId.AsCStr());
}

TEST(DiaValueSchemaRampLegendEdge, EnumFormatTokensInIdLabelAndEmptyLabelFallback)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = EnumEntry("e", 3);
	SetText(info.mEnumValues[0].mId, "100%");
	SetText(info.mEnumValues[1].mId, "%s%d%n");
	SetText(info.mEnumValues[1].mLabel, "%x %n lab");
	SetText(info.mEnumValues[2].mId, "%%");
	const LegendData l = BuildLegend(res, info, false, 0, 0);
	EXPECT_STREQ("100%", l.mSwatches[0].mId.AsCStr());
	EXPECT_STREQ("100%", l.mSwatches[0].mLabel.AsCStr());		// id fallback, verbatim
	EXPECT_STREQ("%s%d%n", l.mSwatches[1].mId.AsCStr());
	EXPECT_STREQ("%x %n lab", l.mSwatches[1].mLabel.AsCStr());
	EXPECT_STREQ("%%", l.mSwatches[2].mLabel.AsCStr());
}

TEST(DiaValueSchemaRampLegendEdge, SwatchColourEqualsPaletteEntry)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const ValueInfo info = EnumEntry("e", 30, 0x0F0F0F0Fu);
	const LegendData l = BuildLegend(res, info, false, 0, 0);
	const ResolvedPalette p = res.ResolvePalette(info);
	ASSERT_EQ(30u, l.mSwatches.Size());
	ASSERT_EQ(30u, p.mEntries.Size());
	for (unsigned i = 0; i < 30; ++i)
	{
		EXPECT_EQ(p.mEntries[i].mRgba8, l.mSwatches[i].mRgba8) << i;
		EXPECT_STREQ(p.mEntries[i].mId.AsCStr(), l.mSwatches[i].mId.AsCStr());
	}
}

TEST(DiaValueSchemaRampLegendEdge, WrapWarningOnceAcrossPaletteAndLegendCalls)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const ValueInfo info = EnumEntry("e", 14);
	res.ResolvePalette(info);
	BuildLegend(res, info, false, 0, 0);
	BuildLegend(res, info, false, 0, 0);
	res.ResolvePalette(info);
	EXPECT_EQ(1u, res.WrapWarningCount());
	BuildLegend(res, EnumEntry("e2", 14), false, 0, 0);
	EXPECT_EQ(2u, res.WrapWarningCount());
}

TEST(DiaValueSchemaRampLegendEdge, EnumLegendIgnoresHasSeenAndRange)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = EnumEntry("e", 3);
	info.mHasMin = true; info.mMin = 0; info.mHasMax = true; info.mMax = 5;
	const LegendData l = BuildLegend(res, info, true, 1.0f, 2.0f);
	EXPECT_TRUE(l.mIsCategory);
	EXPECT_FALSE(l.mHasSeen);
	EXPECT_EQ(0u, l.mTickCount);
}

TEST(DiaValueSchemaRampLegendEdge, LegendCopyAssignOverNonEmpty)
{
	RampFixture f;
	RampResolver res(f.Svc());
	LegendData a = BuildLegend(res, EnumEntry("e", 5), false, 0, 0);
	LegendData b = BuildLegend(res, EnumEntry("e", 9), false, 0, 0);
	b = a;
	ASSERT_EQ(5u, b.mSwatches.Size());
	EXPECT_STREQ("v4", b.mSwatches[4].mId.AsCStr());
}

// ---------------------------------------------------------------------------------------------------------
// Golden legends for every shipped continuous ramp
// ---------------------------------------------------------------------------------------------------------

namespace
{
	std::string DumpLegends(const RampFixture& f)
	{
		RampResolver res(f.Svc());
		std::string out;
		const char* ids[] = { "sequential.green", "sequential.blue", "sequential.heat", "sequential.grey", "sequential.purple",
			"sequential.terrain", "diverging.blue_red" };
		const double ranges[3][2] = { { 0, 1 }, { -50, 50 }, { 3, 17 } };
		char buf[128];
		for (const char* id : ids)
		{
			for (const auto& rg : ranges)
			{
				const LegendData l = BuildLegend(res, Range(rg[0], rg[1], id), false, 0, 0);
				snprintf(buf, sizeof(buf), "%s %g..%g |", l.mRamp.mRamp.mName.AsCStr(), l.mMin, l.mMax);
				out += buf;
				for (unsigned i = 0; i < l.mTickCount; ++i)
				{
					const uint32_t c = SampleRamp(l.mRamp.mRamp, RampT(l.mRamp, l.mTicks[i]));
					snprintf(buf, sizeof(buf), " %g:%08X", static_cast<double>(l.mTicks[i]), c);
					out += buf;
				}
				out += "\n";
			}
		}
		return out;
	}

	// Computed independently (float32-emulating script over the shipped ramps.json) and checked against SampleRamp.
	const char* const kGoldenLegends =
		"sequential.green 0..1 | 0:F7FCF5FF 0.5:74C476FF 1:00441BFF\n"
		"sequential.green -50..50 | -50:F7FCF5FF 0:74C476FF 50:00441BFF\n"
		"sequential.green 3..17 | 3:F7FCF5FF 5:E1F3DBFF 10:74C476FF 15:057130FF 17:00441BFF\n"
		"sequential.blue 0..1 | 0:F7FBFFFF 0.5:6BAED6FF 1:08306BFF\n"
		"sequential.blue -50..50 | -50:F7FBFFFF 0:6BAED6FF 50:08306BFF\n"
		"sequential.blue 3..17 | 3:F7FBFFFF 5:DBE9F6FF 10:6BAED6FF 15:0C56A0FF 17:08306BFF\n"
		"sequential.heat 0..1 | 0:FFFFCCFF 0.5:FD8D3CFF 1:800026FF\n"
		"sequential.heat -50..50 | -50:FFFFCCFF 0:FD8D3CFF 50:800026FF\n"
		"sequential.heat 3..17 | 3:FFFFCCFF 5:FFEA9AFF 10:FD8D3CFF 15:C20425FF 17:800026FF\n"
		"sequential.grey 0..1 | 0:FFFFFFFF 0.5:969696FF 1:000000FF\n"
		"sequential.grey -50..50 | -50:FFFFFFFF 0:969696FF 50:000000FF\n"
		"sequential.grey 3..17 | 3:FFFFFFFF 5:EDEDEDFF 10:969696FF 15:2B2B2BFF 17:000000FF\n"
		"sequential.purple 0..1 | 0:FCFBFDFF 0.5:9E9AC8FF 1:3F007DFF\n"
		"sequential.purple -50..50 | -50:FCFBFDFF 0:9E9AC8FF 50:3F007DFF\n"
		"sequential.purple 3..17 | 3:FCFBFDFF 5:ECEAF4FF 10:9E9AC8FF 15:572D92FF 17:3F007DFF\n"
		"sequential.terrain 0..1 | 0:3B6FA8FF 0.5:C9C27AFF 1:F5F5F5FF\n"
		"sequential.terrain -50..50 | -50:3B6FA8FF 0:C9C27AFF 50:F5F5F5FF\n"
		"sequential.terrain 3..17 | 3:3B6FA8FF 5:8BBDD7FF 10:C9C27AFF 15:806149FF 17:F5F5F5FF\n"
		"diverging.blue_red 0..1 | 0:053061FF 0.5:F7F7F7FF 1:67001FFF\n"
		"diverging.blue_red -50..50 | -50:053061FF 0:F7F7F7FF 50:67001FFF\n"
		"diverging.blue_red 3..17 | 3:053061FF 5:3079B6FF 10:F7F7F7FF 15:C7494BFF 17:67001FFF\n";
}

TEST(DiaValueSchemaRampLegendEdgeGolden, ShippedRampsOverThreeRanges)
{
	RampFixture f;
	EXPECT_EQ(std::string(kGoldenLegends), DumpLegends(f));
}

TEST(DiaValueSchemaRampLegendEdgeGolden, DumpIsDeterministicAcrossFixtures)
{
	RampFixture a, b;
	EXPECT_EQ(DumpLegends(a), DumpLegends(b));
}
