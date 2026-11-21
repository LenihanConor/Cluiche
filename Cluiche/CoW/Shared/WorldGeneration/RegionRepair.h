// S-1 Regions: region partition, second half (repair, records, renumbering, write into the world).
// Takes the provisional ids from RegionPartition.h and produces the final Cell::region + WorldStructure::regions.
//
// Repair order (deterministic; all ties: lowest cell index, then lowest region id):
//   1. split pieces: a region in several connected pieces keeps its largest piece (ties: lowest min cell);
//      the other pieces go to the adjacent region with the longest shared border (same zone as the piece's
//      region first, else any zone);
//   2. regions==0 on land (tiny zone components, unzoned cells) are grouped into connected same-zone pieces and
//      absorbed the same way; a piece with no adjacent region waits for a neighbour piece to be absorbed first;
//   3. leftover pieces that never get an adjacent region (isolated) become ORPHAN regions, one per connected
//      group (counted in RegionBuild::orphanRegions). If the table is full they stay region 0 and
//      RegionBuild::overflow + unassignedLandCells report it (never silently dropped);
//   4. regions under kMinRegionCells merge (ascending size, then id) into the adjacent same-zone region with the
//      longest border, else the adjacent region with the longest border of any zone; one with no neighbour stays;
//   5. regions are renumbered 1..N by smallest cell index.
// Island regions (non-main landmass) are never split or merged. absorbedCells = cells of non-island regions
// whose depth zone differs from the region's zone, except cells of a tiny zone component
// (RegionGrowth::tinyComponents), which count as fragmentCells instead (AC3 amendment: absorbing a disconnected
// fragment is forced, so it measures the terrain, not the partitioner).
// limitedBy = the growth targets' value; if that is None and the repaired mainland has fewer than 10 regions, it
// becomes the zone with the largest (target - final) region deficit, ties zone order (AC4 amendment).
//
// Records: purity = majority-biome cells / ALL cells of the region (lakes count in the denominator); a region
// of only Lake cells reports Lake with purity 1 for those cells. cellCount saturates at 65535.
// centreCell: nearest region cell (world-space hex centres, ties lowest index) to the centroid, preferring
// non-Lake cells. neighbours: ascending ids sharing a hex edge; more than kMaxNeighbours keeps the lowest
// 16 and sets RegionBuild::neighbourOverflow (lists may then be asymmetric; RegionTable::AreAdjacent still
// answers symmetrically).
//
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/regions-and-sites.md (Region partition 5-6)
#pragma once

#include <Shared/WorldGeneration/RegionPartition.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

struct RegionBuild
{
    uint16_t regionCount             = 0;       // final regions written
    uint16_t splitPiecesMoved        = 0;       // step 1 pieces reassigned
    uint16_t looseLandPiecesAbsorbed = 0;       // step 2 pieces (tiny components + unzoned cells) absorbed
    uint16_t undersizedMerged        = 0;       // step 4 regions merged away
    uint16_t orphanRegions           = 0;       // step 3 regions created for isolated pieces
    uint32_t unassignedLandCells     = 0;       // land cells left at region 0 because the table was full
    bool     neighbourOverflow       = false;   // some region had more than kMaxNeighbours neighbours
    bool     overflow                = false;   // growth overflow, or orphans did not fit
};

// Recomputes every record's derived fields from the world's cells: cellCount, primaryBiome, purity, centreCell and
// neighbours (and mainlandRegionCount). Identity (id, zone, landmass, isIsland) and the table's absorbedCells,
// fragmentCells and limitedBy are left alone. Cells whose region id is 0 or beyond the table are ignored; a record
// with no cells gets cellCount 0, purity 0, the registry's Ocean-role biome, centre 0 ("Lake" in the purity rule is the
// Lake-role biome). Returns true if some region had more than
// kMaxNeighbours neighbours (lists then keep the lowest 16). This is the exact arithmetic FinishRegions uses, shared
// so world import (WorldJson) reproduces a generated world's summaries bit for bit.
bool RecomputeRegionRecords(WorldStructure& world, const World::BiomeRegistry& biomes);

// Clears previous region data, repairs `growth`, writes Cell::region for every cell and fills world.regions.
// No other Cell field is touched.
RegionBuild FinishRegions(WorldStructure& world, const TerrainRules& rules, const RegionGrowth& growth, const World::BiomeRegistry& biomes);

// BuildProvisionalRegions + FinishRegions.
RegionBuild BuildRegions(WorldStructure& world, const TerrainRules& rules, Dia::Maths::RandomStream& rng, const World::BiomeRegistry& biomes);

} // namespace WorldGeneration
} // namespace CoW
