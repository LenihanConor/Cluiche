// S-1 Nodes: built-in corridor nodes. See NodesCorridor.h.
#include <Shared/WorldGeneration/NodesCorridor.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <vector>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

namespace
{
    struct Candidate
    {
        uint32_t cell;
        int      source;
    };

    bool IsNodeLand(const Cell& c)
    {
        return c.landmass != 0 && c.waterSource != WaterSource::Ocean && c.waterSource != WaterSource::Lake
            && c.terrain != TerrainType::Peak && c.terrain != TerrainType::Volcano;   // AC8b: no node on Peak/Volcano
    }

    bool OnBoundary(const WorldStructure& world, const HexTopology& topo, uint32_t cell, int barrier)
    {
        // Barrier k separates zone k from zone k+1.
        const DepthZone a = static_cast<DepthZone>(barrier);
        const DepthZone b = static_cast<DepthZone>(barrier + 1);
        const DepthZone z = world.Cells()[cell].depthZone;
        if (z != a && z != b) { return false; }
        const DepthZone other = (z == a) ? b : a;
        for (uint32_t d = 0; d < 6; ++d)
        {
            const uint32_t nb = topo.NeighbourAt(cell, d);
            if (nb != kNoCell && world.Cells()[nb].depthZone == other) { return true; }
        }
        return false;
    }

    // -1 when the cell is not a candidate for the barrier, else CorridorSourceKind.
    int SourceOf(const WorldStructure& world, const HexTopology& topo, uint32_t cell, int barrier, int gapReach)
    {
        const Cell& c = world.Cells()[cell];
        if (!IsNodeLand(c) || !OnBoundary(world, topo, cell, barrier)) { return -1; }
        if (c.terrainFlags & TerrainFlag::Pass) { return static_cast<int>(CorridorSourceKind::Pass); }
        if (c.terrainFlags & TerrainFlag::RiverCrossesBarrier) { return static_cast<int>(CorridorSourceKind::River); }

        const Dia::Pathfinding::CellCoord origin = topo.ToCoord(cell);
        const int span = gapReach + 1;
        bool coastNear = false;
        for (int dy = -span; dy <= span; ++dy)
        {
            for (int dx = -span; dx <= span; ++dx)
            {
                const int x = origin.x + dx, y = origin.y + dy;
                if (x < 0 || y < 0 || x >= world.dimensions.width || y >= world.dimensions.height) { continue; }
                const uint32_t d = HexTopology::Distance(origin, Dia::Pathfinding::CellCoord{ x, y });
                const uint8_t  f = world.At(x, y).terrainFlags;
                if (d <= 2 && (f & (TerrainFlag::Ridge | TerrainFlag::Barrier))) { return -1; }
                if (d <= static_cast<uint32_t>(gapReach) && (f & TerrainFlag::Coast)) { coastNear = true; }
            }
        }
        return coastNear ? static_cast<int>(CorridorSourceKind::Coastal) : -1;
    }
}

CorridorResult PlaceCorridorNodes(WorldStructure& world, const TerrainRules& rules, Dia::Maths::RandomStream& rng)
{
    CorridorResult out;
    const int n = world.CellCount();
    const uint32_t kind = SiteKindId(BuiltinNodeKinds::Corridor());
    const int spacing   = rules.kCorridorSpacing;
    const int gapReach  = rules.kCoastalGapReach;

    int target[2];
    for (int b = 0; b < 2; ++b) { target[b] = 2 + rng.RandomInt() % 3; }
    if (n == 0) { out.corridorLimited[0] = out.corridorLimited[1] = true; return out; }

    const HexTopology topo(world.dimensions.width, world.dimensions.height);
    const uint32_t nodeSpacing = static_cast<uint32_t>(rules.kNodeSpacing);
    std::vector<uint32_t> earlierBarriers;   // corridor nodes of earlier barriers: kNodeSpacing applies across barriers

    for (int b = 0; b < 2; ++b)
    {
        const int barrier = b + 1;
        std::vector<Candidate> cand;
        for (int i = 0; i < n; ++i)
        {
            const int s = SourceOf(world, topo, static_cast<uint32_t>(i), barrier, gapReach);
            if (s >= 0) { cand.push_back({ static_cast<uint32_t>(i), s }); ++out.candidatesByType[b][s]; }
        }
        out.candidates[b] = static_cast<uint16_t>(cand.size());

        for (int i = static_cast<int>(cand.size()) - 1; i >= 1; --i)
        {
            const int j = rng.RandomInt() % (i + 1);
            std::swap(cand[i], cand[j]);
        }
        std::stable_partition(cand.begin(), cand.end(),
                              [](const Candidate& c) { return c.source != static_cast<int>(CorridorSourceKind::Coastal); });

        std::vector<uint32_t> nodes;   // every node of every accepted corridor of this barrier
        int accepted = 0;
        for (const Candidate& c : cand)
        {
            if (accepted >= target[b]) { break; }
            const Dia::Pathfinding::CellCoord cc = topo.ToCoord(c.cell);
            bool tooClose = false;
            for (uint32_t other : nodes)
            {
                if (HexTopology::Distance(cc, topo.ToCoord(other)) < static_cast<uint32_t>(spacing)) { tooClose = true; break; }
            }
            for (uint32_t other : earlierBarriers)
            {
                if (HexTopology::Distance(cc, topo.ToCoord(other)) < nodeSpacing) { tooClose = true; break; }
            }
            if (tooClose || world.sites.Find(BuiltinNodeKinds::Corridor(), c.cell)) { continue; }

            std::vector<uint32_t> cells{ c.cell };
            if (c.source != static_cast<int>(CorridorSourceKind::Pass))
            {
                std::vector<uint32_t> extra;
                for (uint32_t d = 0; d < 6; ++d)
                {
                    const uint32_t nb = topo.NeighbourAt(c.cell, d);
                    if (nb == kNoCell) { continue; }
                    if (SourceOf(world, topo, nb, barrier, gapReach) != c.source) { continue; }
                    if (world.sites.Find(BuiltinNodeKinds::Corridor(), nb)) { continue; }
                    bool nearEarlier = false;
                    for (uint32_t other : earlierBarriers)
                    {
                        if (HexTopology::Distance(topo.ToCoord(nb), topo.ToCoord(other)) < nodeSpacing) { nearEarlier = true; break; }
                    }
                    if (nearEarlier) { continue; }
                    extra.push_back(nb);
                }
                std::sort(extra.begin(), extra.end());
                for (size_t k = 0; k < extra.size() && k < 2; ++k) { cells.push_back(extra[k]); }
            }

            const uint16_t flags = MakeCorridorFlags(barrier, c.source, static_cast<int>(cells.size()));
            for (uint32_t cell : cells)
            {
                Site s;
                s.kindCrc = kind;
                s.cell    = cell;
                s.region  = world.Cells()[cell].region;
                s.variant = static_cast<uint16_t>(accepted);
                s.flags   = flags;
                world.sites.Add(s);
                nodes.push_back(cell);
            }
            ++accepted;
        }
        earlierBarriers.insert(earlierBarriers.end(), nodes.begin(), nodes.end());
        out.accepted[b] = static_cast<uint8_t>(accepted);
        out.corridorLimited[b] = accepted < 2;
    }
    return out;
}

} // namespace WorldGeneration
} // namespace CoW
