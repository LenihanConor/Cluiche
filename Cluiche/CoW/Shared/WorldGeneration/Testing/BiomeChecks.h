// S-1 Biome Classification test support: validity checks over biome arrays (AC3/AC4/AC5), shared with Feature 9.
// Header-only; arrays are row-major, index = y * width + x, biome holds Biome enum values.
#pragma once

#include <Shared/WorldGeneration/BiomeClassify.h>
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
inline std::vector<int> FindZoneViolations(const Stage14Inputs& in, const uint32_t* biome,
                                         const ::CoW::World::BiomeRegistry& reg = ::CoW::World::Testing::ShippedBiomeRegistry())
{
    std::vector<int> bad;
    const int        n = in.dims.width * in.dims.height;
    const ::CoW::World::BiomeIndex volcanic = ::CoW::World::Testing::B(reg, "Biome.Volcanic");
    for (int c = 0; c < n; ++c)
    {
        const ::CoW::World::BiomeIndex b = static_cast<::CoW::World::BiomeIndex>(biome[c]);
        if (b == volcanic && IsVentCell(in, c))
        {
            continue;
        }
        if (!reg.IsValidInStratum(b, static_cast<StratumIndex>(in.stratum[c])))
        {
            bad.push_back(c);
        }
    }
    return bad;
}

// Each forbidden neighbouring pair once, as (lower index, higher index).
inline std::vector<std::pair<int, int>> FindForbiddenPairs(GridDimensions dims, const uint32_t* biome,
                                                          const ::CoW::World::BiomeRegistry& reg = ::CoW::World::Testing::ShippedBiomeRegistry())
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
            if (reg.AreForbidden(static_cast<::CoW::World::BiomeIndex>(biome[c]), static_cast<::CoW::World::BiomeIndex>(biome[nb])))
            {
                pairs.emplace_back(c, static_cast<int>(nb));
            }
        }
    }
    return pairs;
}

// WetlandDelta cells that break AC5: not eligible, or not next to a moisture source (coast, lake, river cell,
// or another WetlandDelta).
inline std::vector<int> CheckWetlandRule(const TerrainRules& rules, const Stage14Inputs& in, const uint32_t* biome,
                                        const ::CoW::World::BiomeRegistry& reg = ::CoW::World::Testing::ShippedBiomeRegistry())
{
    std::vector<int> bad;
    if (in.dims.width <= 0 || in.dims.height <= 0)
    {
        return bad;
    }
    const Dia::Pathfinding::HexTopology topo(in.dims.width, in.dims.height);
    const int                           n = in.dims.width * in.dims.height;
    const ::CoW::World::BiomeIndex      wetland = ::CoW::World::Testing::B(reg, "Biome.Wetland");
    for (int c = 0; c < n; ++c)
    {
        if (static_cast<::CoW::World::BiomeIndex>(biome[c]) != wetland)
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
                     static_cast<::CoW::World::BiomeIndex>(biome[nb]) == wetland;
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
