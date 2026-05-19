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

TEST(ObservationConfigLoader, MissingConfigBlock_ReturnsFalse)
{
    ObservationConfig config;
    EXPECT_FALSE(ObservationConfigLoader::LoadFromString("{\"other\": 1}", config));
}

TEST(ObservationConfigLoader, MissingObservationBlock_ReturnsFalse)
{
    ObservationConfig config;
    EXPECT_FALSE(ObservationConfigLoader::LoadFromString(
        "{\"config\": {\"other\": 1}}", config));
}

TEST(ObservationConfigLoader, ParseGlobalLogLevel)
{
    const char* json = R"({
        "config": {
            "observation": {
                "log_level": "warning"
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_EQ(config.globalLogLevel, LogLevelConfig::kWarning);
}

TEST(ObservationConfigLoader, ParseAllLogLevels)
{
    auto testLevel = [](const char* levelStr, LogLevelConfig expected)
    {
        char json[256];
        snprintf(json, sizeof(json),
            "{\"config\":{\"observation\":{\"log_level\":\"%s\"}}}", levelStr);
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
        "config": {
            "observation": {
                "log_level": "info",
                "log_channels": {
                    "Render": "debug",
                    "Physics": "error"
                }
            }
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
        "config": {
            "observation": {
                "sinks": {
                    "stdout": false,
                    "debug_output": false,
                    "observation_file": false,
                    "trace_file": false,
                    "metrics_file": false
                }
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_FALSE(config.enableStdOutSink);
    EXPECT_FALSE(config.enableDebugOutputSink);
    EXPECT_FALSE(config.enableObservationFileSink);
    EXPECT_FALSE(config.enableTraceFileSink);
    EXPECT_FALSE(config.enableMetricsFileSink);
}

TEST(ObservationConfigLoader, DefaultSinksAllEnabled)
{
    const char* json = R"({
        "config": {
            "observation": {
                "log_level": "info"
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_TRUE(config.enableStdOutSink);
    EXPECT_TRUE(config.enableDebugOutputSink);
    EXPECT_TRUE(config.enableObservationFileSink);
    EXPECT_TRUE(config.enableTraceFileSink);
    EXPECT_TRUE(config.enableMetricsFileSink);
}

TEST(ObservationConfigLoader, PartialSinkOverride_OthersKeepDefault)
{
    const char* json = R"({
        "config": {
            "observation": {
                "sinks": {
                    "trace_file": false
                }
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_TRUE(config.enableStdOutSink);
    EXPECT_TRUE(config.enableObservationFileSink);
    EXPECT_FALSE(config.enableTraceFileSink);
    EXPECT_TRUE(config.enableMetricsFileSink);
}

TEST(ObservationConfigLoader, UnknownLevel_UsesFallback)
{
    const char* json = R"({
        "config": {
            "observation": {
                "log_level": "bogus"
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));
    EXPECT_EQ(config.globalLogLevel, LogLevelConfig::kDefault);
}
