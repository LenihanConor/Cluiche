// S-1 Terrain Generator: Stage 9 (Valleys). See TerrainValleys.h.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma fp_contract(off)

#include <Shared/WorldGeneration/TerrainValleys.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

float ValleyCarveSize(const TerrainRules& rules, float acc, float threshold)
{
    const float full = std::max(rules.kCarveFullAcc(), 1.0001f);
    float t = 0.0f;
    if (threshold > 0.0f && acc > threshold)
    {
        t = std::log(acc / threshold) / std::log(full);
    }
    t = std::min(std::max(t, 0.0f), 1.0f);
    return 0.4f + 0.6f * t;
}

void ComputeStage9(const TerrainRules& rules, GridDimensions dims, float valleyDepth, float threshold,
                   const float* elevationRaw, const uint32_t* flowDirection, const float* flowAccumulation,
                   const uint32_t* riverMask, const uint32_t* lakeMask, const float* lakeSurface, int lakeCount,
                   Stage9Layers out, Stage9Debug& dbg)
{
    dbg = Stage9Debug{};
    const int w = std::max(dims.width, 0);
    const int h = std::max(dims.height, 0);
    const int n = w * h;
    if (n == 0 || elevationRaw == nullptr || out.valleyCarve == nullptr || out.elevation == nullptr)
    {
        return;
    }

    const HexTopology topo(w, h);
    const float coast = rules.kCoastElev();
    const float maxCarve = valleyDepth * rules.kCarveMax();

    auto isOcean = [&](int c) { return elevationRaw[c] <= 0.0f; };
    auto isLake = [&](int c) { return lakeMask != nullptr && lakeMask[c] != 0u; };
    auto surfaceOf = [&](int c) -> float
    {
        const uint32_t id = lakeMask[c];
        return (lakeSurface != nullptr && id >= 1u && static_cast<int>(id) <= lakeCount) ? lakeSurface[id - 1] : elevationRaw[c];
    };
    auto downstream = [&](int c) -> int
    {
        if (flowDirection == nullptr || flowDirection[c] > 5u)
        {
            return -1;
        }
        const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), flowDirection[c]);
        return nb == kNoCell ? -1 : static_cast<int>(nb);
    };

    // 1. Carve per river cell (lake and ocean cells are never river cells), then the neighbour carve.
    std::vector<float> riverCarve(static_cast<size_t>(n), 0.0f);
    std::vector<uint8_t> river(static_cast<size_t>(n), 0);
    for (int c = 0; c < n; ++c)
    {
        if (riverMask != nullptr && riverMask[c] != 0u && !isOcean(c) && !isLake(c))
        {
            river[c] = 1;
            ++dbg.riverCells;
            const float acc = flowAccumulation != nullptr ? flowAccumulation[c] : 0.0f;
            riverCarve[c] = maxCarve * ValleyCarveSize(rules, acc, threshold);
        }
    }
    for (int c = 0; c < n; ++c)
    {
        float carve = 0.0f;
        if (river[c])
        {
            carve = riverCarve[c];
        }
        else if (dbg.riverCells > 0 && !isOcean(c) && !isLake(c))
        {
            float d1 = 0.0f;
            bool any1 = false;
            uint32_t ring1[6];
            for (uint32_t d = 0; d < 6; ++d)
            {
                ring1[d] = topo.NeighbourAt(static_cast<uint32_t>(c), d);
                if (ring1[d] != kNoCell && river[ring1[d]])
                {
                    any1 = true;
                    d1 = std::max(d1, riverCarve[ring1[d]]);
                }
            }
            if (any1)
            {
                carve = 0.5f * d1;
            }
            else
            {
                // No river within one step, so any river among the neighbours' neighbours is exactly two away.
                float d2 = 0.0f;
                for (uint32_t d = 0; d < 6; ++d)
                {
                    if (ring1[d] == kNoCell) { continue; }
                    for (uint32_t e = 0; e < 6; ++e)
                    {
                        const uint32_t nb = topo.NeighbourAt(ring1[d], e);
                        if (nb != kNoCell && river[nb])
                        {
                            d2 = std::max(d2, riverCarve[nb]);
                        }
                    }
                }
                carve = 0.2f * d2;
            }
        }
        out.valleyCarve[c] = carve;
        if (carve > 0.0f)
        {
            ++dbg.carvedCells;
            dbg.maxCarve = std::max(dbg.maxCarve, carve);
        }
    }

    // 2. New elevation (ocean 0, lakes at their surface, land clamped to the coast).
    for (int c = 0; c < n; ++c)
    {
        if (isOcean(c))
        {
            out.elevation[c] = 0.0f;
        }
        else if (isLake(c))
        {
            out.elevation[c] = surfaceOf(c);
        }
        else
        {
            out.elevation[c] = std::max(coast, elevationRaw[c] - out.valleyCarve[c]);
        }
    }
    if (dbg.riverCells == 0)
    {
        return;
    }

    // 3. Lake floors, memoised along the flow path (-1 = not yet known; real floors are > 0).
    std::vector<float> floorOf(static_cast<size_t>(n), -1.0f);
    std::vector<int> path;
    for (int c = 0; c < n; ++c)
    {
        if (!river[c] || floorOf[c] >= 0.0f)
        {
            continue;
        }
        path.clear();
        float fl = coast;
        int cur = c;
        for (int step = 0; step <= n; ++step)
        {
            if (cur < 0 || isOcean(cur)) { fl = coast; break; }
            if (isLake(cur)) { fl = surfaceOf(cur); break; }
            if (floorOf[cur] >= 0.0f) { fl = floorOf[cur]; break; }
            path.push_back(cur);
            cur = downstream(cur);
        }
        for (int p : path) { floorOf[p] = fl; }
    }
    for (int c = 0; c < n; ++c)
    {
        if (river[c] && out.elevation[c] < floorOf[c])
        {
            out.elevation[c] = floorOf[c];
            ++dbg.raisedByFloor;
        }
    }

    // 4. Downhill walk over river and lake cells in increasing accumulation order.
    std::vector<int> order;
    for (int c = 0; c < n; ++c)
    {
        if (river[c] || (isLake(c) && !isOcean(c))) { order.push_back(c); }
    }
    std::sort(order.begin(), order.end(), [&](int a, int b)
    {
        const float fa = flowAccumulation != nullptr ? flowAccumulation[a] : 0.0f;
        const float fb = flowAccumulation != nullptr ? flowAccumulation[b] : 0.0f;
        return fa != fb ? fa < fb : a < b;
    });
    for (int c : order)
    {
        const int ds = downstream(c);
        if (ds < 0 || !river[ds]) { continue; }
        const float v = std::max(floorOf[ds], std::min(out.elevation[ds], out.elevation[c]));
        if (v < out.elevation[ds]) { ++dbg.loweredByWalk; }
        out.elevation[ds] = v;
    }
}

} // namespace WorldGeneration
} // namespace CoW
