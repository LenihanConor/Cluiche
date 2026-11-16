#include <gtest/gtest.h>
#include <DiaObservation/Config/ObservationConfigLoader.h>
#include <DiaObservation/Config/ObservationConfigCli.h>
#include <DiaObservation/Config/ObservationConfig.h>

using namespace Dia::Observation;

// ============================================================================
// ObservationConfigLoader: Perf config from JSON
// ============================================================================

TEST(ObservationConfigLoaderPerfConfig, DefaultPerfConfig)
{
    // Empty JSON should preserve default perf config
    const char* json = R"({})";
    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));

    EXPECT_EQ(config.perfConfig.tripFrames, 10u);
    EXPECT_EQ(config.perfConfig.tripFactor, 2.0f);
    EXPECT_EQ(config.perfConfig.rearmFrames, 30u);
    EXPECT_EQ(config.perfConfig.minIntervalMs, 5000u);
    EXPECT_EQ(config.perfConfig.budgetScale, 1.0f);
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 0u);
    EXPECT_FALSE(config.perfConfig.perfEnabled);
}

TEST(ObservationConfigLoaderPerfConfig, ParsePerfBlockAllFields)
{
    const char* json = R"({
        "observation": {
            "perf": {
                "trip_frames": 15,
                "trip_factor": 2.5,
                "rearm_frames": 40,
                "min_interval_ms": 3000,
                "budget_scale": 1.5,
                "perf_exit_seconds": 60
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));

    EXPECT_EQ(config.perfConfig.tripFrames, 15u);
    EXPECT_EQ(config.perfConfig.tripFactor, 2.5f);
    EXPECT_EQ(config.perfConfig.rearmFrames, 40u);
    EXPECT_EQ(config.perfConfig.minIntervalMs, 3000u);
    EXPECT_EQ(config.perfConfig.budgetScale, 1.5f);
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 60u);
}

TEST(ObservationConfigLoaderPerfConfig, ParsePerfBlockPartial)
{
    const char* json = R"({
        "observation": {
            "perf": {
                "trip_frames": 20,
                "budget_scale": 0.8
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));

    // Specified fields should be updated
    EXPECT_EQ(config.perfConfig.tripFrames, 20u);
    EXPECT_EQ(config.perfConfig.budgetScale, 0.8f);

    // Unspecified fields should retain defaults
    EXPECT_EQ(config.perfConfig.tripFactor, 2.0f);
    EXPECT_EQ(config.perfConfig.rearmFrames, 30u);
    EXPECT_EQ(config.perfConfig.minIntervalMs, 5000u);
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 0u);
}

TEST(ObservationConfigLoaderPerfConfig, InvalidTripFrames_Zero_FallsBackToDefault)
{
    const char* json = R"({
        "observation": {
            "perf": {
                "trip_frames": 0
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));

    // Invalid (0) should fall back to default
    EXPECT_EQ(config.perfConfig.tripFrames, 10u);
}

TEST(ObservationConfigLoaderPerfConfig, InvalidTripFactor_Negative_FallsBackToDefault)
{
    const char* json = R"({
        "observation": {
            "perf": {
                "trip_factor": -1.0
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));

    // Invalid (negative) should fall back to default
    EXPECT_EQ(config.perfConfig.tripFactor, 2.0f);
}

TEST(ObservationConfigLoaderPerfConfig, InvalidBudgetScale_Negative_FallsBackToDefault)
{
    const char* json = R"({
        "observation": {
            "perf": {
                "budget_scale": -0.5
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));

    // Invalid (<= 0) should fall back to default
    EXPECT_EQ(config.perfConfig.budgetScale, 1.0f);
}

TEST(ObservationConfigLoaderPerfConfig, InvalidBudgetScale_Zero_FallsBackToDefault)
{
    const char* json = R"({
        "observation": {
            "perf": {
                "budget_scale": 0.0
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));

    // Invalid (zero) should fall back to default
    EXPECT_EQ(config.perfConfig.budgetScale, 1.0f);
}

TEST(ObservationConfigLoaderPerfConfig, InvalidRearmFrames_Zero_FallsBackToDefault)
{
    const char* json = R"({
        "observation": {
            "perf": {
                "rearm_frames": 0
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));

    // Invalid (0) should fall back to default
    EXPECT_EQ(config.perfConfig.rearmFrames, 30u);
}

TEST(ObservationConfigLoaderPerfConfig, InvalidMinIntervalMs_Zero_FallsBackToDefault)
{
    const char* json = R"({
        "observation": {
            "perf": {
                "min_interval_ms": 0
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));

    // Invalid (0) should fall back to default
    EXPECT_EQ(config.perfConfig.minIntervalMs, 5000u);
}

TEST(ObservationConfigLoaderPerfConfig, MissingPerfBlock_PreservesDefaults)
{
    const char* json = R"({
        "log_level": "info",
        "profile": {
            "enabled": true
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));

    // All perf config fields should be at defaults
    EXPECT_EQ(config.perfConfig.tripFrames, 10u);
    EXPECT_EQ(config.perfConfig.tripFactor, 2.0f);
    EXPECT_EQ(config.perfConfig.rearmFrames, 30u);
    EXPECT_EQ(config.perfConfig.minIntervalMs, 5000u);
    EXPECT_EQ(config.perfConfig.budgetScale, 1.0f);
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 0u);
}

// ============================================================================
// ObservationConfigCli: --perf and --perf=<seconds> flags
// ============================================================================

TEST(ObservationConfigCliPerfConfig, BarePerf_EnablesProfileAndTrips)
{
    const char* argv[] = { "program", "--perf" };
    ObservationConfig config;

    ObservationConfigCli::ApplyOverrides(2, argv, config);

    // --perf should enable profiling with diaapplicationflow category
    EXPECT_TRUE(config.profileEnabled);
    EXPECT_EQ(config.profileCategoryMask & (1u << 0), (1u << 0)); // diaapplicationflow bit
    EXPECT_TRUE(config.perfConfig.perfEnabled);
    // perfExitSeconds should remain at default (0)
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 0u);
}

TEST(ObservationConfigCliPerfConfig, PerfWithSeconds_SetsExitSeconds)
{
    const char* argv[] = { "program", "--perf=20" };
    ObservationConfig config;

    ObservationConfigCli::ApplyOverrides(2, argv, config);

    EXPECT_TRUE(config.profileEnabled);
    EXPECT_EQ(config.profileCategoryMask & (1u << 0), (1u << 0)); // diaapplicationflow bit
    EXPECT_TRUE(config.perfConfig.perfEnabled);
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 20u);
}

TEST(ObservationConfigCliPerfConfig, PerfWithZeroSeconds)
{
    const char* argv[] = { "program", "--perf=0" };
    ObservationConfig config;

    ObservationConfigCli::ApplyOverrides(2, argv, config);

    EXPECT_TRUE(config.perfConfig.perfEnabled);
    EXPECT_TRUE(config.profileEnabled);
    EXPECT_EQ(config.profileCategoryMask & (1u << 0), (1u << 0));
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 0u);
}

TEST(ObservationConfigCliPerfConfig, PerfWithLargeSeconds)
{
    const char* argv[] = { "program", "--perf=3600" };
    ObservationConfig config;

    ObservationConfigCli::ApplyOverrides(2, argv, config);

    EXPECT_TRUE(config.profileEnabled);
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 3600u);
}

TEST(ObservationConfigCliPerfConfig, PerfWithNonDigits_Ignored)
{
    const char* argv[] = { "program", "--perf=abc" };
    ObservationConfig config;
    // Set some non-default values to verify they don't change
    config.profileEnabled = true;
    config.perfConfig.perfExitSeconds = 100;

    ObservationConfigCli::ApplyOverrides(2, argv, config);

    // Invalid argument should be silently ignored, not crash
    // (config state unchanged from before call)
    EXPECT_TRUE(config.profileEnabled);
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 100u);
}

TEST(ObservationConfigCliPerfConfig, PerfEqualsEmpty_Ignored)
{
    const char* argv[] = { "program", "--perf=" };
    ObservationConfig config;
    config.profileEnabled = false;
    config.perfConfig.perfExitSeconds = 0;

    ObservationConfigCli::ApplyOverrides(2, argv, config);

    // Empty value should be silently ignored
    EXPECT_FALSE(config.profileEnabled);
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 0u);
}

TEST(ObservationConfigCliPerfConfig, MultiplePerf_LastWins)
{
    const char* argv[] = { "program", "--perf=10", "--perf=20" };
    ObservationConfig config;

    ObservationConfigCli::ApplyOverrides(3, argv, config);

    EXPECT_TRUE(config.profileEnabled);
    // The second --perf=20 should win
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 20u);
}

TEST(ObservationConfigCliPerfConfig, PerfAndOtherFlags)
{
    const char* argv[] = { "program", "--log-level=warning", "--perf=30", "--log-channel=Render:debug" };
    ObservationConfig config;

    ObservationConfigCli::ApplyOverrides(4, argv, config);

    // --perf should work alongside other flags
    EXPECT_TRUE(config.profileEnabled);
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 30u);
    EXPECT_EQ(config.globalLogLevel, LogLevelConfig::kWarning);
}

// ============================================================================
// Integration: JSON + CLI override
// ============================================================================

TEST(ObservationConfigPerfConfigIntegration, JsonThenCliOverride)
{
    // Load from JSON first
    const char* json = R"({
        "observation": {
            "perf": {
                "trip_frames": 15
            }
        }
    })";

    ObservationConfig config;
    ASSERT_TRUE(ObservationConfigLoader::LoadFromString(json, config));

    // Then apply CLI override
    const char* argv[] = { "program", "--perf=50" };
    ObservationConfigCli::ApplyOverrides(2, argv, config);

    // CLI should enable profiling and set exit seconds
    EXPECT_TRUE(config.profileEnabled);
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 50u);
    // JSON field that wasn't overridden should be preserved
    EXPECT_EQ(config.perfConfig.tripFrames, 15u);
}

// ============================================================================
// Edge cases and robustness
// ============================================================================

TEST(ObservationConfigCliPerfConfig, PerfWithLeadingZeros)
{
    const char* argv[] = { "program", "--perf=0100" };
    ObservationConfig config;

    ObservationConfigCli::ApplyOverrides(2, argv, config);

    EXPECT_TRUE(config.profileEnabled);
    // Should parse as 100 (leading zeros ignored in decimal)
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 100u);
}

TEST(ObservationConfigCliPerfConfig, PerfWithTrailingWhitespace_Invalid)
{
    const char* argv[] = { "program", "--perf=20 " };
    ObservationConfig config;
    config.profileEnabled = false;
    config.perfConfig.perfExitSeconds = 0;

    ObservationConfigCli::ApplyOverrides(2, argv, config);

    // Trailing space makes it invalid (non-digit)
    // Should be silently ignored
    EXPECT_FALSE(config.profileEnabled);
    EXPECT_EQ(config.perfConfig.perfExitSeconds, 0u);
}
