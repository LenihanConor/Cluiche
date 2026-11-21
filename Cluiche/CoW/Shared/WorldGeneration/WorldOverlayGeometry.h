// S-1 World Generation map view: pure overlay geometry (rivers, selection and neighbour outlines, region edges,
// site markers).
// No drawing here; the view host turns these into IDebugDraw calls.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#pragma once

#include <Shared/WorldGeneration/GenerationReport.h>
#include <DraoiHexWorld/Sites.h>
#include <Shared/WorldGeneration/TerrainTypes.h>
#include <DraoiHexWorld/WorldHexGrid.h>

#include <DiaMaths/Vector/Vector2D.h>

#include <cstdint>
#include <span>
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

// ---- Plan worldgen-panel-rest task 3: coloured region edges, tagged site markers, spaced markers, evidence ----

// A region edge with its categorical colour: colourSlot = RegionPaletteSlot of the cell on the lower-index side (or of
// the other side when that one has no region), the same slot the Region colour mode paints.
struct RegionEdge
{
    LineSegment seg;
    RegionId    regionA    = kNoRegion;   // region of the lower-index cell
    RegionId    regionB    = kNoRegion;
    uint32_t    colourSlot = 0;
};
std::vector<RegionEdge> BuildColouredRegionEdges(const WorldStructure& world, const WorldHexGrid& grid);

// One site marker, tagged by kind so the drawer can filter per-kind toggles.
struct SiteMarker
{
    uint32_t                 kindCrc = 0;
    uint32_t                 cell    = 0;
    SiteShape                shape   = SiteShape::Dot;
    uint32_t                 colour  = 0xFFFFFF;   // 0xRRGGBB from SiteKindInfo::debugColour
    Dia::Maths::Vector2D     centre;
    std::vector<LineSegment> lines;
};
// One marker per site of each registered kind (kinds order, ascending cell). Sites of unregistered kinds are skipped.
std::vector<SiteMarker> BuildSiteMarkers(const WorldStructure& world, const WorldHexGrid& grid,
                                         std::span<const SiteKindInfo> kinds, float radius);

struct MarkerRing
{
    uint32_t                 cell = 0;
    Dia::Maths::Vector2D     centre;
    bool                     violating = false;   // the start belongs to a pair in the evidence's violatingPairs
    std::vector<LineSegment> marker;
    std::vector<LineSegment> ring;                // spacing ring, radius = spacing hex steps
};
// Markers for every site of kind 'kindCrc' with spacing rings of 'spacingParam' hex steps. 'spacingEvidence' is the
// GuaranteeResult of the kind's min_spacing guarantee from a FRESH validation or nullptr; only its violatingPairs
// are read.
std::vector<MarkerRing> BuildMarkers(const WorldStructure& world, const WorldHexGrid& grid, uint32_t kindCrc,
                                     float spacingParam, const GuaranteeResult* spacingEvidence, float markerRadius);

struct EvidencePolyline
{
    uint32_t                          guaranteeCrc = 0;
    std::vector<Dia::Maths::Vector2D> points;   // cell centres along one proofPath
};
struct ViolatingCellMark
{
    uint32_t                 guaranteeCrc = 0;
    uint32_t                 cell = 0;
    std::vector<LineSegment> outline;   // hex outline (drawn red)
    std::vector<LineSegment> cross;     // two diagonals
};
struct GuaranteeEvidenceGeometry
{
    std::vector<EvidencePolyline>  polylines;     // proof paths of the expanded guarantee only
    std::vector<ViolatingCellMark> violations;    // violating cells of every failing guarantee
    size_t LineCount() const;
};
// 'evidence' must already be fresh (see FreshEvidence); nullptr draws nothing. expandedGuaranteeId 0 = none expanded.
GuaranteeEvidenceGeometry BuildGuaranteeEvidence(const ValidationResult* evidence, const WorldHexGrid& grid,
                                                 uint32_t expandedGuaranteeId);

// Evidence freshness (layered-debug-rendering.md): evidence draws only when a report or a Validate result belongs to the
// current revision. Revision 0 means "none" and is never fresh.
bool EvidenceIsFresh(uint32_t reportWorldRevision, uint32_t freshValidateRevision, uint32_t currentRevision);
// The evidence to draw: the Validate result when its revision is current, else the report's finalValidation when the
// report's is, else nullptr (stale, imported, failed). Either pointer may be null.
const ValidationResult* FreshEvidence(const GenerationReport* report, const ValidationResult* validate,
                                      uint32_t validateRevision, uint32_t currentRevision);

} // namespace WorldGeneration
} // namespace CoW
