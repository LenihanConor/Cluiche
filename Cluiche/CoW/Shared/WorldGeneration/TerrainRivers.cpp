// S-1 Terrain Generator: Stages 6 (Rivers) and 7 (Lakes). See TerrainRivers.h.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma fp_contract(off)

#include <Shared/WorldGeneration/TerrainRivers.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <map>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

namespace {

// Downstream cell along the flow direction, or -1 for a sink / grid edge.
int Downstream(const HexTopology& topo, const uint32_t* dir, int cell)
{
    if (dir[cell] >= 6u)
    {
        return -1;
    }
    const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(cell), dir[cell]);
    return nb == kNoCell ? -1 : static_cast<int>(nb);
}

} // namespace

void ComputeStage6(const TerrainRules& rules, GridDimensions dims, float rivers, const float* filledElevation,
                   const uint32_t* flowDirection, const float* flowAccumulation, const uint8_t* oceanMask,
                   uint32_t* riverMask, Stage6Debug& out)
{
    out = Stage6Debug{};
    const int w = std::max(dims.width, 0);
    const int h = std::max(dims.height, 0);
    const int n = w * h;
    if (n == 0 || riverMask == nullptr)
    {
        return;
    }
    std::fill(riverMask, riverMask + n, 0u);
    if (filledElevation == nullptr || flowDirection == nullptr || flowAccumulation == nullptr || oceanMask == nullptr ||
        !(rivers > 0.0f))
    {
        return;
    }

    const HexTopology topo(w, h);
    const double fMax = rules.kFlowMax();
    const double fMin = rules.kFlowMin();
    const double threshold = fMax * std::pow(fMin / fMax, static_cast<double>(std::min(rivers, 1.0f)));
    out.threshold = static_cast<float>(threshold);

    std::vector<uint8_t> cand(static_cast<size_t>(n), 0);
    for (int i = 0; i < n; ++i)
    {
        if (oceanMask[i] == 0 && static_cast<double>(flowAccumulation[i]) >= threshold)
        {
            cand[i] = 1;
            ++out.candidates;
        }
    }
    if (out.candidates == 0)
    {
        return;
    }

    // Mouth (first ocean cell on the flow path) per candidate; -1 = no ocean reached, -2 = not yet known.
    std::vector<int> mouth(static_cast<size_t>(n), -2);
    std::vector<int> stack;
    for (int i = 0; i < n; ++i)
    {
        if (cand[i] == 0 || mouth[i] != -2)
        {
            continue;
        }
        stack.clear();
        int c = i;
        int result = -1;
        for (int steps = 0; steps <= n; ++steps)
        {
            if (oceanMask[c] != 0) { result = c; break; }
            if (mouth[c] != -2) { result = mouth[c]; break; }
            stack.push_back(c);
            const int d = Downstream(topo, flowDirection, c);
            if (d < 0) { result = -1; break; }
            c = d;
        }
        for (int s : stack) { mouth[s] = result; }
    }

    // Longest run per cell: Kahn order over candidate-to-candidate links.
    std::vector<int> indeg(static_cast<size_t>(n), 0);
    std::vector<int> down(static_cast<size_t>(n), -1);
    std::vector<uint8_t> hasUp(static_cast<size_t>(n), 0);
    for (int i = 0; i < n; ++i)
    {
        if (cand[i] == 0 || mouth[i] < 0) { continue; }
        const int d = Downstream(topo, flowDirection, i);
        if (d >= 0 && cand[d] != 0)
        {
            down[i] = d;
            ++indeg[d];
            hasUp[d] = 1;
        }
    }
    std::vector<int> run(static_cast<size_t>(n), 1);
    std::vector<int> order;
    for (int i = 0; i < n; ++i)
    {
        if (cand[i] != 0 && mouth[i] >= 0 && indeg[i] == 0) { order.push_back(i); }
    }
    for (size_t head = 0; head < order.size(); ++head)
    {
        const int c = order[head];
        const int d = down[c];
        if (d < 0) { continue; }
        run[d] = std::max(run[d], run[c] + 1);
        if (--indeg[d] == 0) { order.push_back(d); }
    }

    // Per-tree statistics; the map keeps trees ordered by mouth cell.
    std::map<int, RiverTree> trees;
    for (int i = 0; i < n; ++i)
    {
        if (cand[i] == 0 || mouth[i] < 0) { continue; }
        RiverTree& t = trees[mouth[i]];
        t.mouth = mouth[i];
        ++t.cellCount;
        t.longestRun = std::max(t.longestRun, run[i]);
        if (hasUp[i] == 0)
        {
            t.highestSource = std::max(t.highestSource, filledElevation[i]);
        }
    }
    const float minSource = rules.kSourceMinElev();
    const int   minRun = static_cast<int>(std::ceil(rules.kMinRiverLength()));
    for (auto& kv : trees)
    {
        RiverTree& t = kv.second;
        t.kept = t.highestSource >= minSource && t.longestRun >= minRun;
        out.trees.push_back(t);
    }
    for (int i = 0; i < n; ++i)
    {
        if (cand[i] != 0 && mouth[i] >= 0 && trees[mouth[i]].kept)
        {
            riverMask[i] = 1u;
            ++out.riverCells;
        }
    }
}

int ComputeStage7(GridDimensions dims, int lakes, const Stage5Debug& drainage, const uint32_t* flowDirection,
                  const uint8_t* oceanMask, uint32_t* lakeMask, Stage7Debug& out)
{
    out = Stage7Debug{};
    const int w = std::max(dims.width, 0);
    const int h = std::max(dims.height, 0);
    const int n = w * h;
    if (n == 0 || lakeMask == nullptr)
    {
        return 0;
    }
    std::fill(lakeMask, lakeMask + n, 0u);
    if (flowDirection == nullptr || oceanMask == nullptr)
    {
        return 0;
    }

    const HexTopology topo(w, h);
    const int count = std::min(std::max(lakes, 0), static_cast<int>(drainage.ranked.size()));
    for (int k = 0; k < count; ++k)
    {
        const int bi = drainage.ranked[static_cast<size_t>(k)];
        const Basin& basin = drainage.basins[static_cast<size_t>(bi)];
        Lake lake;
        lake.basinIndex = bi;
        lake.cells = basin.cells;
        lake.surfaceHeight = basin.spillHeight;
        lake.spillCell = basin.spillCell;
        for (int c : basin.cells)
        {
            lakeMask[c] = static_cast<uint32_t>(k + 1);
        }
        int c = basin.spillCell;
        for (int steps = 0; c >= 0 && c < n && steps <= n; ++steps)
        {
            lake.outflowPath.push_back(c);
            if (oceanMask[c] != 0) { break; }
            c = Downstream(topo, flowDirection, c);
        }
        out.lakes.push_back(std::move(lake));
    }
    return count;
}

} // namespace WorldGeneration
} // namespace CoW
