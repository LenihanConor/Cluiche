// Supplies the CoW shipped-asset paths to the neutral Draoi biome test helpers (DraoiHexWorld/Testing/BiomeTestRegistry.h).
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>

namespace
{
    const bool kCoWTestAssetPathsSet = []()
    {
        Draoi::HexWorld::Testing::TestAssetPaths paths;
        paths.strata        = "Cluiche/Assets/CoW/Global/World/strata.json";
        paths.strataSidecar = "Cluiche/Assets/CoW/Global/World/strata.schema.json";
        paths.biomes        = "Cluiche/Assets/CoW/Global/World/biomes.json";
        paths.biomesExtra   = "Cluiche/Tests/GoogleTests/CoW/Fixtures/biomes_extra.json";
        Draoi::HexWorld::Testing::SetTestAssetPaths(paths);
        return true;
    }();
}
