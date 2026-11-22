// S-1 Terrain Generator: Stage 8 (Strata). See TerrainStrata.h.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma fp_contract(off)

#include <DraoiHexWorldTerrain/TerrainStrata.h>

#include <DiaPathfinding/GridSearch.h>
#include <DiaPathfinding/HexNeighbours.h>
#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace Draoi {
namespace HexWorld {

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

void ComputeStrataCosts(const TerrainRules& rules, GridDimensions dims, float barrier, const uint32_t* landmass,
                        const uint32_t* coastDistance, const float* ridgeMask, const float* barrierMask,
                        const uint32_t* riverMask, float* zoneCostBase, float* zoneCost)
{
    ComputeZoneCost(rules, dims, barrier, landmass, coastDistance, ridgeMask, barrierMask, nullptr, zoneCostBase);
    ComputeZoneCost(rules, dims, barrier, landmass, coastDistance, ridgeMask, barrierMask, riverMask, zoneCost);
}

void BandStrata(const Draoi::HexWorld::StratumSetSpec& set, GridDimensions dims, int mainLandmass, const uint32_t* landmass,
                const uint32_t* coastDistance, const float* zoneCostBase, const float* zoneCost, StratumIndex* stratum,
                Stage8Debug& dbg)
{
    dbg = Stage8Debug{};
    const int w = std::max(dims.width, 0);
    const int h = std::max(dims.height, 0);
    const int n = w * h;
    if (n == 0 || stratum == nullptr)
    {
        return;
    }
    std::fill(stratum, stratum + n, kNoStratum);
    const uint32_t count = std::min(set.count, Draoi::HexWorld::StratumTable::kMaxStrata);
    if (count == 0 || landmass == nullptr || coastDistance == nullptr || zoneCostBase == nullptr || zoneCost == nullptr)
    {
        return;
    }

    // Cuts from the main landmass base costs (rivers never move them).
    std::vector<float> sorted;
    for (int i = 0; i < n; ++i)
    {
        if (mainLandmass > 0 && landmass[i] == static_cast<uint32_t>(mainLandmass))
        {
            sorted.push_back(zoneCostBase[i]);
        }
    }
    std::sort(sorted.begin(), sorted.end());
    const int m = static_cast<int>(sorted.size());
    dbg.mainCells = m;
    const uint32_t endCuts = count >= 2 ? count - 2 : 0;   // end-inclusive cuts: strata 1..N-2
    dbg.cutCount = count >= 2 ? count - 1 : 0;
    bool hasLast = false;
    if (m == 0)
    {
        for (uint32_t i = 0; i < dbg.cutCount; ++i)
        {
            dbg.cuts[i] = 1.0f;
        }
        dbg.t1 = 1.0f;
        dbg.t2 = 1.0f;
    }
    else
    {
        float previous = sorted.front();   // the baseline the next cut must exceed (the lowest cost until a cut exists)
        bool collapsed = false;
        double cumulative = 0.0;
        for (uint32_t k = 1; k <= endCuts; ++k)
        {
            const float share = set.strata[k - 1].share;
            cumulative += static_cast<double>(share);
            float cut = previous;
            if (!collapsed && !(k >= 2 && share <= 0.0f))
            {
                const int idx = std::max(0, static_cast<int>(std::floor(cumulative * m)) - 1);
                cut = sorted[static_cast<size_t>(std::min(idx, m - 1))];
                if (k >= 2 && cut <= previous)
                {
                    const auto it = std::upper_bound(sorted.begin(), sorted.end(), previous);
                    if (it != sorted.end()) { cut = *it; }
                    else { cut = previous; collapsed = true; }
                }
            }
            dbg.cuts[k - 1] = cut;
            previous = cut;
        }
        if (count >= 2)
        {
            float last = previous;
            const int take = static_cast<int>(std::ceil(static_cast<double>(set.strata[count - 1].share) * m));
            if (!collapsed && take > 0)
            {
                const float t = sorted[static_cast<size_t>(std::max(0, m - take))];
                if (t > previous)
                {
                    hasLast = true;
                    last = t;
                }
                else
                {
                    const auto it = std::upper_bound(sorted.begin(), sorted.end(), previous);
                    if (it != sorted.end())
                    {
                        hasLast = true;
                        last = *it;
                    }
                }
            }
            dbg.cuts[count - 2] = last;
        }
        dbg.t1 = count >= 3 ? dbg.cuts[0] : sorted.front();
        dbg.t2 = count >= 2 ? dbg.cuts[count - 2] : dbg.t1;
    }
    dbg.hasAncient = hasLast;

    auto allowed = [&](uint32_t k, int cell) -> bool
    {
        const Draoi::HexWorld::StratumSpec& z = set.strata[k - 1];
        if (z.mainLandmassOnly && landmass[cell] != static_cast<uint32_t>(mainLandmass)) { return false; }
        return coastDistance[cell] >= static_cast<uint32_t>(std::max(z.minCoastDepth, 0));
    };
    for (int i = 0; i < n; ++i)
    {
        if (landmass[i] == 0u)
        {
            continue;
        }
        const float cost = zoneCost[i];
        uint32_t k = 0;
        for (uint32_t j = 1; j <= endCuts && k == 0; ++j)
        {
            if (cost <= dbg.cuts[j - 1]) { k = j; }
        }
        if (k == 0)
        {
            k = (count >= 2 && hasLast && cost >= dbg.cuts[count - 2]) ? count : (count >= 2 ? count - 1 : 1);
        }
        while (k > 1 && !allowed(k, i))
        {
            --k;
        }
        stratum[i] = static_cast<StratumIndex>(k);
    }
}

void ComputeStage8(const TerrainRules& rules, const Draoi::HexWorld::StratumSetSpec& set, GridDimensions dims, float barrier, int mainLandmass,
                   const uint32_t* landmass, const uint32_t* coastDistance, const float* ridgeMask,
                   const float* barrierMask, const uint32_t* riverMask, Stage8Layers out, Stage8Debug& dbg)
{
    dbg = Stage8Debug{};
    const int n = std::max(dims.width, 0) * std::max(dims.height, 0);
    if (n == 0 || out.zoneCostBase == nullptr || out.zoneCost == nullptr || out.stratum == nullptr)
    {
        return;
    }
    ComputeStrataCosts(rules, dims, barrier, landmass, coastDistance, ridgeMask, barrierMask, riverMask, out.zoneCostBase, out.zoneCost);
    std::vector<StratumIndex> banded(static_cast<size_t>(n), kNoStratum);
    BandStrata(set, dims, mainLandmass, landmass, coastDistance, out.zoneCostBase, out.zoneCost, banded.data(), dbg);
    for (int i = 0; i < n; ++i)
    {
        out.stratum[i] = static_cast<uint32_t>(banded[static_cast<size_t>(i)]);
    }
}

void ComputeStage8(const TerrainRules& rules, const Draoi::HexWorld::StratumTable& strata, GridDimensions dims, float barrier, int mainLandmass,
                   const uint32_t* landmass, const uint32_t* coastDistance, const float* ridgeMask,
                   const float* barrierMask, const uint32_t* riverMask, Stage8Layers out, Stage8Debug& dbg)
{
    static const Draoi::HexWorld::StratumSetSpec kNoSet;
    const Draoi::HexWorld::StratumSetSpec* depth = strata.Depth();
    ComputeStage8(rules, depth != nullptr ? *depth : kNoSet, dims, barrier, mainLandmass, landmass, coastDistance, ridgeMask,
                  barrierMask, riverMask, out, dbg);
}

} // namespace HexWorld
} // namespace Draoi
