#include <gtest/gtest.h>
#include "RampTestSupport.h"
#include <DiaValueSchema/CategoricalColours.h>
#include <DiaObservation/Metric/Counter.h>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <limits>
#include <random>
#include <string>
#include <thread>
#include <vector>

// Exhaustive edge tests for RampResolver / RampT (Feature 5 hardening).
//
// PINNED behaviour (documented by tests below, not necessarily "ideal"):
//  - RampResolver is single-threaded (one resolver per thread); the shared const ValueSchemaService is safe to share.
//  - Warn-key spaces are per kind: ui.ramp, ui.delta_ramp and the palette wrap warning for the same entry each warn once.
//  - The built-in fallback ramp runs black -> white, the shipped sequential.grey runs white -> black.
//  - FallbackCount counts once per resolve call that could not honour the request, never per key.

using namespace Dia::ValueSchema;
using VsTest::RampFixture;
using VsTest::FloatEntry;
using VsTest::EnumEntry;

namespace
{
	constexpr float kInf = std::numeric_limits<float>::infinity();
	constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

	uint32_t ParseOrDie(const char* hex)
	{
		uint32_t v = 0;
		EXPECT_TRUE(ParseHexColour(hex, v));
		return v;
	}

	RampData MakeCategorical(const char* id, unsigned count)
	{
		RampData r;
		SetText(r.mName, id);
		r.mId = Dia::Core::StringCRC(id);
		r.mKind = RampKind::Categorical;
		for (unsigned i = 0; i < count; ++i) { r.mColours[i] = 0x10000000u * (i + 1) | 0xFFu; }
		r.mColourCount = count;
		return r;
	}

	ResolvedRamp Range(float mn, float mx)
	{
		ResolvedRamp r;
		r.mMin = mn; r.mMax = mx;
		return r;
	}
}

// ---------------------------------------------------------------------------------------------------------
// RampT
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampTEdge, FullFloatRangeIsNotNaN)
{
	const ResolvedRamp r = Range(-FLT_MAX, FLT_MAX);
	EXPECT_FLOAT_EQ(0.5f, RampT(r, 0.0f));
	EXPECT_FLOAT_EQ(0.0f, RampT(r, -FLT_MAX));
	EXPECT_FLOAT_EQ(1.0f, RampT(r, FLT_MAX));
	EXPECT_FLOAT_EQ(1.0f, RampT(r, kInf));
	EXPECT_FLOAT_EQ(0.0f, RampT(r, -kInf));
	EXPECT_FLOAT_EQ(0.75f, RampT(r, FLT_MAX / 2.0f));
}

TEST(DiaValueSchemaRampTEdge, WidthBeyondFloatMaxStillScales)
{
	const ResolvedRamp r = Range(-3.0e38f, 3.0e38f);	// width 6e38 overflows float
	EXPECT_FLOAT_EQ(0.5f, RampT(r, 0.0f));
	EXPECT_FLOAT_EQ(1.0f, RampT(r, 3.0e38f));
	EXPECT_FLOAT_EQ(0.0f, RampT(r, -3.0e38f));
	EXPECT_NEAR(0.75f, RampT(r, 1.5e38f), 1e-6f);
}

TEST(DiaValueSchemaRampTEdge, NonFiniteBoundsGiveZero)
{
	EXPECT_FLOAT_EQ(0.0f, RampT(Range(-kInf, kInf), 0.0f));
	EXPECT_FLOAT_EQ(0.0f, RampT(Range(0.0f, kInf), 5.0f));
	EXPECT_FLOAT_EQ(0.0f, RampT(Range(-kInf, 0.0f), -5.0f));
	EXPECT_FLOAT_EQ(0.0f, RampT(Range(kNaN, 1.0f), 0.5f));
	EXPECT_FLOAT_EQ(0.0f, RampT(Range(0.0f, kNaN), 0.5f));
}

TEST(DiaValueSchemaRampTEdge, InfiniteValuesClamp)
{
	const ResolvedRamp r = Range(0.0f, 10.0f);
	EXPECT_FLOAT_EQ(1.0f, RampT(r, kInf));
	EXPECT_FLOAT_EQ(0.0f, RampT(r, -kInf));
	EXPECT_FLOAT_EQ(0.0f, RampT(r, kNaN));
	EXPECT_FLOAT_EQ(0.0f, RampT(r, -kNaN));
}

TEST(DiaValueSchemaRampTEdge, DegenerateRangesGiveZero)
{
	EXPECT_FLOAT_EQ(0.0f, RampT(Range(5.0f, 5.0f), 5.0f));
	EXPECT_FLOAT_EQ(0.0f, RampT(Range(5.0f, 5.0f), 100.0f));
	EXPECT_FLOAT_EQ(0.0f, RampT(Range(10.0f, 0.0f), 5.0f));		// inverted
	EXPECT_FLOAT_EQ(0.0f, RampT(Range(0.0f, -0.0f), 0.0f));
	EXPECT_FLOAT_EQ(0.0f, RampT(Range(-0.0f, 0.0f), 0.0f));
}

TEST(DiaValueSchemaRampTEdge, EndpointsAreExact)
{
	const ResolvedRamp r = Range(-7.5f, 12.25f);
	EXPECT_EQ(0.0f, RampT(r, -7.5f));
	EXPECT_EQ(1.0f, RampT(r, 12.25f));
	EXPECT_EQ(0.0f, RampT(r, -7.51f));
	EXPECT_EQ(1.0f, RampT(r, 12.26f));
}

TEST(DiaValueSchemaRampTEdge, SubnormalRange)
{
	const float d = std::numeric_limits<float>::denorm_min();
	const ResolvedRamp r = Range(0.0f, 4.0f * d);
	EXPECT_FLOAT_EQ(0.0f, RampT(r, 0.0f));
	EXPECT_FLOAT_EQ(0.25f, RampT(r, d));
	EXPECT_FLOAT_EQ(0.5f, RampT(r, 2.0f * d));
	EXPECT_FLOAT_EQ(1.0f, RampT(r, 4.0f * d));
}

TEST(DiaValueSchemaRampTEdge, NegativeZeroValue)
{
	EXPECT_FLOAT_EQ(0.5f, RampT(Range(-1.0f, 1.0f), -0.0f));
}

TEST(DiaValueSchemaRampTEdgeProperty, NeverNaNAndAlwaysInUnitInterval)
{
	std::mt19937 rng(1234);
	const float specials[] = { 0.0f, -0.0f, 1.0f, -1.0f, FLT_MAX, -FLT_MAX, FLT_MIN, -FLT_MIN, std::numeric_limits<float>::denorm_min(),
		kInf, -kInf, kNaN, 3.0e38f, -3.0e38f, 1e-30f, -1e-30f, 123456.0f };
	auto pick = [&]() -> float
	{
		if (rng() % 3 == 0) { return specials[rng() % (sizeof(specials) / sizeof(specials[0]))]; }
		const float mag = std::pow(10.0f, static_cast<float>(static_cast<int>(rng() % 77) - 38));
		return ((rng() & 1) ? mag : -mag) * (1.0f + static_cast<float>(rng() % 1000) / 1000.0f);
	};
	for (int i = 0; i < 20000; ++i)
	{
		const ResolvedRamp r = Range(pick(), pick());
		const float v = pick();
		const float t = RampT(r, v);
		ASSERT_FALSE(std::isnan(t)) << r.mMin << ".." << r.mMax << " v=" << v;
		ASSERT_GE(t, 0.0f) << r.mMin << ".." << r.mMax << " v=" << v;
		ASSERT_LE(t, 1.0f) << r.mMin << ".." << r.mMax << " v=" << v;
	}
}

// ---------------------------------------------------------------------------------------------------------
// ResolveRamp: ui.ramp parsing
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampResolverEdge, NonStringUiRampIsTreatedAsAbsent)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const Json::Value values[] = { Json::Value(5), Json::Value(true), Json::Value(1.5), Json::Value(Json::nullValue),
		Json::Value(Json::arrayValue), Json::Value(Json::objectValue) };
	for (const Json::Value& v : values)
	{
		ValueInfo info = FloatEntry("a");
		info.mUi["ramp"] = v;
		const ResolvedRamp r = res.ResolveRamp(info);
		EXPECT_FALSE(r.mFellBack) << v.toStyledString();
		EXPECT_STREQ("sequential.grey", r.mRamp.mName.AsCStr());
	}
	EXPECT_EQ(0u, res.UnknownRampWarningCount());
	EXPECT_EQ(0u, res.FallbackCount());
}

TEST(DiaValueSchemaRampResolverEdge, EmptyStringRampIsUnknownAndWarnsOnce)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const ValueInfo info = FloatEntry("a", "");
	for (int i = 0; i < 3; ++i)
	{
		const ResolvedRamp r = res.ResolveRamp(info);
		EXPECT_TRUE(r.mFellBack);
		EXPECT_STREQ("sequential.grey", r.mRamp.mName.AsCStr());
	}
	EXPECT_EQ(1u, res.UnknownRampWarningCount());
	EXPECT_EQ(3u, res.FallbackCount());
}

TEST(DiaValueSchemaRampResolverEdge, UiRampWithPercentAndFormatTokensIsLookedUpVerbatim)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const ValueInfo info = FloatEntry("a", "%s%d%n%%");
	const ResolvedRamp r = res.ResolveRamp(info);	// the warning text must not interpret the id
	EXPECT_TRUE(r.mFellBack);
	EXPECT_EQ(1u, res.UnknownRampWarningCount());
}

TEST(DiaValueSchemaRampResolverEdge, UiRampIsCaseAndWhitespaceSensitive)
{
	RampFixture f;
	RampResolver res(f.Svc());
	EXPECT_TRUE(res.ResolveRamp(FloatEntry("a", "Sequential.Green")).mFellBack);
	EXPECT_TRUE(res.ResolveRamp(FloatEntry("b", " sequential.green")).mFellBack);
	EXPECT_TRUE(res.ResolveRamp(FloatEntry("c", "sequential.green ")).mFellBack);
	EXPECT_FALSE(res.ResolveRamp(FloatEntry("d", "sequential.green")).mFellBack);
}

TEST(DiaValueSchemaRampResolverEdge, UiNotAnObjectDoesNotCrash)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const Json::Value uis[] = { Json::Value(Json::arrayValue), Json::Value("text"), Json::Value(7), Json::Value(true), Json::Value(Json::nullValue) };
	for (const Json::Value& ui : uis)
	{
		ValueInfo info = FloatEntry("a");
		info.mUi = ui;
		ResolvedRamp r;
		ResolvedRamp d;
		ResolvedPalette p;
		EXPECT_NO_THROW(r = res.ResolveRamp(info)) << ui.toStyledString();
		EXPECT_NO_THROW(d = res.ResolveDeltaRamp(info, 1.0f)) << ui.toStyledString();
		EXPECT_NO_THROW(p = res.ResolvePalette(info));
		EXPECT_STREQ("sequential.grey", r.mRamp.mName.AsCStr());
		EXPECT_STREQ("diverging.blue_red", d.mRamp.mName.AsCStr());
		EXPECT_FALSE(r.mFellBack);
		EXPECT_FALSE(d.mFellBack);
	}
}

// ---------------------------------------------------------------------------------------------------------
// ResolveRamp: ranges
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampResolverEdge, OnlyMinOrOnlyMaxMeansNoRange)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo onlyMin = FloatEntry("a");
	onlyMin.mHasMin = true; onlyMin.mMin = -5.0;
	ValueInfo onlyMax = FloatEntry("b");
	onlyMax.mHasMax = true; onlyMax.mMax = 5.0;
	for (const ValueInfo* info : { &onlyMin, &onlyMax })
	{
		const ResolvedRamp r = res.ResolveRamp(*info);
		EXPECT_FALSE(r.mHasRange);
		EXPECT_EQ(0.0f, r.mMin);
		EXPECT_EQ(1.0f, r.mMax);
	}
}

TEST(DiaValueSchemaRampResolverEdge, RangeBeyondFloatIsClampedAndNeverNaN)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = FloatEntry("a");
	info.mHasMin = true; info.mMin = -1e300;
	info.mHasMax = true; info.mMax = 1e300;
	const ResolvedRamp r = res.ResolveRamp(info);
	EXPECT_TRUE(r.mHasRange);
	EXPECT_EQ(-FLT_MAX, r.mMin);
	EXPECT_EQ(FLT_MAX, r.mMax);
	for (float v : { 0.0f, 1.0f, -1e30f, 1e30f, FLT_MAX, -FLT_MAX, kInf, -kInf })
	{
		const float t = RampT(r, v);
		EXPECT_FALSE(std::isnan(t)) << v;
		EXPECT_GE(t, 0.0f);
		EXPECT_LE(t, 1.0f);
	}
	EXPECT_FLOAT_EQ(0.5f, RampT(r, 0.0f));
}

TEST(DiaValueSchemaRampResolverEdge, InfiniteDoubleBoundsAreClampedToFloatMax)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = FloatEntry("a");
	info.mHasMin = true; info.mMin = -std::numeric_limits<double>::infinity();
	info.mHasMax = true; info.mMax = std::numeric_limits<double>::infinity();
	const ResolvedRamp r = res.ResolveRamp(info);
	EXPECT_EQ(-FLT_MAX, r.mMin);
	EXPECT_EQ(FLT_MAX, r.mMax);
	EXPECT_FLOAT_EQ(0.5f, RampT(r, 0.0f));
}

TEST(DiaValueSchemaRampResolverEdge, OnlyOneSideBeyondFloat)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = FloatEntry("a");
	info.mHasMin = true; info.mMin = 0.0;
	info.mHasMax = true; info.mMax = 1e300;
	const ResolvedRamp r = res.ResolveRamp(info);
	EXPECT_EQ(0.0f, r.mMin);
	EXPECT_EQ(FLT_MAX, r.mMax);
	EXPECT_FLOAT_EQ(0.5f, RampT(r, FLT_MAX / 2.0f));
	EXPECT_NEAR(0.0f, RampT(r, 1.0f), 1e-30f);	// 1 / 3.4e38 is tiny but not NaN
}

TEST(DiaValueSchemaRampResolverEdge, NaNBoundIsCarriedAndGivesZeroT)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = FloatEntry("a");
	info.mHasMin = true; info.mMin = std::numeric_limits<double>::quiet_NaN();
	info.mHasMax = true; info.mMax = 1.0;
	const ResolvedRamp r = res.ResolveRamp(info);
	EXPECT_TRUE(r.mHasRange);
	EXPECT_TRUE(std::isnan(r.mMin));
	EXPECT_FLOAT_EQ(0.0f, RampT(r, 0.5f));
}

TEST(DiaValueSchemaRampResolverEdge, TinyDoubleBoundsCollapseToZeroWidth)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = FloatEntry("a");
	info.mHasMin = true; info.mMin = 1e-300;
	info.mHasMax = true; info.mMax = 2e-300;
	const ResolvedRamp r = res.ResolveRamp(info);
	EXPECT_TRUE(r.mHasRange);
	EXPECT_EQ(0.0f, RampT(r, 1.5e-300f));
}

TEST(DiaValueSchemaRampResolverEdge, InvertedRangeIsCarriedAsGiven)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = FloatEntry("a");
	info.mHasMin = true; info.mMin = 10.0;
	info.mHasMax = true; info.mMax = 0.0;
	const ResolvedRamp r = res.ResolveRamp(info);
	EXPECT_TRUE(r.mHasRange);
	EXPECT_EQ(10.0f, r.mMin);
	EXPECT_EQ(0.0f, r.mMax);
	EXPECT_EQ(0.0f, RampT(r, 5.0f));
}

TEST(DiaValueSchemaRampResolverEdge, RangeRoundsDoublesToNearestFloat)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = FloatEntry("a");
	info.mHasMin = true; info.mMin = 0.1;
	info.mHasMax = true; info.mMax = 0.3;
	const ResolvedRamp r = res.ResolveRamp(info);
	EXPECT_EQ(0.1f, r.mMin);
	EXPECT_EQ(0.3f, r.mMax);
}

// ---------------------------------------------------------------------------------------------------------
// Fallback counting
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampResolverEdge, FallbackCountIsPerCallNotPerKey)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const ValueInfo a = FloatEntry("a", "nope");
	const ValueInfo b = FloatEntry("b", "nope");
	for (int i = 0; i < 3; ++i) { res.ResolveRamp(a); res.ResolveRamp(b); }
	EXPECT_EQ(6u, res.FallbackCount());
	EXPECT_EQ(2u, res.UnknownRampWarningCount());
	res.ResolveRamp(FloatEntry("ok", "sequential.blue"));
	res.ResolveRamp(FloatEntry("grey"));
	EXPECT_EQ(6u, res.FallbackCount());
}

TEST(DiaValueSchemaRampResolverEdge, BoundCounterMirrorsFallbackCount)
{
	RampFixture f;
	RampResolver res(f.Svc());
	Dia::Observation::Metric::Counter counter;
	res.BindFallbackCounter(&counter);
	const ValueInfo bad = FloatEntry("a", "nope");
	for (int i = 0; i < 4; ++i) { res.ResolveRamp(bad); }
	res.ResolveRamp(FloatEntry("ok"));
	EXPECT_EQ(4u, res.FallbackCount());
	EXPECT_EQ(4u, counter.Value());
	res.ResolveDeltaRamp(FloatEntry("d"), 1.0f);	// honoured: no count
	EXPECT_EQ(4u, counter.Value());
}

TEST(DiaValueSchemaRampResolverEdge, BindingNullCounterIsSafeAndStopsMirroring)
{
	RampFixture f;
	RampResolver res(f.Svc());
	res.BindFallbackCounter(nullptr);
	EXPECT_NO_THROW(res.ResolveRamp(FloatEntry("a", "nope")));
	EXPECT_EQ(1u, res.FallbackCount());

	Dia::Observation::Metric::Counter counter;
	res.BindFallbackCounter(&counter);
	res.ResolveRamp(FloatEntry("a", "nope"));
	EXPECT_EQ(1u, counter.Value());
	res.BindFallbackCounter(nullptr);
	res.ResolveRamp(FloatEntry("a", "nope"));
	EXPECT_EQ(1u, counter.Value());
	EXPECT_EQ(3u, res.FallbackCount());
}

TEST(DiaValueSchemaRampResolverEdge, MissingGreyCountsFallbackButDoesNotWarn)
{
	RampFixture f(true, true, "sequential.grey");
	RampResolver res(f.Svc());
	for (int i = 0; i < 3; ++i)
	{
		const ResolvedRamp r = res.ResolveRamp(FloatEntry("a"));
		EXPECT_TRUE(r.mFellBack);
		EXPECT_STREQ("builtin.fallback", r.mRamp.mName.AsCStr());
	}
	EXPECT_EQ(3u, res.FallbackCount());
	EXPECT_EQ(0u, res.UnknownRampWarningCount());
	// A ramp that exists is still honoured.
	EXPECT_FALSE(res.ResolveRamp(FloatEntry("b", "sequential.blue")).mFellBack);
}

TEST(DiaValueSchemaRampResolverEdge, UnknownRampWithMissingGreyCountsOneFallbackPerCall)
{
	RampFixture f(true, true, "sequential.grey");
	RampResolver res(f.Svc());
	const ValueInfo info = FloatEntry("a", "nope");
	res.ResolveRamp(info);
	res.ResolveRamp(info);
	EXPECT_EQ(2u, res.FallbackCount());
	EXPECT_EQ(1u, res.UnknownRampWarningCount());
}

TEST(DiaValueSchemaRampResolverEdge, NoRampsAtAll)
{
	RampFixture f(false);
	RampResolver res(f.Svc());
	const ResolvedRamp r = res.ResolveRamp(FloatEntry("a", "sequential.green"));
	EXPECT_TRUE(r.mFellBack);
	EXPECT_STREQ("builtin.fallback", r.mRamp.mName.AsCStr());
	EXPECT_EQ(1u, res.UnknownRampWarningCount());
	EXPECT_EQ(1u, res.FallbackCount());
}

TEST(DiaValueSchemaRampResolverEdge, BuiltInFallbackRunsBlackToWhiteWhileShippedGreyRunsWhiteToBlack)
{
	RampFixture shipped;
	RampFixture none(false);
	RampResolver rs(shipped.Svc());
	RampResolver rn(none.Svc());
	const ResolvedRamp grey = rs.ResolveRamp(FloatEntry("a"));
	const ResolvedRamp builtin = rn.ResolveRamp(FloatEntry("a"));
	EXPECT_EQ(0xFFFFFFFFu, SampleRamp(grey.mRamp, 0.0f));
	EXPECT_EQ(0x000000FFu, SampleRamp(grey.mRamp, 1.0f));
	EXPECT_EQ(0x000000FFu, SampleRamp(builtin.mRamp, 0.0f));
	EXPECT_EQ(0xFFFFFFFFu, SampleRamp(builtin.mRamp, 1.0f));
	EXPECT_EQ(2u, builtin.mRamp.mStopCount);
}

TEST(DiaValueSchemaRampResolverEdge, ContinuousRampRegisteredUnderCategoricalKindMismatch)
{
	// A categorical ramp registered as the default grey id is unusable for a float entry: built-in fallback.
	RampFixture f(false, false);
	RampData cat = MakeCategorical("sequential.grey", 3);
	f.mRamps.Register(cat, true, "t");
	f.Finish();
	RampResolver res(f.Svc());
	const ResolvedRamp r = res.ResolveRamp(FloatEntry("a"));
	EXPECT_TRUE(r.mFellBack);
	EXPECT_STREQ("builtin.fallback", r.mRamp.mName.AsCStr());
	EXPECT_EQ(0u, res.UnknownRampWarningCount());
}

// ---------------------------------------------------------------------------------------------------------
// Warn-key spaces
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampResolverEdge, WarnKeySpacesArePerKind)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = FloatEntry("a", "nope");
	info.mUi["delta_ramp"] = "nope2";
	res.ResolveRamp(info);
	res.ResolveRamp(info);
	res.ResolveDeltaRamp(info, 1.0f);
	res.ResolveDeltaRamp(info, 1.0f);
	EXPECT_EQ(2u, res.UnknownRampWarningCount());	// one for ramp, one for delta_ramp
	EXPECT_EQ(4u, res.FallbackCount());
}

TEST(DiaValueSchemaRampResolverEdge, PaletteWrapWarningIsIndependentOfRampWarning)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo e = EnumEntry("e", 13);
	e.mUi["ramp"] = "nope";
	res.ResolveRamp(e);
	res.ResolvePalette(e);
	res.ResolveRamp(e);
	res.ResolvePalette(e);
	EXPECT_EQ(1u, res.UnknownRampWarningCount());
	EXPECT_EQ(1u, res.WrapWarningCount());
}

TEST(DiaValueSchemaRampResolverEdge, EntriesWithEmptyFullKeyShareWarnState)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo a = Dia::ValueSchema::Testing::MakeValueInfo("t", "a");
	ValueInfo b = Dia::ValueSchema::Testing::MakeValueInfo("t", "b");
	a.mUi["ramp"] = "nope";
	b.mUi["ramp"] = "nope";
	res.ResolveRamp(a);
	res.ResolveRamp(b);
	EXPECT_EQ(1u, res.UnknownRampWarningCount());	// mFullKey is filled by the registry; unregistered entries collapse
	EXPECT_EQ(2u, res.FallbackCount());
}

TEST(DiaValueSchemaRampResolverEdge, TwoResolversKeepSeparateCountersAndWarnings)
{
	RampFixture f;
	RampResolver r1(f.Svc());
	RampResolver r2(f.Svc());
	const ValueInfo bad = FloatEntry("a", "nope");
	const ValueInfo wrap = EnumEntry("e", 13);
	r1.ResolveRamp(bad); r1.ResolveRamp(bad); r1.ResolvePalette(wrap);
	EXPECT_EQ(2u, r1.FallbackCount());
	EXPECT_EQ(1u, r1.UnknownRampWarningCount());
	EXPECT_EQ(1u, r1.WrapWarningCount());
	EXPECT_EQ(0u, r2.FallbackCount());
	EXPECT_EQ(0u, r2.UnknownRampWarningCount());
	EXPECT_EQ(0u, r2.WrapWarningCount());
	r2.ResolvePalette(wrap);
	EXPECT_EQ(1u, r2.WrapWarningCount());
}

// ---------------------------------------------------------------------------------------------------------
// ResolveDeltaRamp
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampResolverEdge, DeltaMaxAbsInfinityUsesOne)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ResolvedRamp r = res.ResolveDeltaRamp(FloatEntry("a"), kInf);
	EXPECT_EQ(-1.0f, r.mMin);
	EXPECT_EQ(1.0f, r.mMax);
	EXPECT_FLOAT_EQ(0.5f, RampT(r, 0.0f));
	r = res.ResolveDeltaRamp(FloatEntry("a"), -kInf);
	EXPECT_EQ(-1.0f, r.mMin);
	EXPECT_EQ(1.0f, r.mMax);
}

TEST(DiaValueSchemaRampResolverEdge, DeltaMaxAbsHugeKeepsCentreAtHalf)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const ResolvedRamp r = res.ResolveDeltaRamp(FloatEntry("a"), FLT_MAX);
	EXPECT_EQ(-FLT_MAX, r.mMin);
	EXPECT_EQ(FLT_MAX, r.mMax);
	EXPECT_FLOAT_EQ(0.5f, RampT(r, 0.0f));
	EXPECT_FLOAT_EQ(1.0f, RampT(r, FLT_MAX));
	EXPECT_FLOAT_EQ(0.0f, RampT(r, -FLT_MAX));
}

TEST(DiaValueSchemaRampResolverEdge, DeltaMaxAbsZeroNegativeNaNUseOne)
{
	RampFixture f;
	RampResolver res(f.Svc());
	for (float m : { 0.0f, -0.0f, -1.0f, -1e30f, kNaN, -kNaN })
	{
		const ResolvedRamp r = res.ResolveDeltaRamp(FloatEntry("a"), m);
		EXPECT_EQ(-1.0f, r.mMin) << m;
		EXPECT_EQ(1.0f, r.mMax) << m;
		EXPECT_TRUE(r.mHasRange);
	}
}

TEST(DiaValueSchemaRampResolverEdge, DeltaMaxAbsSubnormalIsHonoured)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const float d = std::numeric_limits<float>::denorm_min();
	const ResolvedRamp r = res.ResolveDeltaRamp(FloatEntry("a"), d);
	EXPECT_EQ(-d, r.mMin);
	EXPECT_EQ(d, r.mMax);
	EXPECT_FLOAT_EQ(0.5f, RampT(r, 0.0f));
	EXPECT_FLOAT_EQ(1.0f, RampT(r, d));
	EXPECT_FLOAT_EQ(0.0f, RampT(r, -d));
}

TEST(DiaValueSchemaRampResolverEdge, DeltaRampIgnoresUiRampAndUsesDeltaKey)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = FloatEntry("a", "sequential.blue");
	EXPECT_STREQ("diverging.blue_red", res.ResolveDeltaRamp(info, 1.0f).mRamp.mName.AsCStr());
	info.mUi["delta_ramp"] = "sequential.heat";
	EXPECT_STREQ("sequential.heat", res.ResolveDeltaRamp(info, 1.0f).mRamp.mName.AsCStr());
	EXPECT_STREQ("sequential.blue", res.ResolveRamp(info).mRamp.mName.AsCStr());
}

TEST(DiaValueSchemaRampResolverEdge, DeltaRampCategoricalIdFallsBackToDefault)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = FloatEntry("a");
	info.mUi["delta_ramp"] = "categorical.12";
	const ResolvedRamp r = res.ResolveDeltaRamp(info, 1.0f);
	EXPECT_TRUE(r.mFellBack);
	EXPECT_STREQ("diverging.blue_red", r.mRamp.mName.AsCStr());
	EXPECT_EQ(1u, res.UnknownRampWarningCount());
}

TEST(DiaValueSchemaRampResolverEdge, DeltaDefaultMissingUsesBuiltIn)
{
	RampFixture f(true, true, "diverging.blue_red");
	RampResolver res(f.Svc());
	const ResolvedRamp r = res.ResolveDeltaRamp(FloatEntry("a"), 2.0f);
	EXPECT_TRUE(r.mFellBack);
	EXPECT_STREQ("builtin.fallback", r.mRamp.mName.AsCStr());
	EXPECT_EQ(-2.0f, r.mMin);
	EXPECT_EQ(0u, res.UnknownRampWarningCount());
	EXPECT_EQ(1u, res.FallbackCount());
}

TEST(DiaValueSchemaRampResolverEdge, DeltaRampWithEntryRangeStillSymmetricFromMaxAbs)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = FloatEntry("a");
	info.mHasMin = true; info.mMin = 0.0;
	info.mHasMax = true; info.mMax = 100.0;
	const ResolvedRamp r = res.ResolveDeltaRamp(info, 3.0f);
	EXPECT_EQ(-3.0f, r.mMin);
	EXPECT_EQ(3.0f, r.mMax);
}

// ---------------------------------------------------------------------------------------------------------
// ResolvePalette
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampResolverEdge, PaletteZeroValues)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const ResolvedPalette p = res.ResolvePalette(EnumEntry("e", 0));
	EXPECT_EQ(0u, p.mEntries.Size());
	EXPECT_FALSE(p.mWrapped);
	EXPECT_EQ(0u, res.WrapWarningCount());
}

TEST(DiaValueSchemaRampResolverEdge, PaletteInvalidGivenColourIsColourless)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const RampData* cat = f.Svc().FindRamp(Dia::Core::StringCRC("categorical.12"));
	ASSERT_NE(cat, nullptr);
	ValueInfo info = EnumEntry("e", 3);
	for (unsigned i = 0; i < 3; ++i) { info.mEnumValues[i].mHasColour = true; }
	SetText(info.mEnumValues[0].mColour, "red");
	SetText(info.mEnumValues[1].mColour, "#12345");
	SetText(info.mEnumValues[2].mColour, "#12345g");
	const ResolvedPalette p = res.ResolvePalette(info);
	ASSERT_EQ(3u, p.mEntries.Size());
	for (unsigned i = 0; i < 3; ++i) { EXPECT_EQ(cat->mColours[i], p.mEntries[i].mRgba8) << i; }
}

TEST(DiaValueSchemaRampResolverEdge, PaletteInvalidGivenColoursCountTowardWrap)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = EnumEntry("e", 13);
	for (unsigned i = 0; i < 13; ++i) { info.mEnumValues[i].mHasColour = true; SetText(info.mEnumValues[i].mColour, "red"); }
	const ResolvedPalette p = res.ResolvePalette(info);
	EXPECT_TRUE(p.mWrapped);
	EXPECT_EQ(1u, res.WrapWarningCount());
}

TEST(DiaValueSchemaRampResolverEdge, PaletteHasColourFlagWithoutFlagIgnoresColourText)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const RampData* cat = f.Svc().FindRamp(Dia::Core::StringCRC("categorical.12"));
	ValueInfo info = EnumEntry("e", 1);
	SetText(info.mEnumValues[0].mColour, "#102030");	// text present, mHasColour false
	const ResolvedPalette p = res.ResolvePalette(info);
	EXPECT_EQ(cat->mColours[0], p.mEntries[0].mRgba8);
}

TEST(DiaValueSchemaRampResolverEdge, PaletteGivenColourKeepsAlphaOpaque)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = EnumEntry("e", 1, 1);
	SetText(info.mEnumValues[0].mColour, "#aBcDeF");
	const ResolvedPalette p = res.ResolvePalette(info);
	EXPECT_EQ(0xABCDEFFFu, p.mEntries[0].mRgba8);
}

TEST(DiaValueSchemaRampResolverEdge, PaletteEmptyIdIsKept)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = EnumEntry("e", 2);
	SetText(info.mEnumValues[1].mId, "");
	const ResolvedPalette p = res.ResolvePalette(info);
	ASSERT_EQ(2u, p.mEntries.Size());
	EXPECT_STREQ("v0", p.mEntries[0].mId.AsCStr());
	EXPECT_STREQ("", p.mEntries[1].mId.AsCStr());
}

TEST(DiaValueSchemaRampResolverEdge, PaletteIdsKeepFormatTokensVerbatim)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ValueInfo info = EnumEntry("e", 2);
	SetText(info.mEnumValues[0].mId, "100%");
	SetText(info.mEnumValues[1].mId, "%s%d%n");
	const ResolvedPalette p = res.ResolvePalette(info);
	EXPECT_STREQ("100%", p.mEntries[0].mId.AsCStr());
	EXPECT_STREQ("%s%d%n", p.mEntries[1].mId.AsCStr());
}

TEST(DiaValueSchemaRampResolverEdge, PaletteWrapThresholdWithShippedCategorical)
{
	RampFixture f;
	const RampData* cat = f.Svc().FindRamp(Dia::Core::StringCRC("categorical.12"));
	ASSERT_NE(cat, nullptr);
	struct Case { unsigned mCount; bool mWraps; };
	for (const Case c : { Case{ 12, false }, Case{ 13, true }, Case{ 32, true }, Case{ 33, true } })
	{
		RampResolver res(f.Svc());
		const ResolvedPalette p = res.ResolvePalette(EnumEntry("e", c.mCount));
		ASSERT_EQ(c.mCount, p.mEntries.Size());
		EXPECT_EQ(c.mWraps, p.mWrapped) << c.mCount;
		EXPECT_EQ(c.mWraps ? 1u : 0u, res.WrapWarningCount()) << c.mCount;
		for (unsigned i = 0; i < c.mCount; ++i) { EXPECT_EQ(cat->mColours[i % 12], p.mEntries[i].mRgba8) << c.mCount << "/" << i; }
	}
}

TEST(DiaValueSchemaRampResolverEdge, PaletteWrapThresholdWithoutCategoricalLoaded)
{
	RampFixture f(false);
	struct Case { unsigned mCount; bool mWraps; };
	for (const Case c : { Case{ 12, false }, Case{ 13, true }, Case{ 32, true }, Case{ 33, true } })
	{
		RampResolver res(f.Svc());
		const ResolvedPalette p = res.ResolvePalette(EnumEntry("e", c.mCount));
		ASSERT_EQ(c.mCount, p.mEntries.Size());
		EXPECT_EQ(c.mWraps, p.mWrapped) << c.mCount;
		EXPECT_EQ(c.mWraps ? 1u : 0u, res.WrapWarningCount()) << c.mCount;
		for (unsigned i = 0; i < c.mCount; ++i)
		{
			EXPECT_EQ(ParseOrDie(GetCategoricalColour(i)), p.mEntries[i].mRgba8) << c.mCount << "/" << i;
		}
	}
}

TEST(DiaValueSchemaRampResolverEdge, PaletteGivenColoursDoNotCountTowardWrap)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const RampData* cat = f.Svc().FindRamp(Dia::Core::StringCRC("categorical.12"));
	const ResolvedPalette p = res.ResolvePalette(EnumEntry("e", 20, 0xFFu));	// first 8 given, 12 colourless
	EXPECT_FALSE(p.mWrapped);
	EXPECT_EQ(0u, res.WrapWarningCount());
	EXPECT_EQ(0x102030FFu, p.mEntries[7].mRgba8);
	EXPECT_EQ(cat->mColours[8], p.mEntries[8].mRgba8);
	EXPECT_EQ(cat->mColours[19 % 12], p.mEntries[19].mRgba8);
	const ResolvedPalette p13 = res.ResolvePalette(EnumEntry("f", 21, 0xFFu));	// 13 colourless
	EXPECT_TRUE(p13.mWrapped);
}

TEST(DiaValueSchemaRampResolverEdge, PaletteWrapWarnsOncePerEntryPerResolver)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const ValueInfo a = EnumEntry("a", 14);
	const ValueInfo b = EnumEntry("b", 14);
	for (int i = 0; i < 5; ++i) { res.ResolvePalette(a); }
	EXPECT_EQ(1u, res.WrapWarningCount());
	for (int i = 0; i < 5; ++i) { res.ResolvePalette(b); res.ResolvePalette(a); }
	EXPECT_EQ(2u, res.WrapWarningCount());
	RampResolver other(f.Svc());
	other.ResolvePalette(a);
	EXPECT_EQ(1u, other.WrapWarningCount());
	EXPECT_EQ(2u, res.WrapWarningCount());
}

TEST(DiaValueSchemaRampResolverEdge, PaletteUsesLoadedCategoricalColourCount)
{
	RampFixture f(false, false);
	f.mRamps.Register(MakeCategorical("categorical.12", 3), true, "t");
	f.Finish();
	RampResolver res(f.Svc());
	const ResolvedPalette p = res.ResolvePalette(EnumEntry("e", 4));
	EXPECT_TRUE(p.mWrapped);						// wrap threshold is the LOADED colour count, not 12
	EXPECT_EQ(0x10000000u * 1 | 0xFFu, p.mEntries[3].mRgba8);	// index 3 % 3 = 0
	EXPECT_EQ(1u, res.WrapWarningCount());
}

TEST(DiaValueSchemaRampResolverEdge, PaletteContinuousRampNamedCategoricalFallsBackToBuiltInList)
{
	RampFixture f(false, false);
	RampData cont;
	SetText(cont.mName, "categorical.12");
	cont.mId = Dia::Core::StringCRC("categorical.12");
	cont.mKind = RampKind::Continuous;
	cont.mStops[0].mT = 0.0f; cont.mStops[0].mRgba8 = 0x000000FFu;
	cont.mStops[1].mT = 1.0f; cont.mStops[1].mRgba8 = 0xFFFFFFFFu;
	cont.mStopCount = 2;
	f.mRamps.Register(cont, true, "t");
	f.Finish();
	RampResolver res(f.Svc());
	const ResolvedPalette p = res.ResolvePalette(EnumEntry("e", 2));
	EXPECT_EQ(ParseOrDie(GetCategoricalColour(0)), p.mEntries[0].mRgba8);
	EXPECT_EQ(ParseOrDie(GetCategoricalColour(1)), p.mEntries[1].mRgba8);
}

TEST(DiaValueSchemaRampResolverEdge, PaletteCategoricalWithZeroColoursUsesBuiltInListAndNeverWraps)
{
	RampFixture f(false, false);
	f.mRamps.Register(MakeCategorical("categorical.12", 0), true, "t");
	f.Finish();
	RampResolver res(f.Svc());
	const ResolvedPalette p = res.ResolvePalette(EnumEntry("e", 20));
	ASSERT_EQ(20u, p.mEntries.Size());
	EXPECT_FALSE(p.mWrapped);	// PIN: threshold is the loaded colour count (0), so the wrap check is skipped
	EXPECT_EQ(ParseOrDie(GetCategoricalColour(5)), p.mEntries[5].mRgba8);
}

TEST(DiaValueSchemaRampResolverEdge, PaletteForEveryNonEnumTypeIsEmpty)
{
	RampFixture f;
	RampResolver res(f.Svc());
	for (ValueType t : { ValueType::Float, ValueType::Int, ValueType::Bool, ValueType::String, ValueType::Ref, ValueType::RefList, ValueType::Map })
	{
		ValueInfo info = EnumEntry("e", 5);		// has enum values but is not an enum
		info.mType = t;
		const ResolvedPalette p = res.ResolvePalette(info);
		EXPECT_EQ(0u, p.mEntries.Size());
		EXPECT_FALSE(p.mWrapped);
	}
}

TEST(DiaValueSchemaRampResolverEdge, PaletteThousandValues)
{
	RampFixture f;
	RampResolver res(f.Svc());
	const ResolvedPalette p = res.ResolvePalette(EnumEntry("e", 1000));
	ASSERT_EQ(1000u, p.mEntries.Size());
	EXPECT_TRUE(p.mWrapped);
	EXPECT_STREQ("v999", p.mEntries[999].mId.AsCStr());
	EXPECT_EQ(p.mEntries[3].mRgba8, p.mEntries[963].mRgba8);
}

// ---------------------------------------------------------------------------------------------------------
// Copy semantics and threads
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaRampResolverEdge, ResolvedRampCopyIsIndependentOfRegistry)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ResolvedRamp a = res.ResolveRamp(FloatEntry("a", "sequential.blue"));
	ResolvedRamp b = a;
	b.mRamp.mStops[0].mRgba8 = 0x12345678u;
	b.mMin = 9.0f;
	EXPECT_NE(0x12345678u, a.mRamp.mStops[0].mRgba8);
	EXPECT_EQ(0.0f, a.mMin);
	EXPECT_EQ(0xF7FBFFFFu, f.Svc().FindRamp(Dia::Core::StringCRC("sequential.blue"))->mStops[0].mRgba8);
	ResolvedRamp c;
	c = b;
	EXPECT_EQ(0x12345678u, c.mRamp.mStops[0].mRgba8);
	EXPECT_EQ(9.0f, c.mMin);
}

TEST(DiaValueSchemaRampResolverEdge, ResolvedPaletteCopyAndAssign)
{
	RampFixture f;
	RampResolver res(f.Svc());
	ResolvedPalette a = res.ResolvePalette(EnumEntry("a", 5));
	ResolvedPalette b = a;
	ASSERT_EQ(5u, b.mEntries.Size());
	for (unsigned i = 0; i < 5; ++i)
	{
		EXPECT_EQ(a.mEntries[i].mRgba8, b.mEntries[i].mRgba8);
		EXPECT_STREQ(a.mEntries[i].mId.AsCStr(), b.mEntries[i].mId.AsCStr());
	}
	b.mEntries[0].mRgba8 = 1;
	EXPECT_NE(1u, a.mEntries[0].mRgba8);
	ResolvedPalette empty = res.ResolvePalette(FloatEntry("x"));
	ResolvedPalette copyOfEmpty = empty;
	EXPECT_EQ(0u, copyOfEmpty.mEntries.Size());
	b = empty;
	EXPECT_EQ(0u, b.mEntries.Size());
	b = a;
	EXPECT_EQ(5u, b.mEntries.Size());
	ResolvedPalette wrapped = res.ResolvePalette(EnumEntry("w", 13));
	ResolvedPalette wrappedCopy = wrapped;
	EXPECT_TRUE(wrappedCopy.mWrapped);
	EXPECT_EQ(13u, wrappedCopy.mEntries.Size());
}

TEST(DiaValueSchemaRampResolverEdge, ResolversOnTwoThreadsShareOneConstServiceWithoutRacing)
{
	RampFixture f;
	const ValueSchemaService& svc = f.Svc();
	const ValueInfo floatInfo = FloatEntry("f", "sequential.heat");
	const ValueInfo enumInfo = EnumEntry("e", 12);		// no wrap => no log output from the threads
	const uint32_t expectedHeatEnd = 0x800026FFu;

	std::atomic<int> failures{ 0 };
	auto work = [&]()
	{
		RampResolver res(svc);
		for (int i = 0; i < 1000; ++i)
		{
			const ResolvedRamp r = res.ResolveRamp(floatInfo);
			if (r.mFellBack || SampleRamp(r.mRamp, 1.0f) != expectedHeatEnd) { ++failures; }
			const ResolvedPalette p = res.ResolvePalette(enumInfo);
			if (p.mEntries.Size() != 12 || p.mWrapped) { ++failures; }
			const ResolvedRamp d = res.ResolveDeltaRamp(floatInfo, 2.0f);
			if (d.mFellBack) { ++failures; }
		}
		if (res.FallbackCount() != 0 || res.UnknownRampWarningCount() != 0 || res.WrapWarningCount() != 0) { ++failures; }
	};
	std::thread t1(work), t2(work);
	t1.join();
	t2.join();
	EXPECT_EQ(0, failures.load());
}
