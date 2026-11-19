// S-1 Terrain Generator: Stage 5 (Drainage). See TerrainDrainage.h for the fill, epsilon and tie rules.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma fp_contract(off)

#include <Shared/WorldGeneration/TerrainDrainage.h>

#include <DiaPathfinding/GridSearch.h>
#include <DiaPathfinding/HexNeighbours.h>
#include <DiaPathfinding/HexTopology.h>

#include <algorithm>

namespace CoW {
namespace WorldGeneration {

namespace {

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

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
    {
        const Dia::Pathfinding::HexNeighbours graph(topo);
        Dia::Pathfinding::GridSearchScratch scratch;
        Dia::Pathfinding::PriorityFlood(graph, elevationRaw, oceanMask, kFillEpsilon, filled, level.data(), scratch);
    }

    // Cells the flood reached (ocean, or connected to ocean): needed by the flow-direction pass.
    std::vector<uint8_t> visited(static_cast<size_t>(n), 0);
    {
        const Dia::Pathfinding::HexNeighbours graph(topo);
        Dia::Pathfinding::GridSearchScratch scratch;
        std::vector<Dia::Pathfinding::StepSeed> seeds;
        for (int i = 0; i < n; ++i)
        {
            if (oceanMask[i] != 0) { seeds.push_back({ static_cast<uint32_t>(i), 0u, static_cast<uint32_t>(i) }); }
        }
        std::vector<uint32_t> steps(static_cast<size_t>(n), Dia::Pathfinding::kUnreached);
        Dia::Pathfinding::MultiSourceSteps(graph, seeds.data(), static_cast<uint32_t>(seeds.size()),
            [](uint32_t) { return true; }, static_cast<uint32_t>(n), steps.data(), nullptr, scratch);
        for (int i = 0; i < n; ++i) { visited[i] = steps[i] != Dia::Pathfinding::kUnreached ? 1 : 0; }
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
    std::vector<uint32_t> label(static_cast<size_t>(n), Dia::Pathfinding::kUnreached);
    {
        const Dia::Pathfinding::HexNeighbours graph(topo);
        Dia::Pathfinding::GridSearchScratch scratch;
        const uint32_t count = Dia::Pathfinding::ConnectedComponents(graph,
            [depth](uint32_t c) { return depth[c] > 0.0f; }, label.data(), nullptr, scratch);
        out.basins.resize(count);
        // Ascending scan: each basin's cells arrive sorted, its first cell is its smallest index.
        for (int i = 0; i < n; ++i)
        {
            const uint32_t l = label[static_cast<size_t>(i)];
            if (l != Dia::Pathfinding::kUnreached) { out.basins[l].cells.push_back(i); }
        }
    }
    for (Basin& b : out.basins)
    {
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
