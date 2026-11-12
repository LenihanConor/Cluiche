// S-1 World Generation map view: pure overlay geometry (rivers, selection and neighbour outlines, region edges,
// site markers).
// No drawing here; the view host turns these into IDebugDraw calls.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#pragma once

#include <Shared/WorldGeneration/Sites.h>
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

// Region edges: the shared hex edge of every in-bounds neighbour pair whose Cell::region differs, once per pair, in
// ascending (cell, direction) order. Region 0 counts as its own value, so coasts (region vs ocean) are drawn and the map
// border is not. Empty when no cell has a region (regions not fresh) or when the world and grid dimensions disagree.
std::vector<LineSegment> BuildRegionEdges(const WorldStructure& world, const WorldHexGrid& grid);

// Outline of one site marker, every point within 'radius' of 'centre'. Dot: small hex with spokes; Diamond / Square /
// Triangle: polygons; Star: 5-point star; Ring: 12-gon at full radius.
void AppendSiteMarker(std::vector<LineSegment>& out, Dia::Maths::Vector2D centre, float radius, SiteShape shape);

// Markers for every site of one kind (WorldStructure::sites, ascending cell), drawn generically from the kind's shape.
// Empty for an unknown kind or when the world and grid dimensions disagree.
std::vector<LineSegment> BuildSiteMarkers(const WorldStructure& world, const WorldHexGrid& grid, uint32_t kindCrc,
                                          SiteShape shape, float radius);

} // namespace WorldGeneration
} // namespace CoW
