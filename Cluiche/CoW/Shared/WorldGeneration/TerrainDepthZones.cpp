// S-1 Terrain Generator: Stage 8 (DepthZones). See TerrainDepthZones.h.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma fp_contract(off)

#include <Shared/WorldGeneration/TerrainDepthZones.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

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
    const double ridgeCost = rules.kRidgeCost();
    const double barrierCost = static_cast<double>(rules.kBarrierCost()) * static_cast<double>(barrier);
    const double corridor = rules.kRiverCorridor();

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

    constexpr double kInf = 1e30;
    std::vector<double> dist(static_cast<size_t>(n), kInf);
    using Entry = std::pair<double, int>;   // (cost, cell index): a strict total order, so pops are deterministic
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> heap;
    for (int i = 0; i < n; ++i)
    {
        if (landmass[i] != 0u && coastDistance[i] == 1u)
        {
            dist[i] = 1.0;   // source cost 1 regardless of the cell's own ridge/barrier terms
            heap.emplace(1.0, i);
        }
    }
    while (!heap.empty())
    {
        const Entry top = heap.top();
        heap.pop();
        const int c = top.second;
        if (top.first > dist[c])
        {
            continue;
        }
        for (uint32_t d = 0; d < 6u; ++d)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
            if (nb == kNoCell || landmass[nb] == 0u)
            {
                continue;
            }
            const double nd = top.first + entryCost(static_cast<int>(nb));
            if (nd < dist[nb])
            {
                dist[nb] = nd;
                heap.emplace(nd, static_cast<int>(nb));
            }
        }
    }
    for (int i = 0; i < n; ++i)
    {
        if (landmass[i] != 0u)
        {
            out[i] = dist[i] >= kInf ? 1.0f : static_cast<float>(dist[i]);
        }
    }
}

void ComputeStage8(const TerrainRules& rules, GridDimensions dims, float barrier, int mainLandmass,
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
    std::fill(out.depthZone, out.depthZone + n, static_cast<uint32_t>(DepthZone::None));
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
        const int idx1 = std::max(0, static_cast<int>(std::floor(static_cast<double>(rules.kFirstShoresShare()) * m)) - 1);
        dbg.t1 = sorted[static_cast<size_t>(std::min(idx1, m - 1))];
        dbg.t2 = dbg.t1;
        const int k = static_cast<int>(std::ceil(static_cast<double>(rules.kAncientShare()) * m));
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

    const uint32_t minDepth = static_cast<uint32_t>(std::max(rules.GetInt(Rule::kAncientMinDepth), 0));
    for (int i = 0; i < n; ++i)
    {
        if (landmass[i] == 0u)
        {
            continue;
        }
        const float cost = out.zoneCost[i];
        DepthZone z = DepthZone::WildReaches;
        if (cost <= dbg.t1)
        {
            z = DepthZone::FirstShores;
        }
        else if (dbg.hasAncient && landmass[i] == static_cast<uint32_t>(mainLandmass) && cost >= dbg.t2 &&
                 coastDistance[i] >= minDepth)
        {
            z = DepthZone::AncientInteriors;
        }
        out.depthZone[i] = static_cast<uint32_t>(z);
    }
}

} // namespace WorldGeneration
} // namespace CoW
