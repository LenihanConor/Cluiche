// The neutral terrain test host (DraoiHexWorldTerrain/Testing/TerrainTestHost.h): config injected by the host
// application (CoWTestAssetPaths.cpp sets it at static init), abort-with-message when unset.
#include <gtest/gtest.h>

#include <DraoiHexWorldTerrain/Testing/TerrainTestHost.h>

using namespace Draoi::HexWorld::Testing;

TEST(TerrainTestHost, ConfigSetByHostLoadsRules)
{
    EXPECT_FALSE(TestSidecarText().empty());
    const Draoi::HexWorld::TerrainRules rules = TestRules();
    (void)rules;
    TestSession session;
    (void)session;
}

TEST(TerrainTestHostDeathTest, UnsetConfigAbortsWithMessage)
{
    EXPECT_DEATH(
        {
            ResetTerrainTestConfigForTest();
            (void)TestSidecarText();
        },
        "TerrainTestHost: terrain test config not set");
}
