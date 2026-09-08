#include <gtest/gtest.h>
#include <DiaObservation/Metric/MetricsFileSink.h>
#include <DiaObservation/Metric/MetricSnapshot.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Testing/MetricFixture.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

using namespace Dia::Observation::Metric;

namespace
{
    const char* kMetricJsonlPath = "test_metric_sink.jsonl";
    const char* kMetricFinalPath = "test_metrics_final.json";
}

struct MetricsFileSinkTest : ::testing::Test
{
    MetricFixture fixture;

    void TearDown() override
    {
        remove(kMetricJsonlPath);
        remove(kMetricFinalPath);
    }

    std::string ReadFile(const char* path)
    {
        std::ifstream f(path);
        if (!f.is_open()) return "";
        return std::string((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    }
};

TEST_F(MetricsFileSinkTest, OnSnapshot_WritesJsonlRecord)
{
    {
        MetricsFileSink sink(kMetricJsonlPath, kMetricFinalPath, "sess-001", 1000000000LL);
        ASSERT_TRUE(sink.IsOpen());

        MetricSnapshot snap;
        std::memset(&snap, 0, sizeof(snap));
        snap.timestampSteadyNs = 5000000000ULL;
        snap.intervalMs = 100;
        snap.entryCount = 1;
        snap.entries[0].name = Dia::Core::StringCRC("test.counter");
        snap.entries[0].kind = MetricEntry::Kind::kCounter;
        snap.entries[0].counterValue = 42;

        sink.OnSnapshot(snap);
    }

    std::string content = ReadFile(kMetricJsonlPath);
    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("\"record_type\":\"metric_snapshot\""), std::string::npos);
    EXPECT_NE(content.find("\"kind\":\"counter\""), std::string::npos);
    EXPECT_NE(content.find("\"value\":42"), std::string::npos);
}

TEST_F(MetricsFileSinkTest, OnFinal_WritesJsonlAndFinalFile)
{
    {
        MetricsFileSink sink(kMetricJsonlPath, kMetricFinalPath, "sess-002", 0);
        ASSERT_TRUE(sink.IsOpen());

        MetricSnapshot snap;
        std::memset(&snap, 0, sizeof(snap));
        snap.timestampSteadyNs = 1000000000ULL;
        snap.intervalMs = 100;
        snap.entryCount = 1;
        snap.entries[0].name = Dia::Core::StringCRC("test.gauge");
        snap.entries[0].kind = MetricEntry::Kind::kGauge;
        snap.entries[0].gaugeValue = 3.14;

        sink.OnFinal(snap);
    }

    std::string jsonl = ReadFile(kMetricJsonlPath);
    EXPECT_NE(jsonl.find("\"record_type\":\"metrics_final\""), std::string::npos);

    std::string final = ReadFile(kMetricFinalPath);
    EXPECT_NE(final.find("\"record_type\":\"metrics_final\""), std::string::npos);
    EXPECT_NE(final.find("\"kind\":\"gauge\""), std::string::npos);
}

TEST_F(MetricsFileSinkTest, Histogram_SerializesBuckets)
{
    {
        MetricsFileSink sink(kMetricJsonlPath, kMetricFinalPath, "sess-003", 0);
        ASSERT_TRUE(sink.IsOpen());

        MetricSnapshot snap;
        std::memset(&snap, 0, sizeof(snap));
        snap.timestampSteadyNs = 2000000000ULL;
        snap.intervalMs = 100;
        snap.entryCount = 1;
        snap.entries[0].name = Dia::Core::StringCRC("test.hist");
        snap.entries[0].kind = MetricEntry::Kind::kHistogram;
        snap.entries[0].histCount = 10;
        snap.entries[0].histSum = 55.0;
        snap.entries[0].p50 = 5.0;
        snap.entries[0].p95 = 9.0;
        snap.entries[0].p99 = 10.0;
        snap.entries[0].bucketCount = 3;
        snap.entries[0].buckets[0] = {1.0f, 3};
        snap.entries[0].buckets[1] = {5.0f, 7};
        snap.entries[0].buckets[2] = {0.0f, 10}; // +Inf

        sink.OnSnapshot(snap);
    }

    std::string content = ReadFile(kMetricJsonlPath);
    EXPECT_NE(content.find("\"kind\":\"histogram\""), std::string::npos);
    EXPECT_NE(content.find("\"+Inf\""), std::string::npos);
    EXPECT_NE(content.find("\"p50\":"), std::string::npos);
}

TEST_F(MetricsFileSinkTest, InvalidPath_IsOpenReturnsFalse)
{
    MetricsFileSink sink("", "", "sess", 0);
    EXPECT_FALSE(sink.IsOpen());
}
