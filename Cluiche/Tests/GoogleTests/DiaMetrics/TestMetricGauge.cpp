#include <gtest/gtest.h>
#include <DiaMetrics/MetricRegistry.h>
#include <DiaMetrics/Testing/MetricFixture.h>

#include <atomic>
#include <thread>
#include <vector>

using namespace Dia::Metric;

struct MetricGaugeTest : ::testing::Test
{
    MetricFixture fixture;
};

TEST_F(MetricGaugeTest, SetAndGetValue)
{
    Gauge* g = MetricRegistry::Instance().RegisterGauge(Dia::Core::StringCRC("test.gauge"));
    ASSERT_NE(g, nullptr);
    g->Set(42.5);
    EXPECT_DOUBLE_EQ(g->Value(), 42.5);
}

TEST_F(MetricGaugeTest, DefaultValueIsZero)
{
    Gauge* g = MetricRegistry::Instance().RegisterGauge(Dia::Core::StringCRC("test.gauge.zero"));
    ASSERT_NE(g, nullptr);
    EXPECT_DOUBLE_EQ(g->Value(), 0.0);
}

TEST_F(MetricGaugeTest, LastWriterWins_Concurrent)
{
    Gauge* g = MetricRegistry::Instance().RegisterGauge(Dia::Core::StringCRC("test.gauge.concurrent"));
    ASSERT_NE(g, nullptr);

    std::atomic<bool> start{ false };
    auto writer = [&](double val) {
        while (!start.load()) {}
        g->Set(val);
    };

    std::thread t1(writer, 1.0);
    std::thread t2(writer, 2.0);
    start.store(true);
    t1.join();
    t2.join();

    double v = g->Value();
    EXPECT_TRUE(v == 1.0 || v == 2.0);
}

TEST_F(MetricGaugeTest, IdempotentRegistration)
{
    Gauge* g1 = MetricRegistry::Instance().RegisterGauge(Dia::Core::StringCRC("test.gauge.idem"));
    Gauge* g2 = MetricRegistry::Instance().RegisterGauge(Dia::Core::StringCRC("test.gauge.idem"));
    EXPECT_EQ(g1, g2);
}

TEST_F(MetricGaugeTest, FindReturnsNullForUnregistered)
{
    Gauge* g = MetricRegistry::Instance().FindGauge(Dia::Core::StringCRC("test.gauge.nothere"));
    EXPECT_EQ(g, nullptr);
}

TEST_F(MetricGaugeTest, SnapshotContainsGauge)
{
    Gauge* g = MetricRegistry::Instance().RegisterGauge(Dia::Core::StringCRC("test.gauge.snap"));
    g->Set(99.9);

    MetricSnapshot snap;
    MetricRegistry::Instance().Snapshot(snap);

    bool found = false;
    for (unsigned int i = 0; i < snap.entryCount; ++i)
    {
        if (snap.entries[i].name == Dia::Core::StringCRC("test.gauge.snap"))
        {
            EXPECT_EQ(snap.entries[i].kind, MetricEntry::Kind::kGauge);
            EXPECT_DOUBLE_EQ(snap.entries[i].gaugeValue, 99.9);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}
