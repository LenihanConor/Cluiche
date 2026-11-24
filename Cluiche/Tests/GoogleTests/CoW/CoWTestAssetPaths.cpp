// Supplies the CoW shipped-asset paths to the neutral Draoi biome test helpers (DraoiHexWorld/Testing/BiomeTestRegistry.h)
// and the CoW terrain test config to the neutral terrain test host (DraoiHexWorldTerrain/Testing/TerrainTestHost.h).
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DraoiHexWorldTerrain/Testing/TerrainTestHost.h>
#include <Shared/CoWNodeKinds.h>
#include <Shared/CoWStepSeedNames.h>
#include <Shared/WorldGeneration/CoWTesting/GuaranteeSpecTestHelpers.h>

namespace
{
    const Draoi::HexWorld::GuaranteeSpec* CoWShippedGuaranteeSpec()
    {
        return &CoW::WorldGeneration::Testing::ShippedGuaranteeSpec();
    }

    const bool kCoWTestAssetPathsSet = []()
    {
        Draoi::HexWorld::Testing::TestAssetPaths paths;
        paths.strata        = "Cluiche/Assets/CoW/Global/World/strata.json";
        paths.strataSidecar = "Cluiche/Assets/CoW/Global/World/strata.schema.json";
        paths.biomes        = "Cluiche/Assets/CoW/Global/World/biomes.json";
        paths.biomesExtra   = "Cluiche/Tests/GoogleTests/CoW/Fixtures/biomes_extra.json";
        Draoi::HexWorld::Testing::SetTestAssetPaths(paths);

        Draoi::HexWorld::Testing::TerrainTestConfig terrain;
        terrain.rulesSidecarPath = "Cluiche/Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json";
        terrain.stepSeedNames    = CoW::WorldGeneration::CoWStepSeedNames();
        terrain.nodeKinds        = CoW::WorldGeneration::CoWNodeKindIds();
        terrain.guaranteeSpec    = &CoWShippedGuaranteeSpec;
        Draoi::HexWorld::Testing::SetTerrainTestConfig(terrain);
        return true;
    }();
}
