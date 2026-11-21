// S-1 Regions, Nodes and Site Hook: RegionsAndSites::Place, the whole placement pass.
//
// Reads terrain and biome data; writes Cell::region, WorldStructure::regions and WorldStructure::sites only, so every
// attempt starts from the same terrain and biomes. Prior regions and sites are discarded first.
// Order: Regions -> built-in nodes (corridors, rivers, hunting/resource fill) -> registered placers in
// SiteRegistry::Order(). Registered kinds therefore never change regions or built-in nodes.
//
// Determinism: the caller passes the attempt seed (= DeriveSubSeed(outer, attempt)). Each step reseeds the stream with
// FoldSeed(DeriveSubSeed(attemptSeed, stepId)) and draw count 0 (Restore). Step ids: "CoW.Regions",
// "CoW.Node.Corridor", "CoW.Node.River" (reseeded for symmetry; river nodes draw nothing), "CoW.Node.Fill", then each
// registered kind id. A step output depends only on the outer seed, the attempt, its own code and what it reads.
//
// Failure: a placer returning Retryable / TerrainCause ends the pass; Place returns that result (reason prefixed with
// the kind id), the failing placer is recorded in the report, later placers do not run and the failed placer's
// partial sites are discarded.
//
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/regions-and-sites.md
#pragma once

#include <Shared/WorldGeneration/LayoutCheck.h>
#include <Shared/WorldGeneration/NodesCorridor.h>
#include <Shared/WorldGeneration/NodesFill.h>
#include <Shared/WorldGeneration/PlaceResult.h>
#include <Shared/WorldGeneration/RegionRepair.h>
#include <Shared/WorldGeneration/SiteRegistry.h>
#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <DiaMaths/Core/RandomStream.h>

#include <cstdint>
#include <vector>

namespace CoW {
namespace WorldGeneration {
namespace RegionsAndSites {

struct PlaceReport
{
    LayoutLimits          limits;                    // what the layout check must tolerate for this world
    RegionBuild           regionBuild;
    CorridorResult        corridors;
    FillResult            fill;
    std::vector<uint32_t> ranPlacers;                // kind ids of registered placers that ran, in order (incl. the failed one)
    int                   failedPlacerIndex = -1;    // index into SiteRegistry::Order(); -1 when none failed
    PlaceResult           result;
};

// attemptSeed = DeriveSubSeed(outerSeed, attempt); attempt is passed on to placers via PlacementContext.
PlaceResult Place(WorldStructure& world, const SiteRegistry& registry, const TerrainRules& rules, const World::BiomeRegistry& biomes,
                  uint64_t attemptSeed,
                  int attempt, Dia::Maths::RandomStream& wldStream, PlaceReport* report = nullptr);

} // namespace RegionsAndSites
} // namespace WorldGeneration
} // namespace CoW
