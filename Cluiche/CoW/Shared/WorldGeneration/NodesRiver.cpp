// S-1 Nodes: built-in river nodes. See NodesRiver.h.
#include <Shared/WorldGeneration/NodesRiver.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <vector>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

namespace
{
    bool IsRiverCell(const Cell& c) { return c.waterSource == WaterSource::River && c.landmass != 0; }
    bool IsOpenWater(const Cell& c) { return c.waterSource == WaterSource::Ocean || c.waterSource == WaterSource::Lake; }

    struct Tree
    {
        uint32_t              mouth = 0;
        std::vector<uint32_t> path;   // path[step] = cell `step` cells upstream of the mouth, along the longest path
    };
}

RiverNodeResult PlaceRiverNodes(WorldStructure& world, const TerrainRules& rules)
{
    RiverNodeResult out;
    const int n = world.CellCount();
    if (n == 0) { return out; }

    const HexTopology topo(world.dimensions.width, world.dimensions.height);
    const Cell* cells = world.Cells();
    const uint32_t spacing      = static_cast<uint32_t>(rules.GetInt(Rule::kNodeSpacing));
    const uint32_t riverSpacing = static_cast<uint32_t>(std::max(1, rules.GetInt(Rule::kRiverNodeSpacing)));

    auto downstreamOf = [&](uint32_t cell) -> uint32_t {
        const uint8_t dir = cells[cell].downstream;
        return dir < 6 ? topo.NeighbourAt(cell, dir) : kNoCell;
    };

    // Mouths (ascending cell index) and the upstream adjacency (children of each river cell).
    std::vector<uint32_t> mouths;
    std::vector<std::vector<uint32_t>> children(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        if (!IsRiverCell(cells[i])) { continue; }
        const uint32_t ds = downstreamOf(static_cast<uint32_t>(i));
        if (ds == kNoCell) { continue; }
        if (IsOpenWater(cells[ds])) { mouths.push_back(static_cast<uint32_t>(i)); }
        else if (IsRiverCell(cells[ds])) { children[ds].push_back(static_cast<uint32_t>(i)); }
    }
    out.trees = static_cast<uint16_t>(mouths.size());

    // Longest upstream path per tree: breadth-first from the mouth, deepest cell wins (ties lowest cell index).
    std::vector<Tree> trees(mouths.size());
    std::vector<int> depth(static_cast<size_t>(n), -1);
    for (size_t t = 0; t < mouths.size(); ++t)
    {
        std::vector<uint32_t> frontier{ mouths[t] };
        depth[mouths[t]] = 0;
        uint32_t deepest = mouths[t];
        int deepestDepth = 0;
        while (!frontier.empty())
        {
            std::vector<uint32_t> next;
            for (uint32_t c : frontier)
            {
                for (uint32_t ch : children[c])
                {
                    if (depth[ch] >= 0) { continue; }
                    depth[ch] = depth[c] + 1;
                    next.push_back(ch);
                }
            }
            if (!next.empty())
            {
                deepestDepth = depth[next.front()];
                deepest = *std::min_element(next.begin(), next.end());
            }
            frontier.swap(next);
        }
        Tree& tree = trees[t];
        tree.mouth = mouths[t];
        tree.path.assign(static_cast<size_t>(deepestDepth) + 1, 0);
        uint32_t cur = deepest;
        for (int step = deepestDepth; step >= 0; --step)
        {
            tree.path[static_cast<size_t>(step)] = cur;
            cur = downstreamOf(cur);
        }
    }

    const uint32_t kRiver    = SiteKindId(BuiltinNodeKinds::River());
    const uint32_t kCorridor = SiteKindId(BuiltinNodeKinds::Corridor());
    auto makeSite = [&](uint32_t cell, size_t ordinal, bool mouth) {
        Site s;
        s.kindCrc = kRiver;
        s.cell    = cell;
        s.region  = cells[cell].region;
        s.variant = static_cast<uint16_t>(ordinal);
        s.flags   = MakeRiverFlags(mouth);
        return s;
    };

    // 1. Mouths: always placed.
    for (size_t t = 0; t < trees.size(); ++t)
    {
        if (world.sites.Add(makeSite(trees[t].mouth, t, true))) { ++out.mouthNodes; }
    }

    // 2. Upstream nodes on the main landmass.
    const bool hasRegions = world.regions.regions.Size() > 0;
    for (size_t t = 0; t < trees.size(); ++t)
    {
        const Tree& tree = trees[t];
        if (cells[tree.mouth].landmass != world.mainLandmass) { continue; }
        for (size_t step = riverSpacing; step < tree.path.size(); step += riverSpacing)
        {
            const uint32_t cell = tree.path[step];
            const Cell& c = cells[cell];
            if (c.landmass != world.mainLandmass || (hasRegions && c.region == kNoRegion)) { ++out.skippedUnclaimable; continue; }

            const bool crossing = (c.terrainFlags & TerrainFlag::RiverCrossesBarrier) != 0;
            const Dia::Pathfinding::CellCoord here = topo.ToCoord(cell);
            bool tooClose = false;
            if (!crossing)
            {
                for (const Site& s : world.sites.Of(kCorridor))
                {
                    if (HexTopology::Distance(here, topo.ToCoord(s.cell)) < spacing) { tooClose = true; break; }
                }
            }
            if (!tooClose)
            {
                for (const Site& s : world.sites.Of(kRiver))
                {
                    if (s.variant == t) { continue; }   // same tree: kRiverNodeSpacing apart by construction
                    if (HexTopology::Distance(here, topo.ToCoord(s.cell)) < spacing) { tooClose = true; break; }
                }
            }
            if (tooClose) { ++out.skippedSpacing; continue; }
            if (world.sites.Add(makeSite(cell, t, false))) { ++out.upstreamNodes; }
        }
    }
    return out;
}

} // namespace WorldGeneration
} // namespace CoW
