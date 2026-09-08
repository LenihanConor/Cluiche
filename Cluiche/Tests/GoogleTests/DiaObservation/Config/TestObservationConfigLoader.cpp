#include <gtest/gtest.h>
#include <DiaObservation/Config/ObservationConfigLoader.h>
#include <DiaObservation/Config/ObservationConfig.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstring>

using namespace Dia::Observation;

TEST(ObservationConfigLoader, NullString_ReturnsFalse)
{
    ObservationConfig config;
    EXPECT_FALSE(ObservationConfigLoader::LoadFromString(nullptr, config));
}

TEST(ObservationConfigLoader, EmptyString_ReturnsFalse)
{
    ObservationConfig config;
    EXPECT_FALSE(ObservationConfigLoader::LoadFromString("", config));
}

TEST(ObservationConfigLoader, InvalidJson_ReturnsFalse)
{
    ObservationConfig config;
    EXPECT_FALSE(ObservationConfigLoader::LoadFromString("{not valid json", config));
}

TEST(ObservationConfigLoader, EmptyObject_ReturnsTrue_DefaultsUnchanged)
{
    ObservationConfig config;
    EXPECT_TRUE(ObservationConfigLoader::LoadFromString("{}", config));
    // Defaults should be preserved
    EXPECT_EQ(config.globalLogLevel, LogLevelConfig::kDefault);
    EXPECT_TRUE(config.enableStdOutSink);
    EXPECT_TRUE(config.enableDebugOutputSink);
}

TEST(ObservationConfigLoader, SchemaFieldIgnored)
{
    const char* json = R"({"schema":"diaobservation/1.0","log_level":"info"})";
    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_EQ(config.globalLogLevel, LogLevelConfig::kInfo);
}

TEST(ObservationConfigLoader, ParseGlobalLogLevel)
{
    const char* json = R"({"log_level": "warning"})";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_EQ(config.globalLogLevel, LogLevelConfig::kWarning);
}

TEST(ObservationConfigLoader, ParseAllLogLevels)
{
    auto testLevel = [](const char* levelStr, LogLevelConfig expected)
    {
        char json[256];
        snprintf(json, sizeof(json), "{\"log_level\":\"%s\"}", levelStr);
        ObservationConfig config;
        ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
        EXPECT_EQ(config.globalLogLevel, expected) << "Failed for level: " << levelStr;
    };

    testLevel("trace", LogLevelConfig::kTrace);
    testLevel("debug", LogLevelConfig::kDebug);
    testLevel("info", LogLevelConfig::kInfo);
    testLevel("warning", LogLevelConfig::kWarning);
    testLevel("error", LogLevelConfig::kError);
}

TEST(ObservationConfigLoader, ParsePerChannelOverrides)
{
    const char* json = R"({
        "log_level": "info",
        "log_channels": {
            "Render": "debug",
            "Physics": "error"
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_EQ(config.channelOverrideCount, 2u);

    bool foundRender = false;
    bool foundPhysics = false;
    for (unsigned int i = 0; i < config.channelOverrideCount; ++i)
    {
        if (config.channelOverrides[i].channel == Dia::Core::StringCRC("Render"))
        {
            EXPECT_EQ(config.channelOverrides[i].level, LogLevelConfig::kDebug);
            foundRender = true;
        }
        if (config.channelOverrides[i].channel == Dia::Core::StringCRC("Physics"))
        {
            EXPECT_EQ(config.channelOverrides[i].level, LogLevelConfig::kError);
            foundPhysics = true;
        }
    }
    EXPECT_TRUE(foundRender);
    EXPECT_TRUE(foundPhysics);
}

TEST(ObservationConfigLoader, ParseSinkEnables)
{
    const char* json = R"({
        "sinks": {
            "stdout": false,
            "debug_output": false,
            "observation_file": false,
            "trace_file": false,
            "metrics_file": false
        },
        "health": {
            "file": false
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_FALSE(config.enableStdOutSink);
    EXPECT_FALSE(config.enableDebugOutputSink);
    EXPECT_FALSE(config.enableObservationFileSink);
    EXPECT_FALSE(config.enableTraceFileSink);
    EXPECT_FALSE(config.enableMetricsFileSink);
    EXPECT_FALSE(config.enableHealthFileSink);
}

TEST(ObservationConfigLoader, DefaultSinksAllEnabled)
{
    const char* json = R"({"log_level": "info"})";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_TRUE(config.enableStdOutSink);
    EXPECT_TRUE(config.enableDebugOutputSink);
    EXPECT_TRUE(config.enableObservationFileSink);
    EXPECT_TRUE(config.enableTraceFileSink);
    EXPECT_TRUE(config.enableMetricsFileSink);
    EXPECT_TRUE(config.enableHealthFileSink);
}

TEST(ObservationConfigLoader, PartialSinkOverride_OthersKeepDefault)
{
    const char* json = R"({
        "sinks": {
            "trace_file": false
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_TRUE(config.enableStdOutSink);
    EXPECT_TRUE(config.enableObservationFileSink);
    EXPECT_FALSE(config.enableTraceFileSink);
    EXPECT_TRUE(config.enableMetricsFileSink);
    EXPECT_TRUE(config.enableHealthFileSink);
}

TEST(ObservationConfigLoader, UnknownLevel_UsesFallback)
{
    const char* json = R"({"log_level": "bogus"})";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_EQ(config.globalLogLevel, LogLevelConfig::kDefault);
}

TEST(ObservationConfigLoader, ParseTraceConfig)
{
    const char* json = R"({
        "trace": {
            "enabled": true,
            "categories": {
                "diaapplicationflow": true,
                "diastream": true
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_TRUE(config.traceEnabled);
    // kDiaApplicationFlow = 1<<0, kDiaStream = 1<<2
    EXPECT_EQ(config.traceCategoryMask, (1u << 0) | (1u << 2));
}

TEST(ObservationConfigLoader, ParseTraceConfig_DisabledClearsCategories)
{
    const char* json = R"({
        "trace": {
            "enabled": false,
            "categories": {
                "all": true
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_FALSE(config.traceEnabled);
    EXPECT_EQ(config.traceCategoryMask, 0u);
}

TEST(ObservationConfigLoader, ParseMetricInterval)
{
    const char* json = R"({
        "metrics": {
            "snapshot_interval_ms": 250
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_EQ(config.metricSnapshotIntervalMs, 250u);
}

TEST(ObservationConfigLoader, ParseHealthInterval)
{
    const char* json = R"({
        "health": {
            "poll_interval_ms": 1000
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_EQ(config.healthPollIntervalMs, 1000u);
}

TEST(ObservationConfigLoader, DefaultIntervals)
{
    const char* json = R"({"log_level": "info"})";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_EQ(config.metricSnapshotIntervalMs, 100u);
    EXPECT_EQ(config.healthPollIntervalMs, 500u);
}
