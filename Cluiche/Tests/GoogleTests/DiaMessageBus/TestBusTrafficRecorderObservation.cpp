// TestBusTrafficRecorderObservation.cpp
//
// Observation instrumentation tests for FileBusRecorder: metrics counters and
// FileBusRecorderHealthReporter. Counters are process-global, so assertions use deltas.
// Debug-only (DIA_DEBUG).

#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <string>

#include <DiaMessageBus/FileBusRecorder.h>
#include <DiaMessageBus/FileBusRecorderHealthReporter.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Health/IHealthReporter.h>

namespace Dia::MessageBus::Testing {

    namespace {
        using Dia::Observation::Metric::MetricRegistry;
        using Dia::Observation::Health::HealthStatus;

        struct ObsTempFile {
            explicit ObsTempFile(const char* name)
                : path((std::filesystem::temp_directory_path() / name).string()) { std::remove(path.c_str()); }
            ~ObsTempFile() { std::remove(path.c_str()); }
            std::string path;
        };

        uint64_t ObsTick(void* ctx) { return (*static_cast<uint64_t*>(ctx))++; }

        FileBusRecorder::Config ObsCfg(uint64_t* tick) {
            FileBusRecorder::Config cfg;
            cfg.tickSource = &ObsTick;
            cfg.tickContext = tick;
            return cfg;
        }

        uint64_t CounterValue(const char* name) {
            auto* c = MetricRegistry::Instance().RegisterCounter(Dia::Core::StringCRC(name));
            return c->Value();
        }

        const Dia::Core::StringCRC kType("ObsMsg");
        const Dia::Core::StringCRC kRouter("broadcast");
    }

    TEST(BusTrafficRecorder_Observation, RecordedCounterIncrementsPerRecord) {
        ObsTempFile f("dia_obs_recorded.dbtr");
        uint64_t tick = 0;
        const uint64_t before = CounterValue("dia.msgbus.recorded");
        FileBusRecorder rec;
        ASSERT_TRUE(rec.Open(f.path.c_str(), ObsCfg(&tick)));
        const uint32_t payload = 7;
        for (int i = 0; i < 5; ++i)
            rec.OnDispatch(kType, kRouter, Pass{}, &payload, sizeof(payload));
        rec.Close();
        EXPECT_EQ(CounterValue("dia.msgbus.recorded") - before, 5u);
    }

    TEST(BusTrafficRecorder_Observation, RecordedBytesMatchesFileSize) {
        ObsTempFile f("dia_obs_bytes.dbtr");
        uint64_t tick = 0;
        const uint64_t before = CounterValue("dia.msgbus.recorded_bytes");
        FileBusRecorder rec;
        ASSERT_TRUE(rec.Open(f.path.c_str(), ObsCfg(&tick)));
        const uint8_t payload[10] = {};
        rec.OnDispatch(kType, kRouter, Pass{}, payload, sizeof(payload));
        rec.OnDispatch(kType, kRouter, Pass{}, nullptr, 0);
        rec.Close();
        const uint64_t fileSize = std::filesystem::file_size(f.path);
        EXPECT_EQ(fileSize, 8u + 24u + 10u + 24u);
        EXPECT_EQ(CounterValue("dia.msgbus.recorded_bytes") - before, fileSize);
    }

    TEST(BusTrafficRecorder_Observation, DroppedCounterIncrementsWhenDisabled) {
        FileBusRecorder rec;
        FileBusRecorder::Config cfg;   // null tick source -> Open fails, recorder disabled
        ASSERT_FALSE(rec.Open("unused.dbtr", cfg));
        const uint64_t droppedBefore = CounterValue("dia.msgbus.record_dropped");
        const uint64_t recordedBefore = CounterValue("dia.msgbus.recorded");
        for (int i = 0; i < 3; ++i)
            rec.OnDispatch(kType, kRouter, Pass{}, nullptr, 0);
        EXPECT_EQ(CounterValue("dia.msgbus.record_dropped") - droppedBefore, 3u);
        EXPECT_EQ(CounterValue("dia.msgbus.recorded") - recordedBefore, 0u);
    }

    TEST(BusTrafficRecorder_Observation, HealthReporterTracksRecorderState) {
        ObsTempFile f("dia_obs_health.dbtr");
        uint64_t tick = 0;
        FileBusRecorder rec;
        FileBusRecorderHealthReporter reporter(Dia::Core::StringCRC("obs.recorder"), rec);

        ASSERT_TRUE(rec.Open(f.path.c_str(), ObsCfg(&tick)));
        reporter.Check();
        EXPECT_EQ(reporter.Report().status, HealthStatus::kOK);

        FileBusRecorder::Config bad;
        EXPECT_FALSE(rec.Open(f.path.c_str(), bad));
        EXPECT_TRUE(rec.IsDisabled());
        reporter.Check();
        EXPECT_EQ(reporter.Report().status, HealthStatus::kFailing);

        ASSERT_TRUE(rec.Open(f.path.c_str(), ObsCfg(&tick)));
        reporter.Check();
        EXPECT_EQ(reporter.Report().status, HealthStatus::kOK);
    }

} // namespace Dia::MessageBus::Testing

#endif // DIA_DEBUG
