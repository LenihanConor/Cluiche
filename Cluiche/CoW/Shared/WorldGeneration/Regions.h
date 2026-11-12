// S-1 Regions, Nodes and Site Hook: region records and table.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/regions-and-sites.md
#pragma once

#include "TerrainEnums.h"

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

// RegionId / kNoRegion are declared in TerrainEnums.h (Cell needs them; avoids a circular include).

struct RegionRecord
{
    static constexpr unsigned kMaxNeighbours = 16;

    RegionId   id           = kNoRegion;
    DepthZone  zone         = DepthZone::None;      // majority zone for island regions
    uint8_t    landmass     = 0;
    bool       isIsland     = false;
    Biome      primaryBiome = Biome::Ocean;
    float      purity       = 0.0f;                 // [0, 1]
    uint16_t   cellCount    = 0;
    uint32_t   centreCell   = 0;
    Dia::Core::Containers::DynamicArrayC<RegionId, kMaxNeighbours> neighbours;   // ascending; asserts if exceeded

    bool operator==(const RegionRecord&) const = default;
};

struct RegionTable
{
    // DynamicArrayC asserts at capacity; callers add through AddRegion, which refuses instead.
    static constexpr unsigned kMaxRegions = 64;

    Dia::Core::Containers::DynamicArrayC<RegionRecord, kMaxRegions> regions;   // index = id - 1
    uint16_t  absorbedCells       = 0;                                         // zone-mismatched cells, excluding fragment cells (AC3)
    uint16_t  fragmentCells       = 0;                                         // cells of tiny zone fragments absorbed into another zone's region
    uint8_t   mainlandRegionCount = 0;
    DepthZone limitedBy           = DepthZone::None;                           // None unless terrain-limited

    // False (table unchanged) when full.
    bool AddRegion(const RegionRecord& r)
    {
        if (regions.IsFull()) { return false; }
        regions.Add(r);
        return true;
    }

    // Symmetric lookup via a's neighbour list or b's; false for self, 0 or unknown ids.
    bool AreAdjacent(RegionId a, RegionId b) const
    {
        if (a == b) { return false; }
        const RegionRecord* ra = Find(a);
        const RegionRecord* rb = Find(b);
        if (!ra || !rb) { return false; }
        for (unsigned i = 0; i < ra->neighbours.Size(); ++i)
        {
            if (ra->neighbours[i] == b) { return true; }
        }
        for (unsigned i = 0; i < rb->neighbours.Size(); ++i)
        {
            if (rb->neighbours[i] == a) { return true; }
        }
        return false;
    }

    // nullptr for id 0 or an id with no record.
    const RegionRecord* Find(RegionId id) const
    {
        if (id == kNoRegion || id > regions.Size()) { return nullptr; }
        return &regions[static_cast<unsigned>(id - 1)];
    }

    bool operator==(const RegionTable& o) const
    {
        return regions == o.regions && absorbedCells == o.absorbedCells && fragmentCells == o.fragmentCells &&
               mainlandRegionCount == o.mainlandRegionCount && limitedBy == o.limitedBy;
    }
};

} // namespace WorldGeneration
} // namespace CoW
