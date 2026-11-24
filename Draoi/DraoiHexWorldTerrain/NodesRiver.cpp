// S-1 Nodes: built-in river nodes. See NodesRiver.h.
#include <DraoiHexWorldTerrain/NodesRiver.h>

#include <DiaPathfinding/AdjacencyGraph.h>
#include <DiaPathfinding/GridSearch.h>
#include <DiaGeometry2D/Grid/HexTopology.h>

#include <algorithm>
#include <vector>

namespace Draoi {
namespace HexWorld {

using Dia::Geometry2D::HexTopology;
using Dia::Geometry2D::kNoCell;

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

RiverNodeResult PlaceRiverNodes(WorldStructure& world, const TerrainRules& rules, const Draoi::HexWorld::NodeKindIds& nodeKinds)
{
    RiverNodeResult out;
    const int n = world.CellCount();
    if (n == 0) { return out; }

    const HexTopology topo(world.dimensions.width, world.dimensions.height);
    const Cell* cells = world.Cells();
    const uint32_t spacing      = static_cast<uint32_t>(rules.kNodeSpacing);
    const uint32_t riverSpacing = static_cast<uint32_t>(std::max(1, rules.kRiverNodeSpacing));

    auto downstreamOf = [&](uint32_t cell) -> uint32_t {
        const uint8_t dir = cells[cell].downstream;
        return dir < 6 ? topo.NeighbourAt(cell, dir) : kNoCell;
    };

    // Mouths (ascending cell index) and the upstream adjacency (children of each river cell).
    std::vector<uint32_t> mouths;
    std::vector<Dia::Pathfinding::AdjacencyEdge> edges;   // downstream -> child (the upstream relation)
    for (int i = 0; i < n; ++i)
    {
        if (!IsRiverCell(cells[i])) { continue; }
        const uint32_t ds = downstreamOf(static_cast<uint32_t>(i));
        if (ds == kNoCell) { continue; }
        if (IsOpenWater(cells[ds])) { mouths.push_back(static_cast<uint32_t>(i)); }
        else if (IsRiverCell(cells[ds])) { edges.push_back({ ds, static_cast<uint32_t>(i) }); }
    }
    out.trees = static_cast<uint16_t>(mouths.size());

    // Longest upstream path per tree: breadth-first from the mouth, deepest cell wins (ties lowest cell index).
    std::vector<Tree> trees(mouths.size());
    std::vector<uint32_t> depth(static_cast<size_t>(n), Dia::Pathfinding::kUnreached);
    std::vector<uint32_t> treeOf(static_cast<size_t>(n), Dia::Pathfinding::kUnreached);
    {
        const Dia::Pathfinding::AdjacencyGraph graph = [&] {
            Dia::Pathfinding::AdjacencyGraph g;
            g.Build(static_cast<uint32_t>(n), edges.data(), static_cast<uint32_t>(edges.size()));
            return g;
        }();
        std::vector<Dia::Pathfinding::StepSeed> seeds(mouths.size());
        for (size_t t = 0; t < mouths.size(); ++t) { seeds[t] = { mouths[t], 0u, static_cast<uint32_t>(t) }; }
        Dia::Pathfinding::GridSearchScratch scratch;
        Dia::Pathfinding::MultiSourceSteps(graph, seeds.data(), static_cast<uint32_t>(seeds.size()),
                                           [](uint32_t) { return true; }, static_cast<uint32_t>(n),
                                           depth.data(), treeOf.data(), scratch);
    }
    // Per tree: deepest cell wins, ties lowest cell index (ascending scan keeps the first).
    std::vector<uint32_t> deepestCell(mouths.size());
    std::vector<uint32_t> deepestDepthOf(mouths.size(), 0u);
    for (size_t t = 0; t < mouths.size(); ++t) { deepestCell[t] = mouths[t]; }
    for (uint32_t c = 0; c < static_cast<uint32_t>(n); ++c)
    {
        if (depth[c] == Dia::Pathfinding::kUnreached) { continue; }
        const uint32_t t = treeOf[c];
        if (depth[c] > deepestDepthOf[t]) { deepestDepthOf[t] = depth[c]; deepestCell[t] = c; }
    }
    for (size_t t = 0; t < mouths.size(); ++t)
    {
        const uint32_t deepest = deepestCell[t];
        const int deepestDepth = static_cast<int>(deepestDepthOf[t]);
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

    const uint32_t kRiver    = SiteKindId(nodeKinds.river);
    const uint32_t kCorridor = SiteKindId(nodeKinds.corridor);
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
            const Dia::Geometry2D::CellCoord here = topo.ToCoord(cell);
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

} // namespace HexWorld
} // namespace Draoi
