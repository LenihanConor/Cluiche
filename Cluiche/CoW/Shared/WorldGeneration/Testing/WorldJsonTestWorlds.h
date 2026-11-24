// CoW half of the S-1 JSON test worlds (header-only). The neutral worlds (MakeTinyWorld, GenerateWorld, MakeCell, ...) live in
// DraoiHexWorldTerrain/Testing/WorldJsonTestWorlds.h and are reachable from this namespace via using-directives.
#pragma once

#include <Shared/CoWNodeKinds.h>

#include <DraoiHexWorldTerrain/Testing/WorldJsonTestWorlds.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

namespace CoW {
namespace WorldGeneration {
namespace Testing {
using namespace Draoi::HexWorld;
using namespace Draoi::HexWorld::Testing;

// A site registry with no placers that carries CoW's node kind ids (a JSON context without a registry knows only the neutral ids).
inline const SiteRegistry& CoWTestSiteRegistry()
{
    static const CoWSiteRegistry registry;
    return registry;
}

// An import context that carries the shipped biome registry (import requires one).
inline WorldJsonContext TestImportContext()
{
    WorldJsonContext c;
    c.registry = &CoWTestSiteRegistry();
    c.biomes = &::Draoi::HexWorld::Testing::ShippedBiomeRegistry();
    return c;
}

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
