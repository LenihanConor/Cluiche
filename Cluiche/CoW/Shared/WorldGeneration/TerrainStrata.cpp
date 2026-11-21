// S-1 Terrain Generator: Stage 8 (DepthZones). See TerrainDepthZones.h.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma fp_contract(off)

#include <Shared/WorldGeneration/TerrainDepthZones.h>

#include <DiaPathfinding/GridSearch.h>
#include <DiaPathfinding/HexNeighbours.h>
#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::HexTopology;

void ComputeZoneCost(const TerrainRules& rules, GridDimensions dims, float barrier, const uint32_t* landmass,
                     const uint32_t* coastDistance, const float* ridgeMask, const float* barrierMask,
                     const uint32_t* riverMask, float* out)
{
    const int w = std::max(dims.width, 0);
    const int h = std::max(dims.height, 0);
    const int n = w * h;
    if (n == 0 || out == nullptr)
    {
        return;
    }
    std::fill(out, out + n, 0.0f);
    if (landmass == nullptr || coastDistance == nullptr)
    {
        return;
    }

    const HexTopology topo(w, h);
    const double ridgeCost = rules.kRidgeCost;
    const double barrierCost = static_cast<double>(rules.kBarrierCost) * static_cast<double>(barrier);
    const double corridor = rules.kRiverCorridor;

    // Cost of entering cell c. Terms are non-negative, so a river (corridor <= 1) never costs more.
    auto entryCost = [&](int c) -> double
    {
        double terms = 0.0;
        if (ridgeMask != nullptr)
        {
            terms += ridgeCost * static_cast<double>(ridgeMask[c]);
        }
        if (barrierMask != nullptr)
        {
            terms += barrierCost * static_cast<double>(barrierMask[c]);
        }
        const bool river = riverMask != nullptr && riverMask[c] != 0u;
        return river ? 1.0 + corridor * terms : 1.0 + terms;
    };

    constexpr double kInf = 1e30;   // unreached cells hold infinity(), which is >= kInf
    std::vector<Dia::Pathfinding::CostSeed<double>> seeds;
    for (int i = 0; i < n; ++i)
    {
        if (landmass[i] != 0u && coastDistance[i] == 1u)
        {
            // source cost 1 regardless of the cell's own ridge/barrier terms
            seeds.push_back({static_cast<uint32_t>(i), 1.0, static_cast<uint32_t>(i)});
        }
    }
    std::vector<double> dist(static_cast<size_t>(n));
    Dia::Pathfinding::DijkstraOptions<double> opts;
    opts.outCost = dist.data();
    const Dia::Pathfinding::HexNeighbours graph(topo);
    Dia::Pathfinding::GridSearchScratch scratch;
    Dia::Pathfinding::MultiSourceDijkstra<double>(
        graph, seeds.data(), static_cast<uint32_t>(seeds.size()),
        [&](uint32_t, uint32_t to) -> double { return landmass[to] == 0u ? -1.0 : entryCost(static_cast<int>(to)); },
        opts, scratch);
    for (int i = 0; i < n; ++i)
    {
        if (landmass[i] != 0u)
        {
            out[i] = dist[i] >= kInf ? 1.0f : static_cast<float>(dist[i]);
        }
    }
}

void ComputeStage8(const TerrainRules& rules, const World::ZoneTable& zones, GridDimensions dims, float barrier, int mainLandmass,
                   const uint32_t* landmass, const uint32_t* coastDistance, const float* ridgeMask,
                   const float* barrierMask, const uint32_t* riverMask, Stage8Layers out, Stage8Debug& dbg)
{
    dbg = Stage8Debug{};
    const int w = std::max(dims.width, 0);
    const int h = std::max(dims.height, 0);
    const int n = w * h;
    if (n == 0 || out.zoneCostBase == nullptr || out.zoneCost == nullptr || out.depthZone == nullptr)
    {
        return;
    }
    std::fill(out.depthZone, out.depthZone + n, static_cast<uint32_t>(kNoZone));
    ComputeZoneCost(rules, dims, barrier, landmass, coastDistance, ridgeMask, barrierMask, nullptr, out.zoneCostBase);
    ComputeZoneCost(rules, dims, barrier, landmass, coastDistance, ridgeMask, barrierMask, riverMask, out.zoneCost);
    if (landmass == nullptr || coastDistance == nullptr)
    {
        return;
    }

    // Thresholds from the main landmass base costs (rivers never move them).
    std::vector<float> sorted;
    for (int i = 0; i < n; ++i)
    {
        if (mainLandmass > 0 && landmass[i] == static_cast<uint32_t>(mainLandmass))
        {
            sorted.push_back(out.zoneCostBase[i]);
        }
    }
    std::sort(sorted.begin(), sorted.end());
    const int m = static_cast<int>(sorted.size());
    dbg.mainCells = m;
    if (m == 0)
    {
        dbg.t1 = 1.0f;
        dbg.t2 = 1.0f;
    }
    else
    {
        const int idx1 = std::max(0, static_cast<int>(std::floor(static_cast<double>(zones.FirstShare()) * m)) - 1);
        dbg.t1 = sorted[static_cast<size_t>(std::min(idx1, m - 1))];
        dbg.t2 = dbg.t1;
        const int k = static_cast<int>(std::ceil(static_cast<double>(zones.LastShare()) * m));
        if (k > 0)
        {
            const float t2 = sorted[static_cast<size_t>(std::max(0, m - k))];
            if (t2 > dbg.t1)
            {
                dbg.hasAncient = true;
                dbg.t2 = t2;
            }
            else
            {
                const auto it = std::upper_bound(sorted.begin(), sorted.end(), dbg.t1);
                if (it != sorted.end())
                {
                    dbg.hasAncient = true;
                    dbg.t2 = *it;
                }
            }
        }
    }

    const uint32_t minDepth = static_cast<uint32_t>(std::max(rules.kAncientMinDepth, 0));
    for (int i = 0; i < n; ++i)
    {
        if (landmass[i] == 0u)
        {
            continue;
        }
        const float cost = out.zoneCost[i];
        DepthZone z = 2;   // the middle zone
        if (cost <= dbg.t1)
        {
            z = 1;   // the first zone
        }
        else if (dbg.hasAncient && landmass[i] == static_cast<uint32_t>(mainLandmass) && cost >= dbg.t2 &&
                 coastDistance[i] >= minDepth)
        {
            z = zones.Last();   // the innermost zone
        }
        out.depthZone[i] = static_cast<uint32_t>(z);
    }
}

} // namespace WorldGeneration
} // namespace CoW
