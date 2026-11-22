// S-1 Regions, Nodes and Site Hook: RegionsAndSites::Place. See RegionsAndSites.h.
#include <Shared/WorldGeneration/RegionsAndSites.h>
#include <DraoiHexWorld/BiomeRegistry.h>

#include <Shared/WorldGeneration/DeriveSubSeed.h>
#include <Shared/WorldGeneration/NodesRiver.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <cstring>
#include <string>

namespace CoW {
namespace WorldGeneration {
namespace RegionsAndSites {

namespace
{
    void Reseed(Dia::Maths::RandomStream& rng, uint64_t attemptSeed, const Dia::Core::StringCRC& step)
    {
        rng.Restore(FoldSeed(DeriveSubSeed(attemptSeed, step)), 0);
    }
}

PlaceResult Place(WorldStructure& world, const SiteRegistry& registry, const TerrainRules& rules, const Draoi::HexWorld::BiomeRegistry& biomes,
                  uint64_t attemptSeed,
                  int attempt, Dia::Maths::RandomStream& wldStream, const StepSeedNames& names, PlaceReport* report)
{
    PlaceReport local;
    PlaceReport& rep = report ? *report : local;
    rep = PlaceReport{};

    world.sites.Clear();
    world.regions = RegionTable{};
    for (int i = 0; i < world.CellCount(); ++i) { world.Cells()[i].region = kNoRegion; }

    // Regions
    Reseed(wldStream, attemptSeed, names.regions);
    rep.regionBuild = BuildRegions(world, rules, wldStream, biomes);

    // Built-in nodes
    Reseed(wldStream, attemptSeed, names.nodeCorridor);
    rep.corridors = PlaceCorridorNodes(world, rules, biomes.Strata(), wldStream, registry.NodeKinds());
    PlaceRiverNodes(world, rules, registry.NodeKinds());   // no RNG: nothing to reseed
    Reseed(wldStream, attemptSeed, names.nodeFill);
    rep.fill = PlaceFillNodes(world, rules, wldStream, registry.NodeKinds());

    rep.limits.regionLimitedBy = world.regions.limitedBy;
    rep.limits.barrierCount = static_cast<uint8_t>(BarrierCountOf(biomes.Strata()));
    for (uint32_t b = 0; b < rep.limits.barrierCount; ++b) { rep.limits.corridorLimited[b] = rep.corridors.corridorLimited[b]; }
    rep.limits.nodeLimited = rep.fill.nodeLimited;

    // Registered kinds, dependencies first
    const auto order = registry.Order();
    for (size_t i = 0; i < order.size(); ++i)
    {
        ISitePlacer* placer = order[i];
        const SiteKindInfo& info = placer->Info();
        rep.ranPlacers.push_back(info.kindCrc);
        Reseed(wldStream, attemptSeed, info.Kind());

        SiteSet produced;
        const PlacementContext ctx{ world, world.dimensions, attempt };
        PlaceResult r = placer->Place(ctx, wldStream, produced);
        if (!r.IsOk())
        {
            const std::string named = std::string(info.id) + ": " + r.reason;
            r = r.code == PlaceResult::Code::Retryable ? PlaceResult::Retryable(named.c_str()) : PlaceResult::TerrainCause(named.c_str());
            rep.failedPlacerIndex = static_cast<int>(i);
            rep.result = r;
            return r;
        }
        for (const Site& s : produced.All()) { world.sites.Add(s); }
    }

    rep.result = PlaceResult::Ok();
    return rep.result;
}

} // namespace RegionsAndSites
} // namespace WorldGeneration
} // namespace CoW
