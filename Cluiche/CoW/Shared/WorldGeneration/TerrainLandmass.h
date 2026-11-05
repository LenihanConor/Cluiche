// S-1 Terrain Generator: Stage 1 (Landmass), part A - steps 1-5 of the spec.
// Continent blobs + peninsula lobes -> potential M (the landPotential layer), land budget L,
// island budget K and the reference mainland (the L highest-M cells).
//
// PART-A SEAM: Stage1Debug is temporary-but-public. It ranks every eligible cell by M (desc,
// ties by lowest index) so the top-n cells can be taken for any n. Task 5 (island placement,
// mainland clean-up per step 7, landmass labelling, coastDistance) replaces/extends it; until
// then "mainland" is simply the top cells with NO clean-up applied.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>
#include <vector>

namespace CoW {
namespace WorldGeneration {

struct SeedFields;

struct Stage1Debug
{
    int  cellCount = 0;            // N
    int  eligibleCount = 0;        // cells with finite M (not in the border exclusion)
    int  landTargetRequested = 0;  // round(Land * N)
    int  landTarget = 0;           // L = requested clamped to eligibleCount
    bool landClamped = false;      // requested > eligibleCount
    int  islandBudget = 0;         // K (0 when Islands == 0)
    std::vector<int> rank;         // eligible cell indices, M descending, ties lowest index first

    // 1 for the n highest-M cells (n clamped to [0, eligibleCount]), else 0. Size cellCount.
    std::vector<uint8_t> TopMask(int n) const;

    // M_ref (part A: top L cells, no clean-up).
    std::vector<uint8_t> ReferenceMainland() const { return TopMask(landTarget); }

    // Mainland once placedIslandCells island cells exist: the top (L - n) cells.
    std::vector<uint8_t> Mainland(int placedIslandCells) const { return TopMask(landTarget - placedIslandCells); }
};

// Steps 1-5. landPotential must hold dims.width*dims.height floats (written; -infinity = excluded).
void ComputeStage1(const TerrainRules& rules, const WorldShapeParams& shape, GridDimensions dims,
                   const SeedFields& seeds, float* landPotential, Stage1Debug& out);

} // namespace WorldGeneration
} // namespace CoW
