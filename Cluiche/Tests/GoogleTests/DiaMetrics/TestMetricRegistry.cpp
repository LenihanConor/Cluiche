#include <cstdio>
#include <gtest/gtest.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Testing/MetricFixture.h>

using namespace Dia::Observation::Metric;

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

// ---- Unregister (retire / revive) ----

namespace
{
    struct VisitCount { int n = 0; };
    void CountVisit(const Dia::Core::StringCRC&, MetricKind, const MetricInfo*, void* user)
    {
        ++static_cast<VisitCount*>(user)->n;
    }

    bool SnapshotHas(const char* name)
    {
        MetricSnapshot snap;
        MetricRegistry::Instance().Snapshot(snap);
        for (unsigned i = 0; i < snap.entryCount; ++i)
            if (snap.entries[i].name == Dia::Core::StringCRC(name)) return true;
        return false;
    }
}

TEST_F(MetricRegistryTest, UnregisterHidesMetricFromAllViews)
{
    auto& reg = MetricRegistry::Instance();
    static const MetricInfo kInfo{ "L", "D", "U" };
    const float bounds[] = { 1.0f, 2.0f };
    reg.RegisterCounter(Dia::Core::StringCRC("test.unreg.c"), kInfo);
    reg.RegisterGauge(Dia::Core::StringCRC("test.unreg.g"));
    reg.RegisterHistogram(Dia::Core::StringCRC("test.unreg.h"), bounds, 2);

    for (const char* n : { "test.unreg.c", "test.unreg.g", "test.unreg.h" })
        EXPECT_TRUE(SnapshotHas(n)) << n;
    EXPECT_NE(reg.GetInfo(Dia::Core::StringCRC("test.unreg.c")), nullptr);

    EXPECT_TRUE(reg.Unregister(Dia::Core::StringCRC("test.unreg.c")));
    EXPECT_TRUE(reg.Unregister(Dia::Core::StringCRC("test.unreg.g")));
    EXPECT_TRUE(reg.Unregister(Dia::Core::StringCRC("test.unreg.h")));

    for (const char* n : { "test.unreg.c", "test.unreg.g", "test.unreg.h" })
        EXPECT_FALSE(SnapshotHas(n)) << n;
    EXPECT_EQ(reg.FindCounter(Dia::Core::StringCRC("test.unreg.c")), nullptr);
    EXPECT_EQ(reg.FindGauge(Dia::Core::StringCRC("test.unreg.g")), nullptr);
    EXPECT_EQ(reg.FindHistogram(Dia::Core::StringCRC("test.unreg.h")), nullptr);
    EXPECT_EQ(reg.GetInfo(Dia::Core::StringCRC("test.unreg.c")), nullptr);
    VisitCount vc;
    reg.ForEachMetric(&CountVisit, &vc);
    EXPECT_EQ(vc.n, 0);
}

TEST_F(MetricRegistryTest, RetiredPointersStaySafeAndInvisible)
{
    auto& reg = MetricRegistry::Instance();
    const float bounds[] = { 1.0f };
    Counter* c = reg.RegisterCounter(Dia::Core::StringCRC("test.unreg.safe.c"));
    Gauge* g = reg.RegisterGauge(Dia::Core::StringCRC("test.unreg.safe.g"));
    Histogram* h = reg.RegisterHistogram(Dia::Core::StringCRC("test.unreg.safe.h"), bounds, 1);
    reg.Unregister(Dia::Core::StringCRC("test.unreg.safe.c"));
    reg.Unregister(Dia::Core::StringCRC("test.unreg.safe.g"));
    reg.Unregister(Dia::Core::StringCRC("test.unreg.safe.h"));

    c->Inc(5);
    g->Set(3.0);
    h->Observe(0.5);
    EXPECT_FALSE(SnapshotHas("test.unreg.safe.c"));
    EXPECT_FALSE(SnapshotHas("test.unreg.safe.g"));
    EXPECT_FALSE(SnapshotHas("test.unreg.safe.h"));
}

TEST_F(MetricRegistryTest, ReRegisterRevivesSameObjectZeroed)
{
    auto& reg = MetricRegistry::Instance();
    const float bounds[] = { 1.0f, 2.0f };
    static const MetricInfo kInfo{ "L", "D", "U" };
    Counter* c = reg.RegisterCounter(Dia::Core::StringCRC("test.revive.c"), kInfo);
    Gauge* g = reg.RegisterGauge(Dia::Core::StringCRC("test.revive.g"));
    Histogram* h = reg.RegisterHistogram(Dia::Core::StringCRC("test.revive.h"), bounds, 2);
    c->Inc(7);
    g->Set(4.0);
    h->Observe(1.5);

    reg.Unregister(Dia::Core::StringCRC("test.revive.c"));
    reg.Unregister(Dia::Core::StringCRC("test.revive.g"));
    reg.Unregister(Dia::Core::StringCRC("test.revive.h"));
    const unsigned warnings = reg.ConflictWarningCount();

    EXPECT_EQ(reg.RegisterCounter(Dia::Core::StringCRC("test.revive.c"), kInfo), c);
    EXPECT_EQ(reg.RegisterGauge(Dia::Core::StringCRC("test.revive.g")), g);
    EXPECT_EQ(reg.RegisterHistogram(Dia::Core::StringCRC("test.revive.h"), bounds, 2), h);
    EXPECT_EQ(c->Value(), 0u);
    EXPECT_EQ(g->Value(), 0.0);
    Histogram::Data data;
    h->ReadData(data);
    EXPECT_EQ(data.total, 0u);
    EXPECT_EQ(data.sum, 0.0);
    for (unsigned i = 0; i <= data.bucketCount; ++i) EXPECT_EQ(data.counts[i], 0u);
    EXPECT_EQ(reg.FindCounter(Dia::Core::StringCRC("test.revive.c")), c);
    EXPECT_NE(reg.GetInfo(Dia::Core::StringCRC("test.revive.c")), nullptr);
    EXPECT_TRUE(SnapshotHas("test.revive.c"));
    EXPECT_EQ(reg.ConflictWarningCount(), warnings);
}

TEST_F(MetricRegistryTest, UnregisterUnknownOrAlreadyRetiredReturnsFalse)
{
    auto& reg = MetricRegistry::Instance();
    EXPECT_FALSE(reg.Unregister(Dia::Core::StringCRC("test.unreg.unknown")));
    reg.RegisterCounter(Dia::Core::StringCRC("test.unreg.twice"));
    EXPECT_TRUE(reg.Unregister(Dia::Core::StringCRC("test.unreg.twice")));
    const unsigned gen = reg.Generation();
    EXPECT_FALSE(reg.Unregister(Dia::Core::StringCRC("test.unreg.twice")));
    EXPECT_EQ(reg.Generation(), gen);
}

TEST_F(MetricRegistryTest, RetiredEntriesDoNotCountTowardSnapshotCap)
{
    auto& reg = MetricRegistry::Instance();
    char name[48];
    for (unsigned i = 0; i < kMaxMetricEntries; ++i)
    {
        std::snprintf(name, sizeof(name), "test.cap.old.%u", i);
        reg.RegisterCounter(Dia::Core::StringCRC(name));
        reg.Unregister(Dia::Core::StringCRC(name));
    }
    reg.RegisterCounter(Dia::Core::StringCRC("test.cap.live"));
    EXPECT_TRUE(SnapshotHas("test.cap.live"));
    MetricSnapshot snap;
    reg.Snapshot(snap);
    EXPECT_EQ(snap.entryCount, 1u);
}

TEST_F(MetricRegistryTest, GenerationIncreasesOnRetireAndRevive)
{
    auto& reg = MetricRegistry::Instance();
    reg.RegisterGauge(Dia::Core::StringCRC("test.gen.g"));
    unsigned gen = reg.Generation();
    reg.Unregister(Dia::Core::StringCRC("test.gen.g"));
    EXPECT_GT(reg.Generation(), gen);
    gen = reg.Generation();
    reg.RegisterGauge(Dia::Core::StringCRC("test.gen.g"));
    EXPECT_GT(reg.Generation(), gen);
}
