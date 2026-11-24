#include <gtest/gtest.h>
#include <DiaValueSchema/ValueClamper.h>
#include <DiaObservation/Metric/Counter.h>
#include <cfloat>
#include <climits>
#include <cmath>
#include <limits>
#include <random>
#include <cstring>
#include <string>
#include <vector>

// ValueClamper edge cases and properties.
//
// PINs: min > max is not validated (a value is clamped to min then to max, so the result is max); entries with an
// empty mFullKey all share one warning slot; NaN and a clamp on the same key share the once-per-key warning;
// Clamp(int) with a fractional bound rounds to nearest and so can step outside the bound.

using namespace Dia::ValueSchema;

namespace
{
	const double kInf = std::numeric_limits<double>::infinity();
	const double kNan = std::numeric_limits<double>::quiet_NaN();

	ValueInfo Make(const std::string& key, ValueType type, bool hasMin, double mn, bool hasMax, double mx)
	{
		ValueInfo i;
		i.mFullKey = key.c_str();
		i.mType = type;
		i.mHasMin = hasMin; i.mMin = mn;
		i.mHasMax = hasMax; i.mMax = mx;
		return i;
	}

	ValueInfo Range(double mn, double mx, ValueType type = ValueType::Float) { return Make("g/r", type, true, mn, true, mx); }
}

TEST(DiaValueSchemaClamperEdge, InfinityWithBoundsClampsToBounds)
{
	ValueClamper c;
	const ValueInfo both = Range(-1.0, 1.0);
	EXPECT_EQ(c.Clamp(kInf, both), 1.0);
	EXPECT_EQ(c.Clamp(-kInf, both), -1.0);
	EXPECT_EQ(c.ClampCount(), 2u);
}

TEST(DiaValueSchemaClamperEdge, InfinityWithOneBoundOnlyClampsTheMatchingSide)
{
	ValueClamper c;
	const ValueInfo minOnly = Make("g/a", ValueType::Float, true, 0.0, false, 0.0);
	const ValueInfo maxOnly = Make("g/b", ValueType::Float, false, 0.0, true, 5.0);
	EXPECT_EQ(c.Clamp(kInf, minOnly), kInf);
	EXPECT_EQ(c.Clamp(-kInf, minOnly), 0.0);
	EXPECT_EQ(c.Clamp(-kInf, maxOnly), -kInf);
	EXPECT_EQ(c.Clamp(kInf, maxOnly), 5.0);
	EXPECT_EQ(c.ClampCount(), 2u);
}

TEST(DiaValueSchemaClamperEdge, InfinityWithoutRangePassesThrough)
{
	ValueClamper c;
	const ValueInfo none = Make("g/n", ValueType::Float, false, 0.0, false, 0.0);
	EXPECT_EQ(c.Clamp(kInf, none), kInf);
	EXPECT_EQ(c.Clamp(-kInf, none), -kInf);
	EXPECT_EQ(c.ClampCount(), 0u);
	EXPECT_EQ(c.WarningCount(), 0u);
}

TEST(DiaValueSchemaClamperEdge, SubnormalValues)
{
	ValueClamper c;
	const double sub = std::numeric_limits<double>::denorm_min();
	EXPECT_EQ(c.Clamp(sub, Range(0.0, 1.0)), sub);
	EXPECT_EQ(c.ClampCount(), 0u);
	EXPECT_EQ(c.Clamp(sub, Range(1e-300, 1.0)), 1e-300);
	EXPECT_EQ(c.ClampCount(), 1u);
	EXPECT_EQ(c.Clamp(-sub, Range(0.0, 1.0)), 0.0);
	EXPECT_EQ(c.Clamp(1e-310, Range(-1e-310, 1e-310)), 1e-310);
}

TEST(DiaValueSchemaClamperEdge, NegativeZeroIsInRangeAtZeroMinAndKeepsItsSign)
{
	ValueClamper c;
	const double r = c.Clamp(-0.0, Range(0.0, 1.0));
	EXPECT_EQ(r, 0.0);
	EXPECT_TRUE(std::signbit(r));
	EXPECT_EQ(c.ClampCount(), 0u);
	EXPECT_EQ(c.WarningCount(), 0u);
}

TEST(DiaValueSchemaClamperEdge, ValueExactlyAtBoundsIsNotCounted)
{
	ValueClamper c;
	const ValueInfo r = Range(-2.5, 7.5);
	EXPECT_EQ(c.Clamp(-2.5, r), -2.5);
	EXPECT_EQ(c.Clamp(7.5, r), 7.5);
	EXPECT_EQ(c.ClampCount(), 0u);
	EXPECT_EQ(c.WarningCount(), 0u);
	EXPECT_EQ(c.Clamp(std::nextafter(7.5, kInf), r), 7.5);
	EXPECT_EQ(c.Clamp(std::nextafter(-2.5, -kInf), r), -2.5);
	EXPECT_EQ(c.ClampCount(), 2u);
}

TEST(DiaValueSchemaClamperEdge, MinEqualsMax)
{
	ValueClamper c;
	const ValueInfo r = Range(3.0, 3.0);
	EXPECT_EQ(c.Clamp(3.0, r), 3.0);
	EXPECT_EQ(c.ClampCount(), 0u);
	EXPECT_EQ(c.Clamp(2.0, r), 3.0);
	EXPECT_EQ(c.Clamp(4.0, r), 3.0);
	EXPECT_EQ(c.Clamp(kInf, r), 3.0);
	EXPECT_EQ(c.ClampCount(), 3u);
}

TEST(DiaValueSchemaClamperEdge, PinMinGreaterThanMaxAlwaysYieldsMax)
{
	ValueClamper c;
	const ValueInfo r = Range(10.0, 1.0);
	EXPECT_EQ(c.Clamp(5.0, r), 1.0);
	EXPECT_EQ(c.Clamp(0.0, r), 1.0);
	EXPECT_EQ(c.Clamp(20.0, r), 1.0);
	EXPECT_EQ(c.Clamp(1.0, r), 1.0);	// equals max: the intermediate min step is invisible, so not counted
	EXPECT_EQ(c.ClampCount(), 3u);
}

// ---- Clamp(int) (suspected defect (d)) -----------------------------------------------------------------------------

TEST(DiaValueSchemaClamperEdge, IntExtremesWithRange)
{
	ValueClamper c;
	const ValueInfo r = Range(-5.0, 5.0, ValueType::Int);
	EXPECT_EQ(c.Clamp(INT_MIN, r), -5);
	EXPECT_EQ(c.Clamp(INT_MAX, r), 5);
	EXPECT_EQ(c.Clamp(-5, r), -5);
	EXPECT_EQ(c.Clamp(5, r), 5);
	EXPECT_EQ(c.Clamp(0, r), 0);
	EXPECT_EQ(c.ClampCount(), 2u);
}

TEST(DiaValueSchemaClamperEdge, IntExtremesWithoutRangeAndOneSidedRange)
{
	ValueClamper c;
	const ValueInfo none = Make("g/n", ValueType::Int, false, 0, false, 0);
	EXPECT_EQ(c.Clamp(INT_MIN, none), INT_MIN);
	EXPECT_EQ(c.Clamp(INT_MAX, none), INT_MAX);
	const ValueInfo minOnly = Make("g/m", ValueType::Int, true, 0.0, false, 0.0);
	EXPECT_EQ(c.Clamp(INT_MAX, minOnly), INT_MAX);
	EXPECT_EQ(c.Clamp(INT_MIN, minOnly), 0);
	const ValueInfo maxOnly = Make("g/x", ValueType::Int, false, 0.0, true, 0.0);
	EXPECT_EQ(c.Clamp(INT_MIN, maxOnly), INT_MIN);
	EXPECT_EQ(c.Clamp(INT_MAX, maxOnly), 0);
}

TEST(DiaValueSchemaClamperEdge, IntBoundOutsideIntRangeSaturates)
{
	ValueClamper c;
	EXPECT_EQ(c.Clamp(5, Make("g/a", ValueType::Int, true, 1e10, false, 0.0)), INT_MAX);
	EXPECT_EQ(c.Clamp(5, Make("g/b", ValueType::Int, false, 0.0, true, -1e10)), INT_MIN);
	EXPECT_EQ(c.Clamp(5, Make("g/c", ValueType::Int, true, 2147483648.0, false, 0.0)), INT_MAX);
	EXPECT_EQ(c.Clamp(5, Make("g/d", ValueType::Int, false, 0.0, true, -2147483649.0)), INT_MIN);
	EXPECT_EQ(c.Clamp(5, Make("g/e", ValueType::Int, true, kInf, false, 0.0)), INT_MAX);
	EXPECT_EQ(c.Clamp(5, Make("g/f", ValueType::Int, false, 0.0, true, -kInf)), INT_MIN);
	// Bounds beyond int on the harmless side leave the value alone.
	EXPECT_EQ(c.Clamp(5, Make("g/g", ValueType::Int, true, -1e10, true, 1e10)), 5);
	EXPECT_EQ(c.Clamp(INT_MAX, Make("g/h", ValueType::Int, true, -1e10, true, 1e10)), INT_MAX);
}

TEST(DiaValueSchemaClamperEdge, PinIntFractionalBoundsRoundToNearest)
{
	// PIN: the double result is converted with lround, so a fractional bound can land outside it (2.5 -> 3).
	ValueClamper c;
	EXPECT_EQ(c.Clamp(0, Make("g/a", ValueType::Int, true, 0.5, false, 0.0)), 1);
	EXPECT_EQ(c.Clamp(5, Make("g/b", ValueType::Int, false, 0.0, true, 2.4)), 2);
	EXPECT_EQ(c.Clamp(5, Make("g/c", ValueType::Int, false, 0.0, true, 2.5)), 3);
}

// ---- Clamp(float) (suspected defect (e)) ---------------------------------------------------------------------------

TEST(DiaValueSchemaClamperEdge, FloatResultNeverExceedsMaxInDouble)
{
	ValueClamper c;
	const ValueInfo r = Range(0.0, 0.1);
	const float out = c.Clamp(0.5f, r);
	EXPECT_LE(static_cast<double>(out), 0.1) << "0.1f is above 0.1 as a double";
	EXPECT_GE(static_cast<double>(out), 0.0);
	EXPECT_NEAR(out, 0.1f, 1e-6f);
	// Float input already above the double max by rounding counts as out of range.
	const float again = c.Clamp(0.1f, r);
	EXPECT_LE(static_cast<double>(again), 0.1);
}

TEST(DiaValueSchemaClamperEdge, FloatResultNeverBelowMinInDouble)
{
	ValueClamper c;
	const ValueInfo r = Range(0.7, 5.0);	// 0.7f < 0.7
	const float out = c.Clamp(-3.0f, r);
	EXPECT_GE(static_cast<double>(out), 0.7);
	EXPECT_NEAR(out, 0.7f, 1e-6f);
}

TEST(DiaValueSchemaClamperEdge, FloatInRangeIsBitwiseUnchanged)
{
	ValueClamper c;
	const ValueInfo r = Range(0.0, 1.0);
	for (float v : { 0.0f, 0.1f, 0.5f, 0.9999999f, 1.0f, FLT_MIN, 1e-40f })
	{
		const float out = c.Clamp(v, r);
		EXPECT_EQ(memcmp(&out, &v, sizeof(float)), 0) << v;
	}
	EXPECT_EQ(c.ClampCount(), 0u);
}

TEST(DiaValueSchemaClamperEdge, FloatInfinityAndHugeBounds)
{
	ValueClamper c;
	const ValueInfo huge = Range(-1e300, 1e300);
	const float hi = c.Clamp(std::numeric_limits<float>::infinity(), huge);
	const float lo = c.Clamp(-std::numeric_limits<float>::infinity(), huge);
	EXPECT_TRUE(std::isfinite(hi));
	EXPECT_TRUE(std::isfinite(lo));
	EXPECT_LE(static_cast<double>(hi), 1e300);
	EXPECT_GE(static_cast<double>(lo), -1e300);
	EXPECT_EQ(c.Clamp(FLT_MAX, huge), FLT_MAX);
	EXPECT_EQ(c.Clamp(std::numeric_limits<float>::infinity(), Make("g/n", ValueType::Float, false, 0, false, 0)), std::numeric_limits<float>::infinity());
}

TEST(DiaValueSchemaClamperEdge, FloatNanPassesThrough)
{
	ValueClamper c;
	EXPECT_TRUE(std::isnan(c.Clamp(std::numeric_limits<float>::quiet_NaN(), Range(0.0, 1.0))));
	EXPECT_EQ(c.ClampCount(), 0u);
	EXPECT_EQ(c.WarningCount(), 1u);
}

// ---- pass-through types --------------------------------------------------------------------------------------------

TEST(DiaValueSchemaClamperEdge, NonNumericTypesWithARangePassThrough)
{
	ValueClamper c;
	for (ValueType t : { ValueType::Bool, ValueType::Enum, ValueType::String, ValueType::Ref, ValueType::RefList, ValueType::Map })
	{
		SCOPED_TRACE(static_cast<int>(t));
		const ValueInfo r = Range(0.0, 1.0, t);
		EXPECT_EQ(c.Clamp(50.0, r), 50.0);
		EXPECT_EQ(c.Clamp(-50.0f, r), -50.0f);
		EXPECT_EQ(c.Clamp(50, r), 50);
		EXPECT_TRUE(std::isnan(c.Clamp(kNan, r)));
	}
	EXPECT_EQ(c.ClampCount(), 0u);
	EXPECT_EQ(c.WarningCount(), 0u);
	EXPECT_EQ(c.WarnedKeyCount(), 0u);
}

TEST(DiaValueSchemaClamperEdge, NanWithoutRangeDoesNotWarn)
{
	ValueClamper c;
	EXPECT_TRUE(std::isnan(c.Clamp(kNan, Make("g/n", ValueType::Float, false, 0, false, 0))));
	EXPECT_EQ(c.WarningCount(), 0u);
	// With only one bound it does warn.
	EXPECT_TRUE(std::isnan(c.Clamp(kNan, Make("g/m", ValueType::Float, true, 0.0, false, 0.0))));
	EXPECT_EQ(c.WarningCount(), 1u);
}

// ---- counters and warning bookkeeping ------------------------------------------------------------------------------

TEST(DiaValueSchemaClamperEdge, NanThenClampOnOneKeyWarnsOnceAndCountsOnlyTheClamp)
{
	ValueClamper c;
	const ValueInfo r = Range(0.0, 1.0);
	c.Clamp(kNan, r);
	c.Clamp(5.0, r);
	c.Clamp(kNan, r);
	c.Clamp(-5.0, r);
	EXPECT_EQ(c.ClampCount(), 2u);
	EXPECT_EQ(c.WarningCount(), 1u);
	EXPECT_EQ(c.WarnedKeyCount(), 1u);
}

TEST(DiaValueSchemaClamperEdge, ClampThenNanOnOneKeyWarnsOnce)
{
	ValueClamper c;
	const ValueInfo r = Range(0.0, 1.0);
	c.Clamp(5.0, r);
	c.Clamp(kNan, r);
	EXPECT_EQ(c.ClampCount(), 1u);
	EXPECT_EQ(c.WarningCount(), 1u);
}

TEST(DiaValueSchemaClamperEdge, PinEntriesWithEmptyFullKeyShareOneWarningKey)
{
	ValueClamper c;
	const ValueInfo a = Make("", ValueType::Float, true, 0.0, true, 1.0);
	const ValueInfo b = Make("", ValueType::Float, true, 0.0, true, 100.0);
	c.Clamp(5.0, a);
	c.Clamp(500.0, b);
	EXPECT_EQ(c.ClampCount(), 2u);
	EXPECT_EQ(c.WarnedKeyCount(), 1u);
	EXPECT_EQ(c.WarningCount(), 1u);
}

TEST(DiaValueSchemaClamperEdge, BindClampCounterNullAndRebind)
{
	Dia::Observation::Metric::Counter first, second;
	ValueClamper c;
	const ValueInfo r = Range(0.0, 1.0);
	c.BindClampCounter(nullptr);
	c.Clamp(5.0, r);	// no counter bound: counts internally only
	EXPECT_EQ(c.ClampCount(), 1u);

	c.BindClampCounter(&first);
	c.Clamp(5.0, r); c.Clamp(5.0, r);
	c.BindClampCounter(&second);
	c.Clamp(5.0, r);
	c.BindClampCounter(nullptr);
	c.Clamp(5.0, r);
	EXPECT_EQ(first.Value(), 2u);
	EXPECT_EQ(second.Value(), 1u);
	EXPECT_EQ(c.ClampCount(), 5u);
}

TEST(DiaValueSchemaClamperEdge, NanAndInRangeDoNotTouchTheCounter)
{
	Dia::Observation::Metric::Counter counter;
	ValueClamper c;
	c.BindClampCounter(&counter);
	const ValueInfo r = Range(0.0, 1.0);
	c.Clamp(kNan, r);
	c.Clamp(0.5, r);
	EXPECT_EQ(counter.Value(), 0u);
}

// ---- properties ----------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaClamperEdge, PropertyDoubleClampStaysInBoundsIsIdempotentAndLeavesInRangeAlone)
{
	std::mt19937 rng(1234);
	std::uniform_real_distribution<double> bound(-1000.0, 1000.0);
	std::uniform_real_distribution<double> value(-5000.0, 5000.0);
	ValueClamper c;
	unsigned distinctKeys = 0;
	for (int k = 0; k < 200; ++k)
	{
		double a = bound(rng), b = bound(rng);
		if (a > b) { std::swap(a, b); }
		const bool hasMin = (rng() % 4 != 0), hasMax = (rng() % 4 != 0);
		const ValueInfo info = Make("g/key" + std::to_string(k), ValueType::Float, hasMin, a, hasMax, b);
		++distinctKeys;
		for (int i = 0; i < 50; ++i)
		{
			const double v = value(rng);
			const double out = c.Clamp(v, info);
			if (hasMin) { EXPECT_GE(out, a); }
			if (hasMax) { EXPECT_LE(out, b); }
			const bool inRange = (!hasMin || v >= a) && (!hasMax || v <= b);
			if (inRange) { EXPECT_EQ(out, v); }
			const unsigned before = c.ClampCount();
			EXPECT_EQ(c.Clamp(out, info), out) << "idempotent";
			EXPECT_EQ(c.ClampCount(), before) << "re-clamping a clamped value is not a clamp";
		}
	}
	EXPECT_LE(c.WarningCount(), distinctKeys);
	EXPECT_EQ(c.WarningCount(), c.WarnedKeyCount());
}

TEST(DiaValueSchemaClamperEdge, PropertyFloatClampStaysInBoundsInDouble)
{
	std::mt19937 rng(1234);
	std::uniform_real_distribution<double> bound(-100.0, 100.0);
	std::uniform_real_distribution<float> value(-300.0f, 300.0f);
	ValueClamper c;
	for (int k = 0; k < 500; ++k)
	{
		double a = bound(rng), b = bound(rng);
		if (a > b) { std::swap(a, b); }
		if (b - a < 0.01) { b = a + 0.01; }
		const ValueInfo info = Range(a, b);
		for (int i = 0; i < 20; ++i)
		{
			const float out = c.Clamp(value(rng), info);
			ASSERT_GE(static_cast<double>(out), a) << "min " << a;
			ASSERT_LE(static_cast<double>(out), b) << "max " << b;
		}
	}
}

TEST(DiaValueSchemaClamperEdge, PropertyIntClampStaysInBounds)
{
	std::mt19937 rng(1234);
	ValueClamper c;
	for (int k = 0; k < 300; ++k)
	{
		int a = static_cast<int>(rng() % 2001) - 1000;
		int b = static_cast<int>(rng() % 2001) - 1000;
		if (a > b) { std::swap(a, b); }
		const ValueInfo info = Range(a, b, ValueType::Int);
		for (int i = 0; i < 20; ++i)
		{
			const int v = static_cast<int>(rng());	// full int range
			const int out = c.Clamp(v, info);
			ASSERT_GE(out, a);
			ASSERT_LE(out, b);
			if (v >= a && v <= b) { ASSERT_EQ(out, v); }
		}
	}
}

TEST(DiaValueSchemaClamperEdge, MillionClampsOverThousandKeysWarnOncePerKey)
{
	ValueClamper c;
	std::vector<ValueInfo> infos;
	for (int k = 0; k < 1000; ++k) { infos.push_back(Make("g/many" + std::to_string(k), ValueType::Float, true, 0.0, true, 1.0)); }
	for (int i = 0; i < 1000000; ++i) { c.Clamp(2.0, infos[i % 1000]); }
	EXPECT_EQ(c.ClampCount(), 1000000u);
	EXPECT_EQ(c.WarnedKeyCount(), 1000u);
	EXPECT_EQ(c.WarningCount(), 1000u);
}
