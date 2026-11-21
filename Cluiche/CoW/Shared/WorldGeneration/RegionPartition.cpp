// S-1 Regions: region partition, first half. See RegionPartition.h.
#pragma fp_contract(off)

#include <Shared/WorldGeneration/RegionPartition.h>
#include <Shared/World/BiomeRegistry.h>

#include <DiaPathfinding/GridSearch.h>
#include <DiaPathfinding/HexNeighbours.h>
#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

bool IsRegionLand(const Cell& c)
{
    return c.landmass != 0 && c.waterSource != WaterSource::Ocean;
}

std::vector<RegionComponent> FindZoneComponents(const WorldStructure& world, const TerrainRules& rules)
{
    std::vector<RegionComponent> out;
    const int n = world.CellCount();
    if (n == 0 || world.mainLandmass == 0) { return out; }
    const int minCells = rules.kMinRegionCells;
    const HexTopology topo(world.dimensions.width, world.dimensions.height);
    const Cell* cells = world.Cells();
    const Dia::Pathfinding::HexNeighbours graph(topo);
    Dia::Pathfinding::GridSearchScratch scratch;
    std::vector<uint32_t> label(static_cast<size_t>(n), Dia::Pathfinding::kUnreached);

    // Components never span zones, so label each zone's eligible cells separately.
    for (int slot = 0; slot < kRegionZoneCount; ++slot)
    {
        const StratumIndex zone = ZoneFromSlot(slot);
        const uint32_t count = Dia::Pathfinding::ConnectedComponents(graph,
            [&](uint32_t c) {
                return cells[c].stratum == zone && cells[c].landmass == world.mainLandmass && IsRegionLand(cells[c]);
            }, label.data(), nullptr, scratch);
        const size_t first = out.size();
        out.resize(first + count);
        for (size_t k = first; k < out.size(); ++k)
        {
            out[k].zone = zone;
            out[k].landmass = world.mainLandmass;
        }
        // Ascending scan: each component's cells arrive sorted.
        for (int i = 0; i < n; ++i)
        {
            const uint32_t l = label[static_cast<size_t>(i)];
            if (l != Dia::Pathfinding::kUnreached) { out[first + l].cells.push_back(static_cast<uint32_t>(i)); }
        }
    }
    for (RegionComponent& comp : out) { comp.tiny = static_cast<int>(comp.cells.size()) < minCells; }
    // Ascending min cell index across zones, as the single-pass scan produced.
    std::sort(out.begin(), out.end(), [](const RegionComponent& a, const RegionComponent& b) { return a.cells.front() < b.cells.front(); });
    return out;
}

void RebalanceRegionCounts(uint8_t count[kRegionZoneCount], const uint16_t capacity[kRegionZoneCount], StratumIndex& limitedBy)
{
    limitedBy = kNoStratum;
    auto total = [&]() { int t = 0; for (int z = 0; z < kRegionZoneCount; ++z) { t += count[z]; } return t; };

    while (total() < kTotalRegionsMin)
    {
        int best = -1, bestUnused = 0;
        for (int z = 0; z < kRegionZoneCount; ++z)
        {
            if (count[z] >= kZoneCountMax[z] || count[z] >= capacity[z]) { continue; }
            const int unused = static_cast<int>(capacity[z]) - count[z];
            if (best < 0 || unused > bestUnused) { best = z; bestUnused = unused; }
        }
        if (best < 0) { break; }
        ++count[best];
    }
    while (total() > kTotalRegionsMax)
    {
        int best = -1, bestSpare = 0;
        for (int z = 0; z < kRegionZoneCount; ++z)
        {
            if (count[z] <= kZoneCountMin[z]) { continue; }
            const int spare = static_cast<int>(capacity[z]) - count[z];
            if (best < 0 || spare < bestSpare) { best = z; bestSpare = spare; }
        }
        if (best < 0) { break; }
        --count[best];
    }
    if (total() < kTotalRegionsMin)
    {
        // Smallest capacity / range minimum; cross-multiplied, strict less keeps the earliest zone on ties.
        int worst = 0;
        for (int z = 1; z < kRegionZoneCount; ++z)
        {
            if (static_cast<int>(capacity[z]) * kZoneCountMin[worst] < static_cast<int>(capacity[worst]) * kZoneCountMin[z])
            {
                worst = z;
            }
        }
        limitedBy = ZoneFromSlot(worst);
    }
}

RegionTargets ComputeRegionTargets(const WorldStructure& world, const TerrainRules& rules,
                                   const std::vector<RegionComponent>& components, Dia::Maths::RandomStream& rng)
{
    (void)world;
    RegionTargets t;
    const int minCells = rules.kMinRegionCells;
    bool hasBig[kRegionZoneCount] = {};
    for (const RegionComponent& c : components)
    {
        const int z = ZoneSlot(c.zone);
        if (z < 0) { continue; }
        t.zoneCells[z] += static_cast<uint32_t>(c.cells.size());
        if (!c.tiny) { hasBig[z] = true; }
    }
    for (int z = 0; z < kRegionZoneCount; ++z)
    {
        t.capacity[z] = hasBig[z] ? static_cast<uint16_t>(t.zoneCells[z] / static_cast<uint32_t>(minCells)) : 0;
        const uint32_t span = static_cast<uint32_t>(kZoneCountMax[z] - kZoneCountMin[z] + 1);
        const uint32_t draw = static_cast<uint32_t>(rng.RandomInt());   // always drawn
        const uint32_t desired = kZoneCountMin[z] + draw % span;
        t.count[z] = static_cast<uint8_t>(std::min<uint32_t>(desired, t.capacity[z]));
    }
    RebalanceRegionCounts(t.count, t.capacity, t.limitedBy);
    for (int z = 0; z < kRegionZoneCount; ++z) { t.total = static_cast<uint8_t>(t.total + t.count[z]); }
    return t;
}

RegionTargets ComputeRegionTargets(const WorldStructure& world, const TerrainRules& rules, Dia::Maths::RandomStream& rng)
{
    return ComputeRegionTargets(world, rules, FindZoneComponents(world, rules), rng);
}

std::vector<int> AllocateSeedShares(const std::vector<RegionComponent>& components, const RegionTargets& targets, int minRegionCells)
{
    std::vector<int> shares(components.size(), 0);
    for (int z = 0; z < kRegionZoneCount; ++z)
    {
        std::vector<size_t> idx;
        uint64_t bigCells = 0;
        for (size_t i = 0; i < components.size(); ++i)
        {
            if (!components[i].tiny && ZoneSlot(components[i].zone) == z) { idx.push_back(i); bigCells += components[i].cells.size(); }
        }
        if (idx.empty()) { continue; }
        const int nZ = targets.count[z];
        std::vector<double> frac(idx.size());
        int sum = 0;
        // A component can't hold more regions than floor(cells / kMinRegionCells) (min 1); the surplus goes to other components.
        std::vector<int> room(idx.size(), 0x7FFFFFFF);
        if (minRegionCells > 0)
        {
            for (size_t k = 0; k < idx.size(); ++k)
            {
                room[k] = std::max(1, static_cast<int>(components[idx[k]].cells.size() / static_cast<size_t>(minRegionCells)));
            }
        }
        for (size_t k = 0; k < idx.size(); ++k)
        {
            const double q = static_cast<double>(nZ) * static_cast<double>(components[idx[k]].cells.size()) / static_cast<double>(bigCells);
            const int base = std::min(room[k], std::max(1, static_cast<int>(std::floor(q))));
            shares[idx[k]] = base;
            frac[k] = q - std::floor(q);
            sum += base;
        }
        while (sum < nZ)
        {
            size_t best = idx.size();
            for (size_t k = 0; k < idx.size(); ++k)
            {
                if (shares[idx[k]] >= room[k]) { continue; }
                if (best == idx.size() || frac[k] > frac[best]) { best = k; }
            }
            if (best == idx.size()) { break; }   // every component is full: the zone gets fewer regions than targeted
            ++shares[idx[best]];
            ++sum;
            frac[best] = -1.0;   // each component takes a remainder unit once before any takes a second
            bool allUsed = true;
            for (double f : frac) { if (f >= 0.0) { allUsed = false; break; } }
            if (allUsed) { std::fill(frac.begin(), frac.end(), 0.0); }
        }
    }
    return shares;
}

std::vector<uint32_t> PlaceSeeds(const WorldStructure& world, const TerrainRules& rules, const RegionComponent& comp,
                                 int seedCount, Dia::Maths::RandomStream& rng, const World::BiomeRegistry& biomes)
{
    std::vector<uint32_t> seeds;
    if (comp.cells.empty() || seedCount <= 0) { return seeds; }
    const HexTopology topo(world.dimensions.width, world.dimensions.height);
    const Cell* cells = world.Cells();
    const float bonus = rules.kSeedBiomeBonus;
    const size_t size = comp.cells.size();
    seedCount = std::min<int>(seedCount, static_cast<int>(size));

    std::vector<uint32_t> minDist(size, 0xFFFFFFFFu);
    std::vector<uint8_t>  isSeed(size, 0);
    std::vector<uint8_t> seedBiome(biomes.Count(), 0);   // per registry row: already used by a seed

    auto addSeed = [&](size_t k) {
        const uint32_t cell = comp.cells[k];
        seeds.push_back(cell);
        isSeed[k] = 1;
        seedBiome[static_cast<size_t>(cells[cell].biome)] = 1;
        const auto cc = topo.ToCoord(cell);
        for (size_t j = 0; j < size; ++j)
        {
            minDist[j] = std::min(minDist[j], HexTopology::Distance(cc, topo.ToCoord(comp.cells[j])));
        }
    };

    addSeed(static_cast<uint32_t>(rng.RandomInt()) % size);
    while (static_cast<int>(seeds.size()) < seedCount)
    {
        size_t best = size;
        float bestScore = 0.0f;
        for (size_t j = 0; j < size; ++j)
        {
            if (isSeed[j]) { continue; }
            const float score = static_cast<float>(minDist[j]) +
                                (seedBiome[static_cast<size_t>(cells[comp.cells[j]].biome)] ? 0.0f : bonus);
            if (best == size || score > bestScore) { best = j; bestScore = score; }   // strict: ties keep lowest index
        }
        addSeed(best);
    }
    return seeds;
}

namespace
{
    void GrowComponent(const WorldStructure& world, const TerrainRules& rules, const HexTopology& topo, const RegionComponent& comp,
                       const std::vector<uint32_t>& seedCells, RegionId firstId, RegionGrowth& out, std::vector<float>& dist,
                       Dia::Pathfinding::GridSearchScratch& scratch, std::vector<float>& tmpCost, std::vector<uint32_t>& tmpId)
    {
        const Cell* cells = world.Cells();
        const int k = static_cast<int>(seedCells.size());
        const float cap = rules.kMaxRegionShare * (static_cast<float>(comp.cells.size()) / static_cast<float>(k));
        const uint32_t capCells = std::max<uint32_t>(1u, static_cast<uint32_t>(std::floor(cap)));
        const float mismatch = rules.kBiomeMismatch;
        const float ridge = rules.kRegionRidgeCost;

        auto inComp = [&](uint32_t c) {
            return cells[c].landmass == comp.landmass && IsRegionLand(cells[c]) && cells[c].stratum == comp.zone;
        };
        auto stepCost = [&](World::BiomeIndex seedBiome, uint32_t c) {
            float cost = 1.0f;
            if (cells[c].biome != seedBiome) { cost += mismatch; }
            if (cells[c].terrainFlags & TerrainFlag::Ridge) { cost += ridge; }
            return cost;
        };

        std::vector<uint32_t> claimed(static_cast<size_t>(k), 0);
        using namespace Dia::Pathfinding;
        const HexNeighbours graph(topo);
        std::vector<CostSeed<float>> costSeeds;
        costSeeds.reserve(static_cast<size_t>(k));
        for (int i = 0; i < k; ++i)
        {
            costSeeds.push_back({ seedCells[i], 0.0f, static_cast<uint32_t>(firstId + i) });
        }
        DijkstraOptions<float> opts;
        opts.outCost = tmpCost.data();
        opts.outSeedId = tmpId.data();
        opts.pushAll = true;
        // The step cost is that of the SEED region's biome (read via outSeedId[from], valid at settle time) entering `to`.
        auto step = [&](uint32_t from, uint32_t to) -> float {
            if (!inComp(to)) { return -1.0f; }
            return stepCost(out.seeds[tmpId[from] - 1].biome, to);
        };
        // Phase 1 (floor): a region that has claimed `floorCells` is satisfied and idles (its entries are parked), so
        // neighbours still under kMinRegionCells can claim before it takes more. Phase 2: parked entries rejoin the heap
        // and the normal capped multi-source Dijkstra resumes.
        const uint32_t floorCells = std::min<uint32_t>(capCells, static_cast<uint32_t>(std::max(1, rules.kMinRegionCells)));
        MultiSourceDijkstra<float>(graph, costSeeds.data(), static_cast<uint32_t>(k), step, opts, scratch,
            [&](uint32_t, float, uint32_t id) {
                const uint32_t ri = id - static_cast<uint32_t>(firstId);
                if (claimed[ri] >= capCells) { return Settle::Skip; }   // capped: no longer a source
                if (claimed[ri] >= floorCells) { return Settle::Park; }   // satisfied: idle
                ++claimed[ri];
                return Settle::Accept;
            });
        ResumeParked<float>(scratch);
        ContinueDijkstra<float>(graph, step, opts, scratch,
            [&](uint32_t, float, uint32_t id) {
                const uint32_t ri = id - static_cast<uint32_t>(firstId);
                if (claimed[ri] >= capCells) { return Settle::Skip; }
                ++claimed[ri];
                return Settle::Accept;
            });
        for (uint32_t c : comp.cells)
        {
            if (tmpId[c] != kUnreached)
            {
                out.regionOf[c] = static_cast<RegionId>(tmpId[c]);
                dist[c] = tmpCost[c];
            }
        }
        for (int i = 0; i < k; ++i) { out.capClaims[firstId - 1 + i] = static_cast<uint16_t>(claimed[i]); }

        // Leftovers: ascending cell index, cheapest adjacent region (neighbour's path cost + step cost), cap ignored;
        // repeat until a pass changes nothing.
        for (bool changed = true; changed;)
        {
            changed = false;
            for (uint32_t c : comp.cells)
            {
                if (out.regionOf[c] != kNoRegion) { continue; }
                RegionId bestR = kNoRegion;
                float bestCost = 0.0f;
                for (uint32_t d = 0; d < 6; ++d)
                {
                    const uint32_t nb = topo.NeighbourAt(c, d);
                    if (nb == kNoCell || !inComp(nb)) { continue; }
                    const RegionId r = out.regionOf[nb];
                    if (r == kNoRegion) { continue; }
                    const float cost = dist[nb] + stepCost(out.seeds[r - 1].biome, c);
                    if (bestR == kNoRegion || cost < bestCost || (cost == bestCost && r < bestR)) { bestR = r; bestCost = cost; }
                }
                if (bestR != kNoRegion)
                {
                    out.regionOf[c] = bestR;
                    dist[c] = bestCost;
                    ++out.leftoverCells;
                    changed = true;
                }
            }
        }
    }
}

void GrowRegions(const WorldStructure& world, const TerrainRules& rules, const std::vector<std::vector<uint32_t>>& seeds,
                 RegionGrowth& out)
{
    const int n = world.CellCount();
    const Cell* cells = world.Cells();
    const HexTopology topo(world.dimensions.width, world.dimensions.height);
    out.regionOf.assign(static_cast<size_t>(n), kNoRegion);
    out.seeds.clear();
    out.capClaims.clear();
    out.leftoverCells = 0;
    out.overflow = false;
    out.tinyComponents.clear();
    out.unzonedCells.clear();
    std::vector<float> dist(static_cast<size_t>(n), 0.0f);
    Dia::Pathfinding::GridSearchScratch scratch;
    scratch.Reserve(static_cast<uint32_t>(n));
    std::vector<float> tmpCost(static_cast<size_t>(n), 0.0f);
    std::vector<uint32_t> tmpId(static_cast<size_t>(n), 0u);

    for (size_t ci = 0; ci < out.components.size(); ++ci)
    {
        const RegionComponent& comp = out.components[ci];
        if (comp.tiny) { out.tinyComponents.push_back(static_cast<uint32_t>(ci)); }
        if (comp.tiny || ci >= seeds.size() || seeds[ci].empty()) { continue; }
        const std::vector<uint32_t>& sc = seeds[ci];
        if (out.seeds.size() + sc.size() > RegionTable::kMaxRegions) { out.overflow = true; continue; }
        const RegionId firstId = static_cast<RegionId>(out.seeds.size() + 1);
        for (uint32_t cell : sc)
        {
            RegionSeed s;
            s.cell = cell; s.biome = cells[cell].biome; s.zone = comp.zone; s.landmass = comp.landmass;
            s.component = static_cast<int32_t>(ci);
            out.seeds.push_back(s);
            out.capClaims.push_back(0);
        }
        GrowComponent(world, rules, topo, comp, sc, firstId, out, dist, scratch, tmpCost, tmpId);
    }
    out.mainlandRegionCount = static_cast<uint8_t>(out.seeds.size());

    for (int c = 0; c < n; ++c)
    {
        if (world.mainLandmass != 0 && cells[c].landmass == world.mainLandmass && IsRegionLand(cells[c]) &&
            cells[c].stratum == kNoStratum)
        {
            out.unzonedCells.push_back(static_cast<uint32_t>(c));
        }
    }

    // Every other landmass is exactly one region, zone = majority of its cells (ties: lowest zone).
    for (int lm = 1; lm < 256; ++lm)
    {
        if (lm == world.mainLandmass) { continue; }
        uint32_t zoneCount[kRegionZoneCount] = {};
        int first = -1;
        for (int c = 0; c < n; ++c)
        {
            if (cells[c].landmass != lm || !IsRegionLand(cells[c])) { continue; }
            if (first < 0) { first = c; }
            const int z = ZoneSlot(cells[c].stratum);
            if (z >= 0) { ++zoneCount[z]; }
        }
        if (first < 0) { continue; }
        if (out.seeds.size() >= RegionTable::kMaxRegions) { out.overflow = true; continue; }
        int bz = 0;
        for (int z = 1; z < kRegionZoneCount; ++z) { if (zoneCount[z] > zoneCount[bz]) { bz = z; } }
        RegionSeed s;
        s.cell = static_cast<uint32_t>(first); s.biome = cells[first].biome; s.zone = ZoneFromSlot(bz);
        s.landmass = static_cast<uint8_t>(lm); s.isIsland = true;
        out.seeds.push_back(s);
        out.capClaims.push_back(0);
        const RegionId id = static_cast<RegionId>(out.seeds.size());
        for (int c = 0; c < n; ++c)
        {
            if (cells[c].landmass == lm && IsRegionLand(cells[c])) { out.regionOf[c] = id; }
        }
    }
}

RegionGrowth BuildProvisionalRegions(const WorldStructure& world, const TerrainRules& rules, Dia::Maths::RandomStream& rng,
                                     const World::BiomeRegistry& biomes)
{
    RegionGrowth out;
    out.components = FindZoneComponents(world, rules);
    out.targets = ComputeRegionTargets(world, rules, out.components, rng);
    const std::vector<int> shares = AllocateSeedShares(out.components, out.targets, rules.kMinRegionCells);
    std::vector<std::vector<uint32_t>> seeds(out.components.size());
    for (size_t i = 0; i < out.components.size(); ++i)
    {
        if (!out.components[i].tiny) { seeds[i] = PlaceSeeds(world, rules, out.components[i], shares[i], rng, biomes); }
    }
    GrowRegions(world, rules, seeds, out);
    return out;
}

} // namespace WorldGeneration
} // namespace CoW
