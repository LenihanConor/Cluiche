// S-1 Terrain Generator: Stage 2 (Mountains). Builds one mountain range (a spine that blends
// into a ring) on the main landmass, with passes cut through it.
//
// Units: all geometry (centre, reach, curve, widths, pass gaps) is in "cell units": the
// centre-to-centre pitch of adjacent cells is 1. A cell's position is
// CellCenter(coord, kCellSize) with kCellSize = 1/sqrt(3) (CellCenter at size 1 has pitch sqrt(3)).
//
// Stage 2 is a pure function of (seed fields, sliders, rules, the main landmass mask).
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma once

#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <cstdint>
#include <vector>

namespace CoW {
namespace WorldGeneration {

struct SeedFields;
struct Effective;

constexpr float kCellSize = 0.57735026919f;   // 1/sqrt(3): adjacent cell centres are 1 apart
constexpr int   kCurveSegments = 256;

struct Vec2 { float x = 0.0f, y = 0.0f; };

struct Stage2Debug
{
    bool  valid = false;          // false when there is no main landmass
    Vec2  centre;                 // C (cell units)
    int   centreCell = -1;        // cell containing C
    float theta = 0.0f;           // orientation, radians (degrees wrapped to [0,360) first)
    float offset = 0.0f;          // spine lateral offset (cells) = spineOffset * inradius
    float inradius = 0.0f;        // min reach over 24 directions
    float width = 0.0f;           // w (cells)
    float gap = 0.0f;             // g (cells)
    float spineToRing = 0.0f;
    float spineReach = 0.8f;
    float ringReach = 0.6f;
    float reachFwd = 0.0f;        // reach(theta)
    float reachBack = 0.0f;       // reach(theta + 180 deg)

    GridDimensions       dims;
    std::vector<uint8_t> mainMask;     // 1 on main landmass land
    std::vector<float>   rawRidge;     // max(0, 1 - d/w) on the main landmass, before group selection/passes
    std::vector<float>   curveArc;     // cumulative arc length at the kCurveSegments+1 polyline points
    float                curveLength = 0.0f;
    std::vector<float>   passT;        // accepted pass t values, in acceptance order
    int                  ridgeGroups = 0;   // connected groups of ridge cells in the final mask

    // Distance from C along direction phi (radians) to the first non-main-land cell (0.25-cell steps).
    float Reach(float phi) const;
    Vec2  Line(float t) const;
    Vec2  Ring(float t) const;
    Vec2  Curve(float t) const;        // P(t) = lerp(L, G, spineToRing)
    float ArcAt(float t) const;        // arc length along the polyline from t=0
};

// The cell centre in cell units.
Vec2 CellPosition(int x, int y);

// C: centroid of the mask's cell centres, or, if its cell is not in the mask, the nearest mask cell
// centre (ties by lowest index). Returns the cell containing C, -1 for an empty mask.
int FindMountainCentre(const std::vector<uint8_t>& mask, GridDimensions dims, Vec2& centre);

// Outputs are sized width*height (null skipped). mainMask is 1 on main-landmass land.
void ComputeStage2(const TerrainRules& rules, const WorldShapeParams& shape, GridDimensions dims,
                   const SeedFields& seeds, const std::vector<uint8_t>& mainMask,
                   float* ridgeMask, uint32_t* passMask, Stage2Debug& out, struct Effective& effective);

} // namespace WorldGeneration
} // namespace CoW
