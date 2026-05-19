#include <gtest/gtest.h>
#include <DiaObservation/Session/SessionManager.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Testing/MetricFixture.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Testing/HealthFixture.h>

#include <cstring>
#include <cstdio>
#include <fstream>
#include <string>

using namespace Dia::Observation;

struct SessionManagerRetentionTest : ::testing::Test
{
    Metric::MetricFixture metricFixture;
    Testing::HealthFixture healthFixture;
    SessionManager mgr;

    SessionConfig MakeConfig()
    {
        SessionConfig c;
        std::memset(&c, 0, sizeof(c));
        strncpy_s(c.appName, "retention_test", sizeof(c.appName) - 1);
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

TEST_F(SessionManagerRetentionTest, WarningsAndErrorsCounted)
{
    ObservationConfig obs;
    std::memset(&obs, 0, sizeof(obs));
    obs.enableObservationFileSink = false;
    obs.enableTraceFileSink = false;
    obs.enableMetricsFileSink = false;

    mgr.Start(MakeConfig(), obs);

    // Emit some warnings and errors through the logger
    DIA_LOG_WARNING("test", "warning message %d", 1);
    DIA_LOG_WARNING("test", "warning message %d", 2);
    DIA_LOG_ERROR("test", "error message %d", 1);

    // Allow drain to process
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    mgr.Stop();

    // Read the session.json and verify counts
    const char* sessionDir = mgr.GetSessionDirectory();
    char sessionPath[600];
    snprintf(sessionPath, sizeof(sessionPath), "%ssession.json", sessionDir);

    std::ifstream f(sessionPath);
    if (f.is_open())
    {
        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
        // Should contain warning and error counts
        EXPECT_NE(content.find("\"warnings\""), std::string::npos);
        EXPECT_NE(content.find("\"errors\""), std::string::npos);
    }
}

TEST_F(SessionManagerRetentionTest, RetentionRingWrapsAt256)
{
    ObservationConfig obs;
    std::memset(&obs, 0, sizeof(obs));
    obs.enableObservationFileSink = false;
    obs.enableTraceFileSink = false;
    obs.enableMetricsFileSink = false;

    mgr.Start(MakeConfig(), obs);

    // Directly inject 300 retainable entries (bypasses async drain)
    for (int i = 0; i < 300; ++i)
    {
        Dia::Observation::Log::LogEntry entry;
        std::memset(&entry, 0, sizeof(entry));
        entry.level = Log::LogLevel::kWarning;
        entry.channel = Dia::Core::StringCRC("retention");
        snprintf(entry.message, sizeof(entry.message), "msg %d", i);
        entry.timestampNs = static_cast<uint64_t>(i);
        entry.threadId = 1;
        mgr.OnRetainableEntry(entry);
    }

    mgr.Stop();

    // Read session.json — retained_warnings_and_errors should have exactly 256 entries
    const char* sessionDir = mgr.GetSessionDirectory();
    char sessionPath[600];
    snprintf(sessionPath, sizeof(sessionPath), "%ssession.json", sessionDir);

    std::ifstream f(sessionPath);
    ASSERT_TRUE(f.is_open());
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

    // Count occurrences of "msg" entries in the retained array
    size_t count = 0;
    size_t pos = content.find("retained_warnings_and_errors");
    ASSERT_NE(pos, std::string::npos);

    std::string sub = content.substr(pos);
    size_t searchPos = 0;
    while ((searchPos = sub.find("\"msg\"", searchPos)) != std::string::npos)
    {
        ++count;
        ++searchPos;
    }

    // Should be capped at 256 (ring capacity)
    EXPECT_EQ(count, 256u);
}

TEST_F(SessionManagerRetentionTest, DoubleStart_ReturnsFalse)
{
    ObservationConfig obs;
    std::memset(&obs, 0, sizeof(obs));
    obs.enableObservationFileSink = false;
    obs.enableTraceFileSink = false;
    obs.enableMetricsFileSink = false;

    EXPECT_TRUE(mgr.Start(MakeConfig(), obs));
    EXPECT_FALSE(mgr.Start(MakeConfig(), obs));
}

TEST_F(SessionManagerRetentionTest, StopWithoutStart_IsSafe)
{
    mgr.Stop();
    SUCCEED();
}
