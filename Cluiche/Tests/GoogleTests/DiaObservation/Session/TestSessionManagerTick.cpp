#include <gtest/gtest.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/IMetricSink.h>
#include <DiaObservation/Metric/MetricSnapshot.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Testing/MetricFixture.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Testing/HealthFixture.h>
#include <DiaObservation/Session/SessionManager.h>

#include <cstring>

using namespace Dia::Observation;

namespace
{
    struct CountingSink : Metric::IMetricSink
    {
        int snapshotCount = 0;
        int finalCount = 0;
        void OnSnapshot(const Metric::MetricSnapshot&) override { ++snapshotCount; }
        void OnFinal(const Metric::MetricSnapshot&) override { ++finalCount; }
    };
}

struct SessionManagerTickTest : ::testing::Test
{
    Metric::MetricFixture metricFixture;
    Testing::HealthFixture healthFixture;
    SessionManager mgr;

    SessionConfig MakeConfig()
    {
        SessionConfig c;
        std::memset(&c, 0, sizeof(c));
        strncpy_s(c.appName, "test", sizeof(c.appName) - 1);
        strncpy_s(c.buildVersion, "1.0", sizeof(c.buildVersion) - 1);
        strncpy_s(c.buildConfig, "debug", sizeof(c.buildConfig) - 1);
        strncpy_s(c.outRootDir, ".", sizeof(c.outRootDir) - 1);
        return c;
    }

    void TearDown() override
    {
        if (mgr.IsStarted())
            mgr.Stop();
    }
};

TEST_F(SessionManagerTickTest, MetricSnapshot_FiresAt100ms)
{
    CountingSink sink;
    Metric::MetricRegistry::Instance().RegisterSink(&sink);

    // Register a gauge so the registry is non-empty; first snapshot differs from empty mLastSnapshot
    Metric::Gauge* g = Metric::MetricRegistry::Instance().RegisterGauge(Dia::Core::StringCRC("test.gauge.tick100"));
    g->Set(1.0);

    ObservationConfig obs;
    std::memset(&obs, 0, sizeof(obs));
    obs.enableObservationFileSink = false;
    obs.enableTraceFileSink = false;
    obs.enableMetricsFileSink = false;

    mgr.Start(MakeConfig(), obs);

    // Tick for 99ms — should not fire
    mgr.Tick(0.099f);
    EXPECT_EQ(sink.snapshotCount, 0);

    // Tick for 2ms — accumulator crosses 100ms
    mgr.Tick(0.002f);
    EXPECT_EQ(sink.snapshotCount, 1);

    Metric::MetricRegistry::Instance().UnregisterSink(&sink);
}

TEST_F(SessionManagerTickTest, MetricSnapshot_AccumulatorSubtractsPreventsReset)
{
    CountingSink sink;
    Metric::MetricRegistry::Instance().RegisterSink(&sink);

    // Register a gauge so registry is non-empty; first snapshot differs from empty mLastSnapshot.
    // Change value between ticks so the second interval also detects a change.
    Metric::Gauge* g = Metric::MetricRegistry::Instance().RegisterGauge(Dia::Core::StringCRC("test.gauge.tickaccum"));
    g->Set(1.0);

    ObservationConfig obs;
    std::memset(&obs, 0, sizeof(obs));
    obs.enableObservationFileSink = false;
    obs.enableTraceFileSink = false;
    obs.enableMetricsFileSink = false;

    mgr.Start(MakeConfig(), obs);

    // Tick 150ms — should fire once, leave 50ms remainder
    mgr.Tick(0.150f);
    EXPECT_EQ(sink.snapshotCount, 1);

    // Change gauge so second snapshot differs from first
    g->Set(2.0);

    // Tick 60ms — accumulator = 50 + 60 = 110ms >= 100, fires again
    mgr.Tick(0.060f);
    EXPECT_EQ(sink.snapshotCount, 2);

    Metric::MetricRegistry::Instance().UnregisterSink(&sink);
}

TEST_F(SessionManagerTickTest, HealthPoll_FiresAt500ms)
{
    ObservationConfig obs;
    std::memset(&obs, 0, sizeof(obs));
    obs.enableObservationFileSink = false;
    obs.enableTraceFileSink = false;
    obs.enableMetricsFileSink = false;

    Testing::MockHealthReporter reporter(Dia::Core::StringCRC("test.tick"));
    Health::HealthRegistry::Instance().Register(&reporter);

    mgr.Start(MakeConfig(), obs);

    reporter.SetDegraded(Dia::Core::StringCRC("slow"));

    // Tick 499ms — health poll should not fire
    mgr.Tick(0.499f);
    // Tick 2ms — crosses 500ms threshold
    mgr.Tick(0.002f);

    // Stop before reporter goes out of scope
    mgr.Stop();

    // The poll ran during Tick, which updates lastKnownStatus in HealthRegistry.
    // A second poll with no further change should produce 0 transitions.
    Health::HealthRegistry::Transition out[32];
    unsigned int count = 0;
    Health::HealthRegistry::Instance().PollTransitions(out, 32, count);
    EXPECT_EQ(count, 0u);

    Health::HealthRegistry::Instance().Unregister(&reporter);
}

TEST_F(SessionManagerTickTest, FinalSnapshot_SentToAllSinks)
{
    CountingSink sink;
    Metric::MetricRegistry::Instance().RegisterSink(&sink);

    ObservationConfig obs;
    std::memset(&obs, 0, sizeof(obs));
    obs.enableObservationFileSink = false;
    obs.enableTraceFileSink = false;
    obs.enableMetricsFileSink = false;

    mgr.Start(MakeConfig(), obs);
    mgr.Stop();

    EXPECT_GE(sink.finalCount, 1);

    Metric::MetricRegistry::Instance().UnregisterSink(&sink);
}
