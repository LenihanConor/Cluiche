// S-1 Terrain Generator: Stage 5 (Drainage). See TerrainDrainage.h for the fill, epsilon and tie rules.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma fp_contract(off)

#include <Shared/WorldGeneration/TerrainDrainage.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>

namespace CoW {
namespace WorldGeneration {

namespace {

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

struct FloodEntry
{
    float level;
    float filled;
    int   cell;
};

// Min-heap order on (level, filled, cell): a strict total order (each cell is pushed once).
struct FloodLater
{
    bool operator()(const FloodEntry& a, const FloodEntry& b) const
    {
        if (a.level != b.level) { return a.level > b.level; }
        if (a.filled != b.filled) { return a.filled > b.filled; }
        return a.cell > b.cell;
    }
};

} // namespace

void ComputeStage5(const TerrainRules& rules, GridDimensions dims, const float* elevationRaw,
                   const uint8_t* oceanMask, Stage5Layers& layers, Stage5Debug& out)
{
    out = Stage5Debug{};
    const int w = std::max(dims.width, 0);
    const int h = std::max(dims.height, 0);
    const int n = w * h;
    if (n == 0 || elevationRaw == nullptr || oceanMask == nullptr || layers.filledElevation == nullptr ||
        layers.basinDepth == nullptr || layers.basinRank == nullptr || layers.flowDirection == nullptr ||
        layers.flowAccumulation == nullptr)
    {
        return;
    }

    float*    filled = layers.filledElevation;
    float*    depth  = layers.basinDepth;
    uint32_t* rank   = layers.basinRank;
    uint32_t* dir    = layers.flowDirection;
    float*    acc    = layers.flowAccumulation;
    std::vector<float>& level = out.fillLevel;
    level.assign(static_cast<size_t>(n), 0.0f);

    // Neighbour table (kNoCell past the edge), directions 0..5.
    const HexTopology topo(w, h);
    std::vector<uint32_t> nbr(static_cast<size_t>(n) * 6u);
    for (int i = 0; i < n; ++i)
    {
        for (uint32_t k = 0; k < 6; ++k)
        {
            nbr[static_cast<size_t>(i) * 6u + k] = topo.NeighbourAt(static_cast<uint32_t>(i), k);
        }
    }

    // 1. Fill: priority flood seeded from every ocean cell. Unreached land (no ocean anywhere in
    //    its component) keeps filled = level = raw.
    std::vector<uint8_t> visited(static_cast<size_t>(n), 0);
    std::vector<FloodEntry> heap;
    heap.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        filled[i] = elevationRaw[i];
        level[i] = elevationRaw[i];
        if (oceanMask[i] != 0)
        {
            visited[i] = 1;
            heap.push_back(FloodEntry{ level[i], filled[i], i });
        }
    }
    const FloodLater later;
    std::make_heap(heap.begin(), heap.end(), later);
    while (!heap.empty())
    {
        std::pop_heap(heap.begin(), heap.end(), later);
        const FloodEntry e = heap.back();
        heap.pop_back();
        for (uint32_t k = 0; k < 6; ++k)
        {
            const uint32_t nb = nbr[static_cast<size_t>(e.cell) * 6u + k];
            if (nb == kNoCell || visited[nb] != 0)
            {
                continue;
            }
            visited[nb] = 1;
            const float raw = elevationRaw[nb];
            level[nb] = std::max(raw, e.level);
            filled[nb] = std::max(raw, e.filled + kFillEpsilon);
            heap.push_back(FloodEntry{ level[nb], filled[nb], static_cast<int>(nb) });
            std::push_heap(heap.begin(), heap.end(), later);
        }
    }

    // 2. Basins: epsilon-free depth, components over the six hex neighbours, discovered in
    //    ascending cell order (so each basin's first cell is its smallest index).
    for (int i = 0; i < n; ++i)
    {
        const float d = oceanMask[i] != 0 ? 0.0f : level[i] - elevationRaw[i];
        depth[i] = d >= kBasinDepthFloor ? d : 0.0f;
        rank[i] = 0u;
    }
    const float minBasinDepth = rules.kMinBasinDepth();
    std::vector<int> label(static_cast<size_t>(n), -1);
    std::vector<int> stack;
    for (int i = 0; i < n; ++i)
    {
        if (depth[i] <= 0.0f || label[i] != -1)
        {
            continue;
        }
        const int id = static_cast<int>(out.basins.size());
        out.basins.emplace_back();
        Basin& b = out.basins.back();
        label[i] = id;
        stack.assign(1, i);
        while (!stack.empty())
        {
            const int c = stack.back();
            stack.pop_back();
            b.cells.push_back(c);
            for (uint32_t k = 0; k < 6; ++k)
            {
                const uint32_t nb = nbr[static_cast<size_t>(c) * 6u + k];
                if (nb != kNoCell && depth[nb] > 0.0f && label[nb] == -1)
                {
                    label[nb] = id;
                    stack.push_back(static_cast<int>(nb));
                }
            }
        }
        std::sort(b.cells.begin(), b.cells.end());
        b.firstCell = b.cells.front();
        for (int c : b.cells)
        {
            b.volume += depth[c];
            b.maxDepth = std::max(b.maxDepth, depth[c]);
            b.spillHeight = std::max(b.spillHeight, level[c]);
        }
        b.eligible = b.maxDepth >= minBasinDepth && b.cells.size() >= 2u;
    }

    // 3. Flow direction: largest drop on the filled surface, ties by the lowest direction.
    for (int i = 0; i < n; ++i)
    {
        dir[i] = kFlowSink;
        if (oceanMask[i] != 0 || visited[i] == 0)   // ocean, or land no ocean reaches
        {
            continue;
        }
        float bestDrop = 0.0f;
        for (uint32_t k = 0; k < 6; ++k)
        {
            const uint32_t nb = nbr[static_cast<size_t>(i) * 6u + k];
            if (nb == kNoCell)
            {
                continue;
            }
            const float drop = filled[i] - filled[nb];
            if (drop > bestDrop)
            {
                bestDrop = drop;
                dir[i] = k;
            }
        }
    }

    // Spill cell: where the basin's lowest filled cell (ties by lowest index) drains to. Its filled
    // value is below every basin cell's, so it is always outside the basin.
    for (Basin& b : out.basins)
    {
        int lowest = b.cells.front();
        for (int c : b.cells)
        {
            if (filled[c] < filled[lowest])
            {
                lowest = c;
            }
        }
        b.spillCell = dir[lowest] < kFlowSink ? static_cast<int>(nbr[static_cast<size_t>(lowest) * 6u + dir[lowest]]) : -1;
    }

    // Ranking: eligible basins by volume (largest first), then smallest cell index.
    for (int b = 0; b < static_cast<int>(out.basins.size()); ++b)
    {
        if (out.basins[static_cast<size_t>(b)].eligible)
        {
            out.ranked.push_back(b);
        }
    }
    std::sort(out.ranked.begin(), out.ranked.end(), [&](int a, int b)
    {
        const Basin& ba = out.basins[static_cast<size_t>(a)];
        const Basin& bb = out.basins[static_cast<size_t>(b)];
        if (ba.volume != bb.volume) { return ba.volume > bb.volume; }
        return ba.firstCell < bb.firstCell;
    });
    for (size_t r = 0; r < out.ranked.size(); ++r)
    {
        Basin& b = out.basins[static_cast<size_t>(out.ranked[r])];
        b.rank = static_cast<uint32_t>(r + 1);
        for (int c : b.cells)
        {
            rank[c] = b.rank;
        }
    }
    out.eligibleBasins = static_cast<int>(out.ranked.size());

    // 4. Flow accumulation: land cells from the highest filled to the lowest (ties by ascending
    //    index); every downstream cell is strictly lower, so its upstream totals are complete first.
    const float highlandRain = rules.kHighlandRain();
    std::vector<int> order;
    order.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        acc[i] = 0.0f;
        if (oceanMask[i] == 0)
        {
            acc[i] = 1.0f + highlandRain * elevationRaw[i];
            order.push_back(i);
        }
    }
    std::sort(order.begin(), order.end(), [&](int a, int b)
    {
        if (filled[a] != filled[b]) { return filled[a] > filled[b]; }
        return a < b;
    });
    for (int c : order)
    {
        if (dir[c] >= kFlowSink)
        {
            continue;
        }
        const uint32_t down = nbr[static_cast<size_t>(c) * 6u + dir[c]];
        if (oceanMask[down] == 0)
        {
            acc[down] += acc[c];
        }
    }
}

} // namespace WorldGeneration
} // namespace CoW
