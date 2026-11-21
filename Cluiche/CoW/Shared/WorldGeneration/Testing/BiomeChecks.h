// S-1 Biome Classification test support: validity checks over biome arrays (AC3/AC4/AC5), shared with Feature 9.
// Header-only; arrays are row-major, index = y * width + x, biome holds Biome enum values.
#pragma once

#include <Shared/WorldGeneration/BiomeClassify.h>
#include <Shared/WorldGeneration/BiomeTypes.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>

#include <DiaPathfinding/HexTopology.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

// Cells whose biome is not valid in their depth zone. Volcanic cells at a vent (Volcanic flag or terrain Volcano)
// are exempt (AC3 vent island).
inline std::vector<int> FindZoneViolations(const Stage14Inputs& in, const uint32_t* biome)
{
    std::vector<int> bad;
    const int        n = in.dims.width * in.dims.height;
    const Biome      volcanic = ::CoW::World::Testing::LegacyB("Biome.Volcanic");
    for (int c = 0; c < n; ++c)
    {
        const Biome b = static_cast<Biome>(biome[c]);
        if (b == volcanic && IsVentCell(in, c))
        {
            continue;
        }
        if (!IsBiomeValidInZone(b, static_cast<DepthZone>(in.depthZone[c])))
        {
            bad.push_back(c);
        }
    }
    return bad;
}

// Each forbidden neighbouring pair once, as (lower index, higher index).
inline std::vector<std::pair<int, int>> FindForbiddenPairs(GridDimensions dims, const uint32_t* biome)
{
    std::vector<std::pair<int, int>> pairs;
    if (dims.width <= 0 || dims.height <= 0)
    {
        return pairs;
    }
    const Dia::Pathfinding::HexTopology topo(dims.width, dims.height);
    const int                           n = dims.width * dims.height;
    for (int c = 0; c < n; ++c)
    {
        for (uint32_t d = 0; d < 6u; ++d)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
            if (nb == Dia::Pathfinding::kNoCell || static_cast<int>(nb) <= c)
            {
                continue;
            }
            if (AreBiomesForbidden(static_cast<Biome>(biome[c]), static_cast<Biome>(biome[nb])))
            {
                pairs.emplace_back(c, static_cast<int>(nb));
            }
        }
    }
    return pairs;
}

// WetlandDelta cells that break AC5: not eligible, or not next to a moisture source (coast, lake, river cell,
// or another WetlandDelta).
inline std::vector<int> CheckWetlandRule(const TerrainRules& rules, const Stage14Inputs& in, const uint32_t* biome)
{
    std::vector<int> bad;
    if (in.dims.width <= 0 || in.dims.height <= 0)
    {
        return bad;
    }
    const Dia::Pathfinding::HexTopology topo(in.dims.width, in.dims.height);
    const int                           n = in.dims.width * in.dims.height;
    const Biome                         wetland = ::CoW::World::Testing::LegacyB("Biome.Wetland");
    for (int c = 0; c < n; ++c)
    {
        if (static_cast<Biome>(biome[c]) != wetland)
        {
            continue;
        }
        bool source = (in.terrainFlags[c] & TerrainFlag::Coast) != 0;
        for (uint32_t d = 0; d < 6u && !source; ++d)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
            if (nb == Dia::Pathfinding::kNoCell)
            {
                continue;
            }
            const uint32_t ws = in.waterSource[nb];
            source = ws == static_cast<uint32_t>(WaterSource::Lake) || ws == static_cast<uint32_t>(WaterSource::River) ||
                     ws == static_cast<uint32_t>(WaterSource::Ocean) ||
                     static_cast<Biome>(biome[nb]) == wetland;
        }
        if (!IsWetlandEligible(rules, in, c) || !source)
        {
            bad.push_back(c);
        }
    }
    return bad;
}

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
