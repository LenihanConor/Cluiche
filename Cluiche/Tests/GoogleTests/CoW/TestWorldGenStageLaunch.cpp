// Launch presets (Stage Catalogue F2) Task 11 / AC 14: WorldGenerationModule::OnStageLaunch applies preset, seed and size
// from a launch preset's override on top of the manifest config.
#include <gtest/gtest.h>
#include <Modules/WorldGen/WorldGenerationModule.h>   // first: RandomStream.h must precede windows.h (min/max macros)
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

using Dia::Core::StringCRC;

namespace {

struct Probe : CoW::WorldGenerationModule
{
    Probe() : CoW::WorldGenerationModule(StringCRC("WorldGenerationModule")) { SetRulesSidecarText(CoW::WorldGeneration::TestSidecarText()); LoadRulesSidecar(); }
    using CoW::WorldGenerationModule::OnConfigure;
    using CoW::WorldGenerationModule::OnStageLaunch;
};

}

TEST(WorldGenStageLaunch, SeedAndSizeApplied)
{
    Probe m;
    m.OnStageLaunch("{\"seed\": 42, \"size\": \"Small\"}");
    EXPECT_EQ(m.GetSession().Seed(), 42u);
    const auto smallDims = Draoi::HexWorld::WorldSizeDimensions(Draoi::HexWorld::WorldSize::Small);
    EXPECT_EQ(m.GetSession().Dimensions().width, smallDims.width);
    EXPECT_EQ(m.GetSession().Dimensions().height, smallDims.height);
    EXPECT_TRUE(m.GetHandoffError().empty());
}

TEST(WorldGenStageLaunch, OverridesManifestConfig)
{
    Probe m;
    m.OnConfigure("{\"seed\": 7, \"size\": \"Large\"}");
    m.OnStageLaunch("{\"seed\": 99}");
    EXPECT_EQ(m.GetSession().Seed(), 99u);
    const auto largeDims = Draoi::HexWorld::WorldSizeDimensions(Draoi::HexWorld::WorldSize::Large);
    EXPECT_EQ(m.GetSession().Dimensions().width, largeDims.width) << "keys the override omits keep the manifest value (shallow merge)";
}

TEST(WorldGenStageLaunch, EmptyOrMalformedOverrideIsNoOp)
{
    Probe m;
    m.OnConfigure("{\"seed\": 7}");
    m.OnStageLaunch("");
    m.OnStageLaunch("not json");
    m.OnStageLaunch("[1,2]");
    EXPECT_EQ(m.GetSession().Seed(), 7u);
    EXPECT_TRUE(m.GetHandoffError().empty());
}

TEST(WorldGenStageLaunch, InvalidSeedAndSizeIgnored)
{
    Probe m;
    m.OnConfigure("{\"seed\": 7}");
    m.OnStageLaunch("{\"seed\": -3, \"size\": \"Huge\"}");
    EXPECT_EQ(m.GetSession().Seed(), 7u);
}

TEST(WorldGenStageLaunch, UnknownPresetRecordsHandoffError)
{
    Probe m;
    m.OnStageLaunch("{\"preset\": \"no_such_preset_xyz\"}");
    EXPECT_FALSE(m.GetHandoffError().empty());
}

TEST(WorldGenStageLaunch, NonStringPresetRecordsHandoffError)
{
    Probe m;
    m.OnStageLaunch("{\"preset\": 5}");
    EXPECT_FALSE(m.GetHandoffError().empty());
}
