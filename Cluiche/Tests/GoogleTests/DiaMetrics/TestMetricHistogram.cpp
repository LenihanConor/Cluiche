#include <gtest/gtest.h>
#include <DiaMetrics/MetricRegistry.h>
#include <DiaMetrics/Testing/MetricFixture.h>

using namespace Dia::Metric;

struct MetricHistogramTest : ::testing::Test
{
    MetricFixture fixture;
};

TEST_F(MetricHistogramTest, ObserveInCorrectBucket)
{
    float bounds[] = { 10.0f, 20.0f, 30.0f };
    Histogram* h = MetricRegistry::Instance().RegisterHistogram(
        Dia::Core::StringCRC("test.hist"), bounds, 3);
    ASSERT_NE(h, nullptr);

    h->Observe(5.0);   // bucket 0 (<=10)
    h->Observe(15.0);  // bucket 1 (<=20)
    h->Observe(25.0);  // bucket 2 (<=30)
    h->Observe(35.0);  // +Inf

    Histogram::Data data;
    h->ReadData(data);

    EXPECT_EQ(data.counts[0], 1u); // <=10
    EXPECT_EQ(data.counts[1], 1u); // <=20
    EXPECT_EQ(data.counts[2], 1u); // <=30
    EXPECT_EQ(data.counts[3], 1u); // +Inf
    EXPECT_EQ(data.total, 4u);
    EXPECT_DOUBLE_EQ(data.sum, 80.0);
}

TEST_F(MetricHistogramTest, PercentilesWithinTolerance)
{
    // 100 observations spread: 50 at 5ms, 40 at 15ms, 10 at 25ms
    float bounds[] = { 10.0f, 20.0f, 30.0f };
    Histogram* h = MetricRegistry::Instance().RegisterHistogram(
        Dia::Core::StringCRC("test.hist.perc"), bounds, 3);
    ASSERT_NE(h, nullptr);

    for (int i = 0; i < 50; ++i) h->Observe(5.0);
    for (int i = 0; i < 40; ++i) h->Observe(15.0);
    for (int i = 0; i < 10; ++i) h->Observe(25.0);

    MetricSnapshot snap;
    MetricRegistry::Instance().Snapshot(snap);

    bool found = false;
    for (unsigned int i = 0; i < snap.entryCount; ++i)
    {
        if (snap.entries[i].name == Dia::Core::StringCRC("test.hist.perc"))
        {
            // p50: 50th value is at boundary of bucket 0 (<=10)
            EXPECT_LE(snap.entries[i].p50, 10.0);
            // p95: 95th value falls in bucket 2 (<=30) since bucket 1 only covers up to 90th
            EXPECT_LE(snap.entries[i].p95, 30.0);
            // p99: 99th value falls in bucket 2 (<=30)
            EXPECT_LE(snap.entries[i].p99, 30.0);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(MetricHistogramTest, IdempotentRegistration)
{
    float bounds[] = { 5.0f };
    Histogram* h1 = MetricRegistry::Instance().RegisterHistogram(
        Dia::Core::StringCRC("test.hist.idem"), bounds, 1);
    Histogram* h2 = MetricRegistry::Instance().RegisterHistogram(
        Dia::Core::StringCRC("test.hist.idem"), bounds, 1);
    EXPECT_EQ(h1, h2);
}

TEST_F(MetricHistogramTest, FindReturnsNullForUnregistered)
{
    Histogram* h = MetricRegistry::Instance().FindHistogram(
        Dia::Core::StringCRC("test.hist.nothere"));
    EXPECT_EQ(h, nullptr);
}

TEST_F(MetricHistogramTest, BucketCountsExceedingMaxBoundsClipped)
{
    // Pass 20 bounds — only first 16 should be stored
    float bounds[20];
    for (int i = 0; i < 20; ++i) bounds[i] = static_cast<float>((i + 1) * 10);

    Histogram* h = MetricRegistry::Instance().RegisterHistogram(
        Dia::Core::StringCRC("test.hist.clip"), bounds, 20);
    ASSERT_NE(h, nullptr);

    Histogram::Data data;
    h->ReadData(data);
    EXPECT_EQ(data.bucketCount, static_cast<unsigned int>(kHistogramMaxBounds));
}
