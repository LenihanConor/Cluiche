#include <gtest/gtest.h>
#include <DiaApplicationFlow/Metrics/MetricsCollectorModule.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Application;
using namespace Dia::Core;

class TestPU : public ProcessingUnit
{
public:
    TestPU(const char* name) : ProcessingUnit(StringCRC(name), -1.0f, 16, 16) {}
    virtual bool FlaggedToStopUpdating() const override { return true; }
};

TEST(MetricsCollector, InitialSnapshotIsEmpty)
{
    TestPU pu("TestPU");
    MetricsCollectorModule collector(&pu);

    const MetricsSnapshot& snap = collector.GetSnapshot();
    EXPECT_EQ(snap.puCount, 0u);
    EXPECT_FLOAT_EQ(snap.memoryUsedMB, 0.0f);
    EXPECT_FLOAT_EQ(snap.uptimeSeconds, 0.0f);
}

TEST(MetricsCollector, ReportFrameAddsPU)
{
    TestPU pu("TestPU");
    MetricsCollectorModule collector(&pu);

    collector.ReportFrame(StringCRC("MainPU"), "MainPU", 16.6f);

    const MetricsSnapshot& snap = collector.GetSnapshot();
    EXPECT_EQ(snap.puCount, 1u);
    EXPECT_STREQ(snap.puMetrics[0].name, "MainPU");
    EXPECT_GT(snap.puMetrics[0].fps, 0.0f);
    EXPECT_FLOAT_EQ(snap.puMetrics[0].frameTimeMs, 16.6f);
}

TEST(MetricsCollector, ReportFrameSamePUUpdatesInPlace)
{
    TestPU pu("TestPU");
    MetricsCollectorModule collector(&pu);

    collector.ReportFrame(StringCRC("MainPU"), "MainPU", 16.6f);
    collector.ReportFrame(StringCRC("MainPU"), "MainPU", 33.3f);

    const MetricsSnapshot& snap = collector.GetSnapshot();
    EXPECT_EQ(snap.puCount, 1u);
    EXPECT_FLOAT_EQ(snap.puMetrics[0].frameTimeMs, 33.3f);
}

TEST(MetricsCollector, ReportFrameMultiplePUs)
{
    TestPU pu("TestPU");
    MetricsCollectorModule collector(&pu);

    collector.ReportFrame(StringCRC("MainPU"), "MainPU", 16.6f);
    collector.ReportFrame(StringCRC("RenderPU"), "RenderPU", 8.0f);
    collector.ReportFrame(StringCRC("SimPU"), "SimPU", 33.3f);

    const MetricsSnapshot& snap = collector.GetSnapshot();
    EXPECT_EQ(snap.puCount, 3u);
    EXPECT_STREQ(snap.puMetrics[0].name, "MainPU");
    EXPECT_STREQ(snap.puMetrics[1].name, "RenderPU");
    EXPECT_STREQ(snap.puMetrics[2].name, "SimPU");
}

TEST(MetricsCollector, MaxPULimitRespected)
{
    TestPU pu("TestPU");
    MetricsCollectorModule collector(&pu);

    for (unsigned int i = 0; i < MetricsSnapshot::kMaxProcessingUnits; ++i)
    {
        char name[32];
        sprintf_s(name, sizeof(name), "PU%u", i);
        collector.ReportFrame(StringCRC(name), name, 16.0f);
    }

    EXPECT_EQ(collector.GetSnapshot().puCount, MetricsSnapshot::kMaxProcessingUnits);

    collector.ReportFrame(StringCRC("OverflowPU"), "OverflowPU", 16.0f);
    EXPECT_EQ(collector.GetSnapshot().puCount, MetricsSnapshot::kMaxProcessingUnits);
}

TEST(MetricsCollector, FPSSmoothing)
{
    TestPU pu("TestPU");
    MetricsCollectorModule collector(&pu);

    collector.ReportFrame(StringCRC("MainPU"), "MainPU", 16.6f);
    float fps1 = collector.GetSnapshot().puMetrics[0].fps;

    collector.ReportFrame(StringCRC("MainPU"), "MainPU", 16.6f);
    float fps2 = collector.GetSnapshot().puMetrics[0].fps;

    // With EMA, after two identical frames, FPS should converge
    float instantFps = 1000.0f / 16.6f;
    EXPECT_GT(fps1, 0.0f);
    EXPECT_GT(fps2, 0.0f);
    // Second sample should be closer to target than first
    EXPECT_LT(fabs(fps2 - instantFps), fabs(fps1 - instantFps) + 0.1f);
}

TEST(MetricsCollector, UptimeIncrements)
{
    TestPU pu("TestPU");
    MetricsCollectorModule collector(&pu);

    collector.ReportFrame(StringCRC("MainPU"), "MainPU", 100.0f); // 0.1s
    float up1 = collector.GetSnapshot().uptimeSeconds;

    collector.ReportFrame(StringCRC("MainPU"), "MainPU", 200.0f); // 0.2s
    float up2 = collector.GetSnapshot().uptimeSeconds;

    EXPECT_GT(up2, up1);
    EXPECT_NEAR(up1, 0.1f, 0.01f);
    EXPECT_NEAR(up2, 0.3f, 0.01f);
}

TEST(MetricsCollector, ZeroDeltaTimeHandled)
{
    TestPU pu("TestPU");
    MetricsCollectorModule collector(&pu);

    collector.ReportFrame(StringCRC("MainPU"), "MainPU", 0.0f);

    const MetricsSnapshot& snap = collector.GetSnapshot();
    EXPECT_EQ(snap.puCount, 1u);
    EXPECT_FLOAT_EQ(snap.puMetrics[0].fps, 0.0f);
}

TEST(MetricsCollector, ProcessingUnitInjection)
{
    TestPU pu("TestPU");
    EXPECT_EQ(pu.GetMetricsCollector(), nullptr);

    MetricsCollectorModule collector(&pu);
    pu.SetMetricsCollector(&collector);
    EXPECT_EQ(pu.GetMetricsCollector(), &collector);

    pu.SetMetricsCollector(nullptr);
    EXPECT_EQ(pu.GetMetricsCollector(), nullptr);
}
