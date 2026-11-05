// S-1 Terrain Generator: Stage 10 (Classify) and Stage 11 (Latitude). See TerrainClassify.h.
#pragma fp_contract(off)

#include <Shared/WorldGeneration/TerrainClassify.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace CoW {
namespace WorldGeneration {

using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

void ComputeStage10(const TerrainRules& rules, GridDimensions dims, const uint32_t* landmass,
                    const uint32_t* coastDistance, const uint32_t* volcanic, const float* ridgeMask,
                    const uint32_t* passMask, const float* band, const uint32_t* riverMask,
                    const uint32_t* lakeMask, const float* elevation, Stage10Layers out)
{
    const int w = std::max(dims.width, 0);
    const int h = std::max(dims.height, 0);
    const int n = w * h;
    if (n == 0 || landmass == nullptr || elevation == nullptr || out.slope == nullptr || out.terrainType == nullptr
        || out.waterSource == nullptr || out.terrainFlags == nullptr)
    {
        return;
    }

    const HexTopology topo(w, h);
    auto isOcean = [&](int c) { return landmass[c] == 0u; };

    // Slope: largest elevation difference to non-ocean neighbours; ocean cells themselves stay 0.
    for (int c = 0; c < n; ++c)
    {
        float s = 0.0f;
        if (!isOcean(c))
        {
            for (uint32_t d = 0; d < 6; ++d)
            {
                const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
                if (nb != kNoCell && !isOcean(static_cast<int>(nb)))
                {
                    s = std::max(s, std::fabs(elevation[c] - elevation[nb]));
                }
            }
        }
        out.slope[c] = s;
    }

    // Volcanic influence: multi-source BFS from every vent out to kVentRadius (hex distance, through any cell).
    const int ventRadius = std::max(rules.GetInt(Rule::kVentRadius), 0);
    std::vector<uint8_t> nearVent(static_cast<size_t>(n), 0);
    if (volcanic != nullptr)
    {
        std::vector<int> queue;
        std::vector<int> depth(static_cast<size_t>(n), -1);
        for (int c = 0; c < n; ++c)
        {
            if (volcanic[c] != 0u)
            {
                depth[c] = 0;
                nearVent[c] = 1;
                queue.push_back(c);
            }
        }
        for (size_t head = 0; head < queue.size(); ++head)
        {
            const int c = queue[head];
            if (depth[c] >= ventRadius)
            {
                continue;
            }
            for (uint32_t d = 0; d < 6; ++d)
            {
                const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
                if (nb != kNoCell && depth[nb] < 0)
                {
                    depth[nb] = depth[c] + 1;
                    nearVent[nb] = 1;
                    queue.push_back(static_cast<int>(nb));
                }
            }
        }
    }

    const uint32_t shelf = static_cast<uint32_t>(std::max(rules.GetInt(Rule::kShelfWidth), 0));
    const float cliff = rules.kCliffSlope();
    const float steepSlope = rules.kSteepSlope();
    const float peak = rules.kPeakElev();
    const float mountain = rules.kMountainElev();
    const float highland = rules.kHighlandElev();
    const float hill = rules.kHillElev();
    const float hillSlope = rules.kHillSlope();
    const float ridgeMin = rules.kRidgeCellMin();

    for (int c = 0; c < n; ++c)
    {
        const bool ocean = isOcean(c);
        const bool lake = !ocean && lakeMask != nullptr && lakeMask[c] != 0u;
        const bool river = !ocean && !lake && riverMask != nullptr && riverMask[c] != 0u;
        const bool vent = volcanic != nullptr && volcanic[c] != 0u;
        const bool pass = passMask != nullptr && passMask[c] != 0u;
        const float e = elevation[c];
        const float s = out.slope[c];

        TerrainType type = TerrainType::Lowland;
        if (ocean)
        {
            const uint32_t cd = coastDistance != nullptr ? coastDistance[c] : 0u;
            type = cd >= shelf ? TerrainType::DeepWater : TerrainType::ShallowWater;
        }
        else if (lake)                              { type = TerrainType::Lake; }
        else if (river)                             { type = TerrainType::River; }
        else if (vent)                              { type = TerrainType::Volcano; }
        else if (s >= cliff)                        { type = TerrainType::Cliff; }
        else if (pass)                              { type = TerrainType::Pass; }
        else if (e >= peak)                         { type = TerrainType::Peak; }
        else if (e >= mountain || s >= steepSlope)  { type = TerrainType::Mountain; }
        else if (e >= highland)                     { type = TerrainType::Highland; }
        else if (e >= hill || s >= hillSlope)       { type = TerrainType::Hills; }
        out.terrainType[c] = static_cast<uint32_t>(type);

        out.waterSource[c] = static_cast<uint32_t>(ocean ? WaterSource::Ocean : lake ? WaterSource::Lake
                                                    : river ? WaterSource::River : WaterSource::None);

        uint32_t flags = 0;
        if (!ocean)
        {
            for (uint32_t d = 0; d < 6; ++d)
            {
                const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
                if (nb != kNoCell && isOcean(static_cast<int>(nb)))
                {
                    flags |= TerrainFlag::Coast;
                    break;
                }
            }
            if (nearVent[c] != 0)
            {
                flags |= TerrainFlag::Volcanic;
            }
        }
        if (ridgeMask != nullptr && ridgeMask[c] >= ridgeMin) { flags |= TerrainFlag::Ridge; }
        if (band != nullptr && band[c] >= 0.5f)               { flags |= TerrainFlag::Barrier; }
        if (pass)                                             { flags |= TerrainFlag::Pass; }
        if (river && (flags & (TerrainFlag::Ridge | TerrainFlag::Barrier)) != 0)
        {
            flags |= TerrainFlag::RiverCrossesBarrier;
        }
        out.terrainFlags[c] = flags;
    }
}

float MaxCentreY(GridDimensions dims)
{
    const int w = std::max(dims.width, 1);
    const int h = std::max(dims.height, 1);
    const CellCoord maxYCell{ w > 1 ? 1 : 0, h - 1 };
    float maxY = HexTopology::CellCenter(maxYCell, 1.0f).y;
    if (maxY < 1e-6f)
    {
        maxY = 1.0f;
    }
    return maxY;
}

void ComputeStage11(GridDimensions dims, float northDeg, float southDeg, float* latitude)
{
    if (dims.width <= 0 || dims.height <= 0 || latitude == nullptr)
    {
        return;
    }
    const float maxY = MaxCentreY(dims);
    for (int y = 0; y < dims.height; ++y)
    {
        for (int x = 0; x < dims.width; ++x)
        {
            const float cy = HexTopology::CellCenter(CellCoord{ x, y }, 1.0f).y;
            const float latDeg = northDeg + (southDeg - northDeg) * cy / maxY;
            latitude[y * dims.width + x] = std::min(1.0f, std::fabs(latDeg) / 90.0f);
        }
    }
}

} // namespace WorldGeneration
} // namespace CoW
