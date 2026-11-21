// S-1 Terrain Generator: Stage 10 (Classify) and Stage 11 (Latitude).
//
// Stage 10 (reads stages 1,2,3,6,7,8,9):
//   slope(c) = largest |e(c) - e(n)| over neighbouring non-ocean (land or lake) cells, e = final elevation.
//     Ocean cells themselves get slope 0; land cells next to ocean ignore their ocean neighbours; a cell
//     with no non-ocean neighbour has slope 0.
//   ocean = landmass == 0; lake = lakeMask != 0 (lakes are land in the landmass layer); river = riverMask != 0;
//   vent = volcanic != 0; pass = passMask != 0.
//   terrainType: first matching row of
//     1 ocean, coastDistance >= kShelfWidth -> DeepWater     7 pass                      -> Pass
//     2 ocean                               -> ShallowWater  8 e >= kPeakElev            -> Peak
//     3 lake                                -> Lake          9 e >= kMountainElev or slope >= kSteepSlope -> Mountain
//     4 river                               -> River        10 e >= kHighlandElev        -> Highland
//     5 vent                                -> Volcano      11 e >= kHillElev or slope >= kHillSlope -> Hills
//     6 slope >= kCliffSlope                -> Cliff        12 otherwise                 -> Lowland
//   waterSource: Ocean / Lake / River by the same tests (lake wins over river), else None.
//   terrainFlags (TerrainFlag bits):
//     Coast = land cell with an ocean neighbour; Ridge = ridgeMask >= kRidgeCellMin; Barrier = band >= 0.5;
//     Pass = passMask != 0; Volcanic = non-ocean cell within hex distance kVentRadius of a vent (vents included;
//     ocean cells never carry it); RiverCrossesBarrier = river (not lake) cell with Ridge or Barrier.
//   band is the raw stage-3 barrier band (null = all zero).
//
// Stage 11 (grid only): latDeg(c) = north + (south - north) * cy(c) / maxCentreY, where cy is the cell centre's y
//   (HexTopology::CellCenter(c, 1).y) and maxCentreY is stage 0's map-space divisor (a degenerate grid whose
//   maxCentreY is ~0 uses 1). latitude = min(1, |latDeg| / 90); 0 = equator, 1 = pole (unsigned).
// Free functions over arrays so tests can use hand-built grids.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

struct Stage10Layers
{
    float*    slope = nullptr;
    uint32_t* terrainType = nullptr;    // TerrainType values
    uint32_t* waterSource = nullptr;    // WaterSource values
    uint32_t* terrainFlags = nullptr;   // TerrainFlag bits
};

// Writes every cell of out. Null inputs (other than landmass/elevation) read as all-zero.
void ComputeStage10(const TerrainRules& rules, GridDimensions dims, const uint32_t* landmass,
                    const uint32_t* coastDistance, const uint32_t* volcanic, const float* ridgeMask,
                    const uint32_t* passMask, const float* band, const uint32_t* riverMask,
                    const uint32_t* lakeMask, const float* elevation, Stage10Layers out);

// Stage 0's maxCentreY (largest cell-centre y on the grid, 1 if ~0).
float MaxCentreY(GridDimensions dims);

void ComputeStage11(GridDimensions dims, float northDeg, float southDeg, float* latitude);

} // namespace WorldGeneration
} // namespace CoW
