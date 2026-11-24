#include <gtest/gtest.h>

#include <string.h>

#include <Shared/World/WorldSimNames.h>

namespace CoW {
namespace World {
namespace Testing {

TEST(WorldSimNamesTest, BuildersForWeather)
{
    EXPECT_STREQ("weather.temperature_c", LayerId(kWeather, "temperature_c").c_str());
    EXPECT_STREQ("cow.weather.layers/temperature_c", LayerSchemaKey(kWeather, "temperature_c").c_str());
    EXPECT_STREQ("cow.weather.params/snow.melt_threshold", ParamSchemaKey(kWeather, "snow.melt_threshold").c_str());
    EXPECT_STREQ("cow.weather.params", CommandName(kWeather, "params").c_str());
    EXPECT_STREQ("cow.weather", SaveId(kWeather).c_str());
    EXPECT_STREQ("cow.weather_state", StateFormat(kWeather).c_str());
    EXPECT_STREQ("cow.weather_params", ParamsFormat(kWeather).c_str());
    EXPECT_STREQ("cow.weather_preset", PresetFormat(kWeather).c_str());
    EXPECT_STREQ("Global/Weather/Presets/x.weather_preset.json", PresetPath(kWeather, "x").c_str());
    EXPECT_STREQ("weather", StageId(kWeather).c_str());
    EXPECT_STREQ("cow.weather.step_ms", MetricName(kWeather, "step_ms").c_str());
}

TEST(WorldSimNamesTest, BuildersForHydro)
{
    EXPECT_STREQ("hydro.flow_accumulation", LayerId(kHydro, "flow_accumulation").c_str());
    EXPECT_STREQ("cow.hydro.layers/flow_accumulation", LayerSchemaKey(kHydro, "flow_accumulation").c_str());
    EXPECT_STREQ("cow.hydro.params/k", ParamSchemaKey(kHydro, "k").c_str());
    EXPECT_STREQ("cow.hydro.set_param", CommandName(kHydro, "set_param").c_str());
    EXPECT_STREQ("cow.hydro", SaveId(kHydro).c_str());
    EXPECT_STREQ("cow.hydro_state", StateFormat(kHydro).c_str());
    EXPECT_STREQ("cow.hydro_params", ParamsFormat(kHydro).c_str());
    EXPECT_STREQ("cow.hydro_preset", PresetFormat(kHydro).c_str());
    EXPECT_STREQ("Global/Hydro/Presets/wet.hydro_preset.json", PresetPath(kHydro, "wet").c_str());
    EXPECT_STREQ("hydro", StageId(kHydro).c_str());
    EXPECT_STREQ("cow.hydro.discarded_steps", MetricName(kHydro, "discarded_steps").c_str());
}

TEST(WorldSimNamesTest, StageIdsAndDirectories)
{
    EXPECT_STREQ("weather", StageId(kWeather).c_str());
    EXPECT_STREQ("hydro", StageId(kHydro).c_str());
    EXPECT_STREQ("ecology", StageId(kEcology).c_str());
    EXPECT_STREQ("Global/Ecology/Presets/a.ecology_preset.json", PresetPath(kEcology, "a").c_str());
    EXPECT_STREQ("Global/Worldgen/Presets/a.worldgen_preset.json", PresetPath(kWorldgen, "a").c_str());
    EXPECT_STREQ("Global/Succession/Presets/a.succession_preset.json", PresetPath(kSuccession, "a").c_str());
}

TEST(WorldSimNamesTest, SnakeCaseAndTokens)
{
    EXPECT_TRUE(IsSnakeCase("flow_accumulation"));
    EXPECT_TRUE(IsSnakeCase("temperature_c"));
    EXPECT_FALSE(IsSnakeCase("flowAccumulation"));
    EXPECT_FALSE(IsSnakeCase(""));
    EXPECT_FALSE(IsSnakeCase(nullptr));
    EXPECT_TRUE(IsKnownToken("worldgen"));
    EXPECT_TRUE(IsKnownToken("weather"));
    EXPECT_TRUE(IsKnownToken("hydro"));
    EXPECT_TRUE(IsKnownToken("ecology"));
    EXPECT_TRUE(IsKnownToken("succession"));
    EXPECT_FALSE(IsKnownToken("lava"));
    EXPECT_FALSE(IsKnownToken(nullptr));
}

TEST(WorldSimNamesTest, RejectsCamelCaseField)
{
    EXPECT_STREQ("", LayerId(kHydro, "flowAccumulation").c_str());
    EXPECT_STREQ("", LayerSchemaKey(kHydro, "flowAccumulation").c_str());
}

TEST(WorldSimNamesTest, RejectsUnknownToken)
{
    EXPECT_STREQ("", SaveId("lava").c_str());
    EXPECT_STREQ("", LayerId("lava", "x").c_str());
}

TEST(WorldSimNamesTest, LongInputIsRejectedWithoutOverflow)
{
    char longField[400];
    memset(longField, 'a', sizeof(longField));
    longField[399] = '\0';
    const Name n = LayerId(kWeather, longField);
    EXPECT_EQ(0u, strlen(n.c_str()));
    const Name p = PresetPath(kWeather, longField);
    EXPECT_LT(strlen(p.c_str()), 192u);
}

}  // namespace Testing
}  // namespace World
}  // namespace CoW
