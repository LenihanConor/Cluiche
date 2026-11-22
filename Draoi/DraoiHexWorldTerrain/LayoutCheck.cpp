// S-1 Regions, Nodes and Site Hook: layout check. See LayoutCheck.h.
#include <DraoiHexWorldTerrain/LayoutCheck.h>

#include <DraoiHexWorldTerrain/NodesCorridor.h>
#include <DraoiHexWorldTerrain/NodesRiver.h>
#include <DraoiHexWorldTerrain/RegionPartition.h>
#include <DraoiHexWorld/Sites.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <set>

namespace Draoi {
namespace HexWorld {

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

namespace
{
    constexpr int kMinCorridors = 2, kMaxCorridors = 4;
    constexpr int kMinRegionNodes = 6, kMaxRegionNodes = 15;
    constexpr int kMinTotalNodes = 80, kMaxTotalNodes = 150;

    bool IsLand(const Cell& c) { return c.landmass != 0 && c.waterSource != WaterSource::Ocean; }

    bool IsNodeCell(const Cell& c)
    {
        return IsLand(c) && c.waterSource != WaterSource::Lake && c.terrain != TerrainType::Lake &&
               c.terrain != TerrainType::DeepWater && c.terrain != TerrainType::ShallowWater;
    }

    void Evidence(LayoutViolation& v, uint32_t cell)
    {
        if (v.cellCount < kMaxEvidenceCells) { v.cells[v.cellCount++] = cell; }
    }

    LayoutViolation& Add(LayoutReport& report, LayoutCode code, RegionId region, uint32_t count)
    {
        report.violations.emplace_back();
        LayoutViolation& v = report.violations.back();
        v.code = code;
        v.region = region;
        v.count = count;
        report.ok = false;
        return v;
    }

    struct Node
    {
        uint32_t cell = 0;
        uint32_t kind = 0;
        uint16_t variant = 0;
        uint16_t flags = 0;
    };
}

const char* LayoutCodeName(LayoutCode code)
{
    switch (code)
    {
    case LayoutCode::UncoveredLand:      return "UncoveredLand";
    case LayoutCode::OceanHasRegion:     return "OceanHasRegion";
    case LayoutCode::RegionDisconnected: return "RegionDisconnected";
    case LayoutCode::RegionTwoStrata:     return "RegionTwoStrata";
    case LayoutCode::RegionTooSmall:     return "RegionTooSmall";
    case LayoutCode::RegionCountRange:   return "RegionCountRange";
    case LayoutCode::AbsorbedOverCap:    return "AbsorbedOverCap";
    case LayoutCode::NodeOnWater:        return "NodeOnWater";
    case LayoutCode::NodeSpacing:        return "NodeSpacing";
    case LayoutCode::CorridorCount:      return "CorridorCount";
    case LayoutCode::RegionNodeCount:    return "RegionNodeCount";
    case LayoutCode::TotalNodeCount:     return "TotalNodeCount";
    case LayoutCode::FragmentOverCap:    return "FragmentOverCap";
    }
    return "Unknown";
}

LayoutReport CheckLayout(const WorldStructure& world, const TerrainRules& rules, const Draoi::HexWorld::NodeKindIds& nodeKinds,
                         const LayoutLimits& limits)
{
    LayoutReport report;
    const int n = world.CellCount();
    const Cell* cells = world.Cells();
    const unsigned regionCount = world.regions.regions.Size();
    const HexTopology topo(std::max(world.dimensions.width, 1), std::max(world.dimensions.height, 1));

    // 1. coverage, and 4. absorbed cap
    {
        uint32_t uncovered = 0, oceanRegion = 0, land = 0;
        LayoutViolation evU, evO;
        for (int i = 0; i < n; ++i)
        {
            const Cell& c = cells[i];
            if (IsLand(c))
            {
                ++land;
                if (c.region == kNoRegion || c.region > regionCount) { ++uncovered; Evidence(evU, static_cast<uint32_t>(i)); }
            }
            else if (c.region != kNoRegion) { ++oceanRegion; Evidence(evO, static_cast<uint32_t>(i)); }
        }
        if (uncovered)   { evU.code = LayoutCode::UncoveredLand;  evU.count = uncovered;   report.violations.push_back(evU); report.ok = false; }
        if (oceanRegion) { evO.code = LayoutCode::OceanHasRegion; evO.count = oceanRegion; report.violations.push_back(evO); report.ok = false; }

        if (static_cast<float>(world.regions.absorbedCells) > rules.kMaxAbsorbedShare * static_cast<float>(land))
        {
            Add(report, LayoutCode::AbsorbedOverCap, kNoRegion, world.regions.absorbedCells);
        }
        if (static_cast<float>(world.regions.fragmentCells) > rules.kMaxFragmentShare * static_cast<float>(land))
        {
            Add(report, LayoutCode::FragmentOverCap, kNoRegion, world.regions.fragmentCells);
        }
    }

    // 2. region shape and 3. counts
    {
        std::vector<std::vector<uint32_t>> cellsOf(regionCount + 1);
        for (int i = 0; i < n; ++i)
        {
            const RegionId r = cells[i].region;
            if (r >= 1 && r <= regionCount && IsLand(cells[i])) { cellsOf[r].push_back(static_cast<uint32_t>(i)); }
        }
        int mainland = 0;
        const int minCells = rules.kMinRegionCells;
        std::vector<int> comp(static_cast<size_t>(n), -1);
        for (unsigned r = 1; r <= regionCount; ++r)
        {
            const RegionRecord& rec = world.regions.regions[r - 1];
            const std::vector<uint32_t>& rc = cellsOf[r];
            if (!rec.isIsland) { ++mainland; }
            if (rec.isIsland || rec.landmass != world.mainLandmass || rc.empty()) { continue; }

            // connectivity
            std::vector<std::vector<uint32_t>> pieces;
            for (uint32_t start : rc)
            {
                if (comp[start] >= 0) { continue; }
                const int id = static_cast<int>(pieces.size());
                pieces.emplace_back();
                std::vector<uint32_t> stack{ start };
                comp[start] = id;
                while (!stack.empty())
                {
                    const uint32_t c = stack.back(); stack.pop_back();
                    pieces[id].push_back(c);
                    for (uint32_t d = 0; d < 6; ++d)
                    {
                        const uint32_t nb = topo.NeighbourAt(c, d);
                        if (nb == kNoCell || static_cast<int>(nb) >= n) { continue; }
                        if (cells[nb].region != r || !IsLand(cells[nb]) || comp[nb] >= 0) { continue; }
                        comp[nb] = id;
                        stack.push_back(nb);
                    }
                }
            }
            if (pieces.size() > 1)
            {
                size_t largest = 0;
                for (size_t p = 1; p < pieces.size(); ++p) { if (pieces[p].size() > pieces[largest].size()) { largest = p; } }
                LayoutViolation& v = Add(report, LayoutCode::RegionDisconnected, static_cast<RegionId>(r), static_cast<uint32_t>(pieces.size()));
                for (size_t p = 0; p < pieces.size(); ++p)
                {
                    if (p == largest) { continue; }
                    for (uint32_t c : pieces[p]) { Evidence(v, c); }
                }
            }
            // zone
            uint32_t foreign = 0;
            for (uint32_t c : rc) { foreign += cells[c].stratum != rec.stratum; }
            if (foreign * 2 > rc.size())
            {
                LayoutViolation& v = Add(report, LayoutCode::RegionTwoStrata, static_cast<RegionId>(r), foreign);
                for (uint32_t c : rc) { if (cells[c].stratum != rec.stratum) { Evidence(v, c); } }
            }
            // size
            if (static_cast<int>(rc.size()) < minCells)
            {
                LayoutViolation& v = Add(report, LayoutCode::RegionTooSmall, static_cast<RegionId>(r), static_cast<uint32_t>(rc.size()));
                for (uint32_t c : rc) { Evidence(v, c); }
            }
        }
        if (limits.regionLimitedBy == kNoStratum && (mainland < kTotalRegionsMin || mainland > kTotalRegionsMax))
        {
            Add(report, LayoutCode::RegionCountRange, kNoRegion, static_cast<uint32_t>(mainland));
        }
    }

    // 5. built-in nodes
    std::vector<Node> nodes;
    const uint32_t kinds[4] = { SiteKindId(nodeKinds.corridor), SiteKindId(nodeKinds.river),
                                SiteKindId(nodeKinds.hunting),  SiteKindId(nodeKinds.resource) };
    for (const Site& s : world.sites.All())
    {
        if (std::find(kinds, kinds + 4, s.kindCrc) == kinds + 4) { continue; }
        nodes.push_back({ s.cell, s.kindCrc, s.variant, s.flags });
    }

    {   // on valid land
        LayoutViolation ev;
        uint32_t bad = 0;
        for (const Node& nd : nodes)
        {
            if (static_cast<int>(nd.cell) >= n || !IsNodeCell(cells[nd.cell])) { ++bad; Evidence(ev, nd.cell); }
        }
        if (bad) { ev.code = LayoutCode::NodeOnWater; ev.count = bad; report.violations.push_back(ev); report.ok = false; }
    }

    {   // spacing
        const int spacing = rules.kNodeSpacing;
        const uint32_t corridor = kinds[0], river = kinds[1];
        auto exempt = [&](const Node& a, const Node& c) {
            if (a.kind == corridor && c.kind == corridor && CorridorBarrier(a.flags) == CorridorBarrier(c.flags) && a.variant == c.variant) { return true; }
            if ((a.kind == river && RiverIsMouth(a.flags)) || (c.kind == river && RiverIsMouth(c.flags))) { return true; }
            if (cells[a.cell].terrainFlags & TerrainFlag::RiverCrossesBarrier) { return true; }
            if (cells[c.cell].terrainFlags & TerrainFlag::RiverCrossesBarrier) { return true; }
            return false;
        };
        LayoutViolation ev;
        uint32_t pairs = 0;
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            if (static_cast<int>(nodes[i].cell) >= n) { continue; }
            for (size_t j = i + 1; j < nodes.size(); ++j)
            {
                if (static_cast<int>(nodes[j].cell) >= n) { continue; }
                if (static_cast<int>(HexTopology::Distance(topo.ToCoord(nodes[i].cell), topo.ToCoord(nodes[j].cell))) >= spacing) { continue; }
                if (exempt(nodes[i], nodes[j])) { continue; }
                ++pairs;
                Evidence(ev, nodes[i].cell);
                Evidence(ev, nodes[j].cell);
            }
        }
        if (pairs) { ev.code = LayoutCode::NodeSpacing; ev.count = pairs; report.violations.push_back(ev); report.ok = false; }
    }

    // corridors per barrier
    for (int barrier = 1; barrier <= static_cast<int>(std::min<uint32_t>(limits.barrierCount, kMaxBarriers)); ++barrier)
    {
        std::set<uint16_t> ordinals;
        std::vector<uint32_t> at;
        for (const Node& nd : nodes)
        {
            if (nd.kind == kinds[0] && CorridorBarrier(nd.flags) == barrier) { ordinals.insert(nd.variant); at.push_back(nd.cell); }
        }
        const int count = static_cast<int>(ordinals.size());
        if (count > kMaxCorridors || (count < kMinCorridors && !limits.corridorLimited[barrier - 1]))
        {
            LayoutViolation& v = Add(report, LayoutCode::CorridorCount, static_cast<RegionId>(barrier), static_cast<uint32_t>(count));
            for (uint32_t c : at) { Evidence(v, c); }
        }
    }

    if (!limits.nodeLimited)
    {
        std::vector<std::vector<uint32_t>> nodeCellsOf(regionCount + 1);
        for (const Node& nd : nodes)
        {
            if (static_cast<int>(nd.cell) >= n) { continue; }
            const RegionId r = cells[nd.cell].region;
            if (r >= 1 && r <= regionCount) { nodeCellsOf[r].push_back(nd.cell); }
        }
        for (unsigned r = 1; r <= regionCount; ++r)
        {
            const int count = static_cast<int>(nodeCellsOf[r].size());
            if (count < kMinRegionNodes || count > kMaxRegionNodes)
            {
                LayoutViolation& v = Add(report, LayoutCode::RegionNodeCount, static_cast<RegionId>(r), static_cast<uint32_t>(count));
                for (uint32_t c : nodeCellsOf[r]) { Evidence(v, c); }
            }
        }
        const int total = static_cast<int>(nodes.size());
        if (total < kMinTotalNodes || total > kMaxTotalNodes) { Add(report, LayoutCode::TotalNodeCount, kNoRegion, static_cast<uint32_t>(total)); }
    }

    return report;
}

} // namespace HexWorld
} // namespace Draoi
