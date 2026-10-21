#include <gtest/gtest.h>
#include <DiaValueSchema/ValueClamper.h>
#include <DiaObservation/Metric/Counter.h>
#include <cmath>
#include <limits>

using namespace Dia::ValueSchema;

namespace
{
	ValueInfo Make(const char* key, ValueType type, bool hasMin, double mn, bool hasMax, double mx)
	{
		ValueInfo i;
		i.mFullKey = key;
		i.mType = type;
		i.mHasMin = hasMin; i.mMin = mn;
		i.mHasMax = hasMax; i.mMax = mx;
		return i;
	}
}

TEST(DiaValueSchemaClamp, InRangeUnchangedNoWarning)
{
	ValueClamper c;
	auto i = Make("g/rate", ValueType::Float, true, 0, true, 2);
	EXPECT_DOUBLE_EQ(c.Clamp(1.5, i), 1.5);
	EXPECT_EQ(c.ClampCount(), 0u);
	EXPECT_EQ(c.WarningCount(), 0u);
}

TEST(DiaValueSchemaClamp, BelowMinClamps)
{
	ValueClamper c;
	auto i = Make("g/rate", ValueType::Float, true, 0, true, 2);
	EXPECT_DOUBLE_EQ(c.Clamp(-1.0, i), 0.0);
	EXPECT_EQ(c.ClampCount(), 1u);
}

TEST(DiaValueSchemaClamp, AboveMaxClamps)
{
	ValueClamper c;
	auto i = Make("g/rate", ValueType::Float, true, 0, true, 2);
	EXPECT_DOUBLE_EQ(c.Clamp(2.5, i), 2.0);
	EXPECT_FLOAT_EQ(c.Clamp(5.0f, i), 2.0f);
}

TEST(DiaValueSchemaClamp, OnlyMinOnlyMaxNoRange)
{
	ValueClamper c;
	auto minOnly = Make("g/a", ValueType::Float, true, 0, false, 0);
	auto maxOnly = Make("g/b", ValueType::Float, false, 0, true, 10);
	auto none = Make("g/c", ValueType::Float, false, 0, false, 0);
	EXPECT_DOUBLE_EQ(c.Clamp(-5.0, minOnly), 0.0);
	EXPECT_DOUBLE_EQ(c.Clamp(1e9, minOnly), 1e9);
	EXPECT_DOUBLE_EQ(c.Clamp(50.0, maxOnly), 10.0);
	EXPECT_DOUBLE_EQ(c.Clamp(-1e9, maxOnly), -1e9);
	EXPECT_DOUBLE_EQ(c.Clamp(-1e9, none), -1e9);
	EXPECT_EQ(c.ClampCount(), 2u);
}

TEST(DiaValueSchemaClamp, IntType)
{
	ValueClamper c;
	auto i = Make("g/n", ValueType::Int, true, 1, true, 5);
	EXPECT_EQ(c.Clamp(3, i), 3);
	EXPECT_EQ(c.Clamp(0, i), 1);
	EXPECT_EQ(c.Clamp(9, i), 5);
	EXPECT_EQ(c.ClampCount(), 2u);
}

TEST(DiaValueSchemaClamp, NonNumericPassThrough)
{
	ValueClamper c;
	for (ValueType t : { ValueType::Bool, ValueType::Enum, ValueType::String, ValueType::Ref, ValueType::RefList, ValueType::Map })
	{
		auto i = Make("g/x", t, true, 0, true, 1);
		EXPECT_DOUBLE_EQ(c.Clamp(99.0, i), 99.0);
		EXPECT_EQ(c.Clamp(99, i), 99);
	}
	EXPECT_EQ(c.ClampCount(), 0u);
	EXPECT_EQ(c.WarningCount(), 0u);
}

TEST(DiaValueSchemaClamp, OneWarningPerKeyAcrossManyClamps)
{
	ValueClamper c;
	auto i = Make("g/rate", ValueType::Float, true, 0, true, 2);
	for (int n = 0; n < 10; ++n) { c.Clamp(5.0, i); }
	EXPECT_EQ(c.ClampCount(), 10u);
	EXPECT_EQ(c.WarnedKeyCount(), 1u);
	EXPECT_EQ(c.WarningCount(), 1u);
}

TEST(DiaValueSchemaClamp, TwoKeysEachWarnOnce)
{
	ValueClamper c;
	auto a = Make("g/a", ValueType::Float, true, 0, true, 2);
	auto b = Make("g/b", ValueType::Float, true, 0, true, 2);
	c.Clamp(5.0, a); c.Clamp(5.0, a); c.Clamp(5.0, b); c.Clamp(5.0, b);
	EXPECT_EQ(c.WarnedKeyCount(), 2u);
	EXPECT_EQ(c.WarningCount(), 2u);
	EXPECT_EQ(c.ClampCount(), 4u);
}

TEST(DiaValueSchemaClamp, NanPassesThroughWarnsOnce)
{
	ValueClamper c;
	auto i = Make("g/rate", ValueType::Float, true, 0, true, 2);
	const double nan = std::numeric_limits<double>::quiet_NaN();
	EXPECT_TRUE(std::isnan(c.Clamp(nan, i)));
	EXPECT_TRUE(std::isnan(c.Clamp(nan, i)));
	EXPECT_EQ(c.WarningCount(), 1u);
	EXPECT_EQ(c.ClampCount(), 0u);
}

TEST(DiaValueSchemaClamp, MetricCounterIncrementedOnEveryClamp)
{
	Dia::Observation::Metric::Counter counter;
	ValueClamper c;
	c.BindClampCounter(&counter);
	auto i = Make("g/rate", ValueType::Float, true, 0, true, 2);
	c.Clamp(5.0, i); c.Clamp(5.0, i); c.Clamp(1.0, i); c.Clamp(-3.0, i);
	EXPECT_EQ(counter.Value(), 3u);
	EXPECT_EQ(c.ClampCount(), 3u);
}

TEST(DiaValueSchemaClamp, InstancesDoNotShareState)
{
	ValueClamper a, b;
	auto i = Make("g/rate", ValueType::Float, true, 0, true, 2);
	a.Clamp(5.0, i);
	b.Clamp(5.0, i);
	EXPECT_EQ(a.WarningCount(), 1u);
	EXPECT_EQ(b.WarningCount(), 1u);
	EXPECT_EQ(a.ClampCount(), 1u);
}
