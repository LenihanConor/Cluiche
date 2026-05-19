#include <gtest/gtest.h>
#include <DiaMetrics/MetricRegistry.h>
#include <DiaMetrics/Testing/MetricFixture.h>

using namespace Dia::Metric;

struct MetricRegistryTest : ::testing::Test
{
    MetricFixture fixture;
};

TEST_F(MetricRegistryTest, AvailableBeforeSessionStart)
{
    // Simply registering without any session infrastructure must not crash or assert.
    Counter* c = MetricRegistry::Instance().RegisterCounter(
        Dia::Core::StringCRC("test.reg.presession"));
    ASSERT_NE(c, nullptr);
    c->Inc();
    EXPECT_EQ(c->Value(), 1u);
}

TEST_F(MetricRegistryTest, FindNullForUnregisteredCounter)
{
    EXPECT_EQ(MetricRegistry::Instance().FindCounter(
        Dia::Core::StringCRC("test.notexist.counter")), nullptr);
}

TEST_F(MetricRegistryTest, FindNullForUnregisteredGauge)
{
    EXPECT_EQ(MetricRegistry::Instance().FindGauge(
        Dia::Core::StringCRC("test.notexist.gauge")), nullptr);
}

TEST_F(MetricRegistryTest, FindNullForUnregisteredHistogram)
{
    EXPECT_EQ(MetricRegistry::Instance().FindHistogram(
        Dia::Core::StringCRC("test.notexist.hist")), nullptr);
}

TEST_F(MetricRegistryTest, SnapshotContainsAllKinds)
{
    float bounds[] = { 1.0f, 2.0f };
    MetricRegistry::Instance().RegisterCounter  (Dia::Core::StringCRC("test.all.counter"));
    MetricRegistry::Instance().RegisterGauge    (Dia::Core::StringCRC("test.all.gauge"));
    MetricRegistry::Instance().RegisterHistogram(Dia::Core::StringCRC("test.all.hist"), bounds, 2);

    MetricSnapshot snap;
    MetricRegistry::Instance().Snapshot(snap);
    EXPECT_GE(snap.entryCount, 3u);
}

TEST_F(MetricRegistryTest, SinkReceivesSnapshot)
{
    struct TestSink : IMetricSink
    {
        int snapshotCount = 0;
        int finalCount    = 0;
        void OnSnapshot(const MetricSnapshot&) override { snapshotCount++; }
        void OnFinal   (const MetricSnapshot&) override { finalCount++;    }
    };

    TestSink sink;
    MetricRegistry::Instance().RegisterSink(&sink);

    MetricSnapshot snap;
    MetricRegistry::Instance().Snapshot(snap);
    MetricRegistry::Instance().NotifySnapshot(snap);
    MetricRegistry::Instance().NotifyFinal(snap);

    EXPECT_EQ(sink.snapshotCount, 1);
    EXPECT_EQ(sink.finalCount, 1);

    MetricRegistry::Instance().UnregisterSink(&sink);
}

TEST_F(MetricRegistryTest, UnregisterSinkStopsCallbacks)
{
    struct TestSink : IMetricSink
    {
        int count = 0;
        void OnSnapshot(const MetricSnapshot&) override { count++; }
        void OnFinal   (const MetricSnapshot&) override { count++; }
    };

    TestSink sink;
    MetricRegistry::Instance().RegisterSink(&sink);
    MetricRegistry::Instance().UnregisterSink(&sink);

    MetricSnapshot snap;
    MetricRegistry::Instance().NotifySnapshot(snap);
    EXPECT_EQ(sink.count, 0);
}
