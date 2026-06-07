////////////////////////////////////////////////////////////////////////////////
// Filename: TestPUTiming.cpp
// GoogleTest suite — ProcessingUnit per-tick timing gauge (Task 34/35)
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Module.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Testing/MetricFixture.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <thread>
#include <chrono>

using namespace Dia::ApplicationFlow;
using namespace Dia::Core;
using namespace Dia::Observation::Metric;

struct PUTimingTest : ::testing::Test
{
    MetricFixture fixture;
};

TEST_F(PUTimingTest, GaugeRegisteredOnConstruction)
{
    ProcessingUnit pu(StringCRC("pu_timing_test"), 60.0f, false);

    Gauge* g = MetricRegistry::Instance().FindGauge(StringCRC("pu.pu_timing_test.last_tick_ms"));
    EXPECT_NE(g, nullptr) << "Expected gauge 'pu.pu_timing_test.last_tick_ms' in registry";
}

TEST_F(PUTimingTest, GaugeUpdatesAfterUpdate)
{
    ProcessingUnit pu(StringCRC("pu_gauge_upd"), 60.0f, false);
    pu.Update(0.016f);

    Gauge* g = MetricRegistry::Instance().FindGauge(StringCRC("pu.pu_gauge_upd.last_tick_ms"));
    ASSERT_NE(g, nullptr);
    EXPECT_GE(g->Value(), 0.0) << "Tick time should be non-negative after Update";
}

TEST_F(PUTimingTest, GaugeDefaultsToZeroBeforeFirstUpdate)
{
    ProcessingUnit pu(StringCRC("pu_zero"), 60.0f, false);

    Gauge* g = MetricRegistry::Instance().FindGauge(StringCRC("pu.pu_zero.last_tick_ms"));
    ASSERT_NE(g, nullptr);
    EXPECT_DOUBLE_EQ(g->Value(), 0.0) << "Gauge should default to zero before first Update";
}
