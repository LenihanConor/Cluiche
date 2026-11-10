// S-1 World Generation map view: pure overlay geometry (rivers, selection and neighbour outlines).
// No drawing here; the view host turns these into IDebugDraw calls.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#pragma once

#include <Shared/WorldGeneration/TerrainTypes.h>
#include <Shared/WorldGeneration/WorldHexGrid.h>

#include <DiaMaths/Vector/Vector2D.h>

#include <vector>

namespace CoW {
namespace WorldGeneration {

struct LineSegment
{
    Dia::Maths::Vector2D a;
    Dia::Maths::Vector2D b;
};

// River cell centre -> downstream neighbour centre, one segment per River cell with a valid, in-bounds
// downstream direction (0..5, same convention as HexTopology::NeighbourAt). Empty when the world and grid
// dimensions disagree.
std::vector<LineSegment> BuildRiverSegments(const WorldStructure& world, const WorldHexGrid& grid);

struct NeighbourOutline
{
    int                       cell = -1;                      // selected cell, -1 when invalid
    std::vector<LineSegment>  selectedOutline;                // 6 hex edges of the selected cell
    std::vector<LineSegment>  neighbourOutlines;              // 6 edges per in-bounds neighbour
    Dia::Maths::Vector2D      labelAnchor[6];                 // neighbour centre, valid where hasLabel[d]
    bool                      hasLabel[6] = { false, false, false, false, false, false };
    int                       neighbourCell[6] = { -1, -1, -1, -1, -1, -1 };
};

NeighbourOutline BuildNeighbourOutline(const WorldHexGrid& grid, int cell);

// The 6 edges of the flat-top hex centred at 'centre' (corner k at angle 60*k degrees).
void AppendHexOutline(std::vector<LineSegment>& out, Dia::Maths::Vector2D centre, float cellSize);

} // namespace WorldGeneration
} // namespace CoW
