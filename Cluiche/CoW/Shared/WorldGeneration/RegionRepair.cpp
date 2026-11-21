// S-1 Regions: second half. See RegionRepair.h.
#pragma fp_contract(off)

#include <Shared/WorldGeneration/RegionRepair.h>

#include <Shared/World/BiomeRegistry.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cstring>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

namespace
{
    constexpr unsigned kMaxId = RegionTable::kMaxRegions;   // ids are 1..kMaxId

    struct Info
    {
        DepthZone zone     = DepthZone::None;
        uint8_t   landmass = 0;
        bool      island   = false;
        bool      alive    = false;
    };

    struct Repair
    {
        const WorldStructure& world;
        const Cell*           cells;
        HexTopology           topo;
        int                   n;
        std::vector<RegionId> reg;
        Info                  info[kMaxId + 1];
        unsigned              idCount = 0;   // highest id in use

        Repair(const WorldStructure& w) : world(w), cells(w.Cells()), topo(w.dimensions.width, w.dimensions.height), n(w.CellCount()) {}

        bool MainLand(int c) const { return world.mainLandmass != 0 && cells[c].landmass == world.mainLandmass && IsRegionLand(cells[c]); }

        // Connected groups of cells with the same non-zero key, ascending min cell index.
        template <typename KeyFn>
        std::vector<std::vector<uint32_t>> Pieces(KeyFn key) const
        {
            std::vector<std::vector<uint32_t>> out;
            std::vector<uint8_t> seen(static_cast<size_t>(n), 0);
            for (int s = 0; s < n; ++s)
            {
                const uint32_t k = key(s);
                if (seen[s] || k == 0) { continue; }
                std::vector<uint32_t> piece;
                std::vector<uint32_t> stack{ static_cast<uint32_t>(s) };
                seen[s] = 1;
                while (!stack.empty())
                {
                    const uint32_t c = stack.back(); stack.pop_back();
                    piece.push_back(c);
                    for (uint32_t d = 0; d < 6; ++d)
                    {
                        const uint32_t nb = topo.NeighbourAt(c, d);
                        if (nb == kNoCell || seen[nb] || key(static_cast<int>(nb)) != k) { continue; }
                        seen[nb] = 1; stack.push_back(nb);
                    }
                }
                std::sort(piece.begin(), piece.end());
                out.push_back(std::move(piece));
            }
            return out;
        }

        // Adjacent live non-island region with the longest border to `piece` (edges between a piece cell and a cell
        // of that region), preferring `prefZone` regions, ties lowest id. `ownId` and region 0 are excluded.
        // Only regions on the same landmass as the piece are candidates. 0 = none.
        RegionId BestNeighbour(const std::vector<uint32_t>& piece, RegionId ownId, DepthZone prefZone) const
        {
            uint32_t border[kMaxId + 1] = {};
            for (uint32_t c : piece)
            {
                for (uint32_t d = 0; d < 6; ++d)
                {
                    const uint32_t nb = topo.NeighbourAt(c, d);
                    if (nb == kNoCell) { continue; }
                    const RegionId r = reg[nb];
                    if (r == kNoRegion || r == ownId || !info[r].alive || info[r].island) { continue; }
                    if (info[r].landmass != cells[c].landmass) { continue; }
                    ++border[r];
                }
            }
            RegionId bestSame = 0, bestAny = 0;
            for (unsigned r = 1; r <= idCount; ++r)
            {
                if (border[r] == 0) { continue; }
                if (info[r].zone == prefZone && (bestSame == 0 || border[r] > border[bestSame])) { bestSame = static_cast<RegionId>(r); }
                if (bestAny == 0 || border[r] > border[bestAny]) { bestAny = static_cast<RegionId>(r); }
            }
            return bestSame != 0 ? bestSame : bestAny;
        }

        DepthZone MajorityZone(const std::vector<uint32_t>& piece) const
        {
            uint32_t count[4] = {};
            for (uint32_t c : piece) { ++count[static_cast<int>(cells[c].depthZone)]; }
            int best = 0;
            for (int z = 1; z < 4; ++z) { if (count[z] > count[best]) { best = z; } }
            return static_cast<DepthZone>(best);
        }

        void Assign(const std::vector<uint32_t>& piece, RegionId id) { for (uint32_t c : piece) { reg[c] = id; } }
    };
}

bool RecomputeRegionRecords(WorldStructure& world, const World::BiomeRegistry& biomes)
{
    const int n = world.CellCount();
    const unsigned count = world.regions.regions.Size();
    world.regions.mainlandRegionCount = 0;
    HexTopology topo(world.dimensions.width, world.dimensions.height);
    const Cell* cells = world.Cells();

    const unsigned biomeCount = biomes.Count();
    const World::BiomeIndex oceanRow = biomes.Role(World::BiomeRole::Ocean);
    const World::BiomeIndex lakeRow  = biomes.Role(World::BiomeRole::Lake);
    struct Stats
    {
        uint32_t count = 0;
        double   sx = 0.0, sy = 0.0;
    };
    std::vector<Stats> stats(count + 1);
    std::vector<uint32_t> biomeHist(static_cast<size_t>(count + 1) * biomeCount, 0u);   // [region][biome row]
    auto centre = [&](int c) {
        const auto p = HexTopology::CellCenter(topo.ToCoord(static_cast<uint32_t>(c)), 1.0f);
        return std::pair<double, double>(static_cast<double>(p.x), static_cast<double>(p.y));
    };
    for (int c = 0; c < n; ++c)
    {
        const RegionId r = cells[c].region;
        if (r == kNoRegion || r > count) { continue; }
        Stats& s = stats[r];
        ++s.count;
        const size_t row = static_cast<size_t>(cells[c].biome);
        if (row < biomeCount) { ++biomeHist[static_cast<size_t>(r) * biomeCount + row]; }
        const auto p = centre(c);
        s.sx += p.first; s.sy += p.second;
    }

    // Nearest cell to the centroid, preferring non-Lake cells; ties (within 1e-4) keep the lowest index.
    std::vector<uint32_t> centreCell(count + 1, 0);
    std::vector<double>   centreDist(count + 1, 0.0);
    std::vector<uint8_t>  centreLake(count + 1, 2);
    for (int c = 0; c < n; ++c)
    {
        const RegionId r = cells[c].region;
        if (r == kNoRegion || r > count) { continue; }
        const Stats& s = stats[r];
        const auto p = centre(c);
        const double dx = p.first - s.sx / s.count, dy = p.second - s.sy / s.count;
        const double d = dx * dx + dy * dy;
        const uint8_t lake = cells[c].waterSource == WaterSource::Lake ? 1 : 0;
        if (centreLake[r] == 2 || lake < centreLake[r] || (lake == centreLake[r] && d < centreDist[r] - 1e-4))   // epsilon: float32 cell centres must not break index ties
        {
            centreLake[r] = lake; centreDist[r] = d; centreCell[r] = static_cast<uint32_t>(c);
        }
    }

    // Edge adjacency straight from the cells.
    static_assert(kMaxId <= 64, "adjacency bitmask is 64 bits");
    std::vector<uint64_t> adj(count + 1, 0);
    for (int c = 0; c < n; ++c)
    {
        const RegionId r = cells[c].region;
        if (r == kNoRegion || r > count) { continue; }
        for (uint32_t d = 0; d < 6; ++d)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
            if (nb == kNoCell) { continue; }
            const RegionId o = cells[nb].region;
            if (o != kNoRegion && o <= count && o != r) { adj[r] |= 1ull << (o - 1); }
        }
    }

    bool neighbourOverflow = false;
    unsigned mainland = 0;
    for (unsigned id = 1; id <= count; ++id)
    {
        const Stats& s = stats[id];
        RegionRecord& rec = world.regions.regions[id - 1];
        if (s.count == 0)
        {
            rec.primaryBiome = static_cast<Biome>(oceanRow);   // interim bridge: Task 23 makes this a BiomeIndex
            rec.purity = 0.0f;
            rec.cellCount = 0;
            rec.centreCell = 0;
        }
        else
        {
            const uint32_t* hist = &biomeHist[static_cast<size_t>(id) * biomeCount];
            int best = -1;
            for (int b = 0; b < static_cast<int>(biomeCount); ++b)
            {
                if (b == static_cast<int>(lakeRow) || hist[b] == 0) { continue; }
                if (best < 0 || hist[b] > hist[best]) { best = b; }
            }
            if (best < 0) { best = static_cast<int>(lakeRow); }   // all-Lake region
            rec.primaryBiome = static_cast<Biome>(best);   // interim bridge: Task 23 makes this a BiomeIndex
            rec.purity = static_cast<float>(hist[best]) / static_cast<float>(s.count);
            rec.cellCount = static_cast<uint16_t>(std::min<uint32_t>(s.count, 0xFFFFu));   // saturates
            rec.centreCell = centreCell[id];
        }
        rec.neighbours = decltype(rec.neighbours){};
        unsigned listed = 0;
        for (unsigned o = 1; o <= count; ++o)
        {
            if (!(adj[id] & (1ull << (o - 1)))) { continue; }
            if (listed == RegionRecord::kMaxNeighbours) { neighbourOverflow = true; break; }
            rec.neighbours.Add(static_cast<RegionId>(o));
            ++listed;
        }
        if (!rec.isIsland) { ++mainland; }
    }
    world.regions.mainlandRegionCount = static_cast<uint8_t>(mainland);
    return neighbourOverflow;
}

RegionBuild FinishRegions(WorldStructure& world, const TerrainRules& rules, const RegionGrowth& growth, const World::BiomeRegistry& biomes)
{
    RegionBuild out;
    const int n = world.CellCount();
    world.regions = RegionTable{};
    Cell* cells = world.Cells();
    for (int c = 0; c < n; ++c) { cells[c].region = kNoRegion; }
    if (n == 0 || growth.regionOf.size() != static_cast<size_t>(n)) { return out; }

    const int minCells = rules.kMinRegionCells;
    Repair R(world);
    R.reg = growth.regionOf;
    R.idCount = static_cast<unsigned>(std::min<size_t>(growth.seeds.size(), kMaxId));
    out.overflow = growth.overflow;
    for (unsigned i = 0; i < R.idCount; ++i)
    {
        const RegionSeed& s = growth.seeds[i];
        R.info[i + 1] = { s.zone, s.landmass, s.isIsland, true };
    }
    for (int c = 0; c < n; ++c)   // defensive: ids without a seed cannot exist
    {
        if (R.reg[c] > R.idCount) { R.reg[c] = kNoRegion; }
    }

    // 1. Split pieces.
    {
        const auto pieces = R.Pieces([&](int c) -> uint32_t {
            const RegionId r = R.reg[c];
            return (r != kNoRegion && !R.info[r].island) ? r : 0u;
        });
        std::vector<std::vector<size_t>> byRegion(R.idCount + 1);
        for (size_t i = 0; i < pieces.size(); ++i) { byRegion[R.reg[pieces[i][0]]].push_back(i); }
        for (unsigned r = 1; r <= R.idCount; ++r)
        {
            if (byRegion[r].size() < 2) { continue; }
            size_t keep = byRegion[r][0];
            for (size_t i : byRegion[r]) { if (pieces[i].size() > pieces[keep].size()) { keep = i; } }
            for (size_t i : byRegion[r])
            {
                if (i == keep) { continue; }
                const RegionId to = R.BestNeighbour(pieces[i], static_cast<RegionId>(r), R.info[r].zone);
                R.Assign(pieces[i], to);   // 0 (no neighbour) falls through to the loose-land pass
                ++out.splitPiecesMoved;
            }
        }
    }

    // 2. Loose land (region 0 on main-landmass land): same-zone connected pieces, repeated until nothing moves.
    {
        auto pieces = R.Pieces([&](int c) -> uint32_t {
            return (R.reg[c] == kNoRegion && R.MainLand(c)) ? 1u + static_cast<uint32_t>(cells[c].depthZone) : 0u;
        });
        std::vector<uint8_t> done(pieces.size(), 0);
        for (bool progress = true; progress;)
        {
            progress = false;
            for (size_t i = 0; i < pieces.size(); ++i)
            {
                if (done[i]) { continue; }
                const RegionId to = R.BestNeighbour(pieces[i], kNoRegion, cells[pieces[i][0]].depthZone);
                if (to == 0) { continue; }
                R.Assign(pieces[i], to);
                done[i] = 1; progress = true;
                ++out.looseLandPiecesAbsorbed;
            }
        }
        // 3. Whatever is left is isolated from every region: one orphan region per connected group.
        const auto orphans = R.Pieces([&](int c) -> uint32_t { return (R.reg[c] == kNoRegion && R.MainLand(c)) ? 1u : 0u; });
        for (const auto& piece : orphans)
        {
            if (R.idCount >= kMaxId)
            {
                out.overflow = true;
                out.unassignedLandCells += static_cast<uint32_t>(piece.size());
                continue;
            }
            const RegionId id = static_cast<RegionId>(++R.idCount);
            R.info[id] = { R.MajorityZone(piece), world.mainLandmass, false, true };
            R.Assign(piece, id);
            ++out.orphanRegions;
        }
    }

    // 4. Undersized merges, smallest first.
    {
        bool stuck[kMaxId + 1] = {};
        for (;;)
        {
            uint32_t size[kMaxId + 1] = {};
            for (int c = 0; c < n; ++c) { ++size[R.reg[c]]; }
            unsigned pick = 0;
            for (unsigned r = 1; r <= R.idCount; ++r)
            {
                if (!R.info[r].alive || R.info[r].island || stuck[r] || size[r] == 0 || static_cast<int>(size[r]) >= minCells) { continue; }
                if (pick == 0 || size[r] < size[pick]) { pick = r; }
            }
            if (pick == 0) { break; }
            std::vector<uint32_t> piece;
            for (int c = 0; c < n; ++c) { if (R.reg[c] == pick) { piece.push_back(static_cast<uint32_t>(c)); } }
            const RegionId to = R.BestNeighbour(piece, static_cast<RegionId>(pick), R.info[pick].zone);
            if (to == 0) { stuck[pick] = true; continue; }
            R.Assign(piece, to);
            R.info[pick].alive = false;
            ++out.undersizedMerged;
        }
    }

    // 5. Renumber by smallest cell index and gather per-region statistics.
    RegionId newId[kMaxId + 1] = {};
    unsigned count = 0;
    std::vector<unsigned> oldOf;   // new id - 1 -> old id
    for (int c = 0; c < n; ++c)
    {
        const RegionId r = R.reg[c];
        if (r == kNoRegion || newId[r] != 0) { continue; }
        newId[r] = static_cast<RegionId>(++count);
        oldOf.push_back(r);
    }
    for (int c = 0; c < n; ++c) { cells[c].region = newId[R.reg[c]]; }

    // Cells of tiny zone components (AC3 amendment): absorbed whole, counted as fragmentCells instead of absorbed.
    std::vector<uint8_t> inFragment(static_cast<size_t>(n), 0);
    for (uint32_t ci : growth.tinyComponents)
    {
        if (ci >= growth.components.size()) { continue; }
        for (uint32_t c : growth.components[ci].cells) { if (c < static_cast<uint32_t>(n)) { inFragment[c] = 1; } }
    }
    uint32_t absorbed = 0, fragment = 0;
    for (int c = 0; c < n; ++c)
    {
        const RegionId r = cells[c].region;
        if (r == kNoRegion) { continue; }
        const Info& inf = R.info[oldOf[r - 1]];
        if (!inf.island && cells[c].depthZone != inf.zone) { ++(inFragment[c] ? fragment : absorbed); }
    }

    world.regions.absorbedCells = static_cast<uint16_t>(std::min<uint32_t>(absorbed, 0xFFFFu));
    world.regions.fragmentCells = static_cast<uint16_t>(std::min<uint32_t>(fragment, 0xFFFFu));
    world.regions.limitedBy = growth.targets.limitedBy;

    // Records: identity from the repair, everything derived from the written cells.
    for (unsigned id = 1; id <= count; ++id)
    {
        const Info& inf = R.info[oldOf[id - 1]];
        RegionRecord rec;
        rec.id = static_cast<RegionId>(id);
        rec.zone = inf.zone;
        rec.landmass = inf.landmass;
        rec.isIsland = inf.island;
        world.regions.AddRegion(rec);
    }
    out.neighbourOverflow = RecomputeRegionRecords(world, biomes);
    const unsigned mainland = world.regions.mainlandRegionCount;

    // AC4 amendment: repair (undersize merges) leaving the mainland below 10 regions reports terrain-limited, naming
    // the zone that lost the most regions against its target (ties: zone order).
    if (world.regions.limitedBy == DepthZone::None && static_cast<int>(mainland) < kTotalRegionsMin)
    {
        int finalCount[kRegionZoneCount] = {};
        for (unsigned i = 0; i < world.regions.regions.Size(); ++i)
        {
            const RegionRecord& rec = world.regions.regions[i];
            const int slot = ZoneSlot(rec.zone);
            if (!rec.isIsland && slot >= 0 && slot < kRegionZoneCount) { ++finalCount[slot]; }
        }
        int worst = 0;
        for (int z = 1; z < kRegionZoneCount; ++z)
        {
            if (growth.targets.count[z] - finalCount[z] > growth.targets.count[worst] - finalCount[worst]) { worst = z; }
        }
        world.regions.limitedBy = ZoneFromSlot(worst);
    }
    out.regionCount = static_cast<uint16_t>(count);
    return out;
}

RegionBuild BuildRegions(WorldStructure& world, const TerrainRules& rules, Dia::Maths::RandomStream& rng, const World::BiomeRegistry& biomes)
{
    const RegionGrowth g = BuildProvisionalRegions(world, rules, rng, biomes);
    return FinishRegions(world, rules, g, biomes);
}

} // namespace WorldGeneration
} // namespace CoW
