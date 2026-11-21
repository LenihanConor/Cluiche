// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#include <Shared/WorldGeneration/WorldOverlayGeometry.h>

#include <Shared/WorldGeneration/SiteGuarantees.h>
#include <Shared/WorldGeneration/WorldLayerSource.h>

#include <algorithm>
#include <cmath>

namespace CoW {
namespace WorldGeneration {

using Dia::Maths::Vector2D;

std::vector<LineSegment> BuildRiverSegments(const WorldStructure& world, const WorldHexGrid& grid)
{
    std::vector<LineSegment> out;
    const GridDimensions wd = world.dimensions;
    const GridDimensions gd = grid.Dimensions();
    if (wd.width != gd.width || wd.height != gd.height || world.CellCount() != static_cast<int>(grid.CellCount()))
        return out;

    const Cell* cells = world.Cells();
    for (int i = 0; i < world.CellCount(); ++i)
    {
        const Cell& c = cells[i];
        if (c.waterSource != WaterSource::River || c.downstream > 5)
            continue;
        const int32_t n = grid.Neighbour(static_cast<uint32_t>(i), c.downstream);
        if (n < 0)
            continue;
        out.push_back({ grid.CellCenter(static_cast<uint32_t>(i)), grid.CellCenter(static_cast<uint32_t>(n)) });
    }
    return out;
}

void AppendHexOutline(std::vector<LineSegment>& out, Vector2D centre, float cellSize)
{
    const float kPi = 3.14159265358979f;
    Vector2D corner[6];
    for (int k = 0; k < 6; ++k)
    {
        const float a = kPi / 3.0f * static_cast<float>(k);
        corner[k] = Vector2D(centre.x + cellSize * std::cos(a), centre.y + cellSize * std::sin(a));
    }
    for (int k = 0; k < 6; ++k)
        out.push_back({ corner[k], corner[(k + 1) % 6] });
}

NeighbourOutline BuildNeighbourOutline(const WorldHexGrid& grid, int cell)
{
    NeighbourOutline o;
    if (cell < 0 || static_cast<uint32_t>(cell) >= grid.CellCount())
        return o;

    o.cell = cell;
    AppendHexOutline(o.selectedOutline, grid.CellCenter(static_cast<uint32_t>(cell)), grid.CellSize());
    for (uint32_t d = 0; d < 6u; ++d)
    {
        const int32_t n = grid.Neighbour(static_cast<uint32_t>(cell), d);
        if (n < 0)
            continue;
        o.hasLabel[d]      = true;
        o.neighbourCell[d] = n;
        o.labelAnchor[d]   = grid.CellCenter(static_cast<uint32_t>(n));
        AppendHexOutline(o.neighbourOutlines, o.labelAnchor[d], grid.CellSize());
    }
    return o;
}

namespace
{
    constexpr float kPi = 3.14159265358979f;

    bool SameDimensions(const WorldStructure& world, const WorldHexGrid& grid)
    {
        const GridDimensions wd = world.dimensions;
        const GridDimensions gd = grid.Dimensions();
        return wd.width == gd.width && wd.height == gd.height && world.CellCount() == static_cast<int>(grid.CellCount());
    }

    Vector2D Polar(Vector2D c, float r, float radians) { return Vector2D(c.x + r * std::cos(radians), c.y + r * std::sin(radians)); }

    // Closed polygon through n points at radius r, first point at 'start' radians.
    void AppendPolygon(std::vector<LineSegment>& out, Vector2D c, float r, int n, float start)
    {
        for (int k = 0; k < n; ++k)
        {
            const float a0 = start + 2.0f * kPi * static_cast<float>(k) / static_cast<float>(n);
            const float a1 = start + 2.0f * kPi * static_cast<float>(k + 1) / static_cast<float>(n);
            out.push_back({ Polar(c, r, a0), Polar(c, r, a1) });
        }
    }
}

std::vector<LineSegment> BuildRegionEdges(const WorldStructure& world, const WorldHexGrid& grid)
{
    std::vector<LineSegment> out;
    for (const RegionEdge& e : BuildColouredRegionEdges(world, grid))
        out.push_back(e.seg);
    return out;
}

std::vector<RegionEdge> BuildColouredRegionEdges(const WorldStructure& world, const WorldHexGrid& grid)
{
    std::vector<RegionEdge> out;
    if (!SameDimensions(world, grid))
        return out;
    const Cell* cells = world.Cells();
    const uint32_t n = grid.CellCount();
    bool any = false;
    for (uint32_t i = 0; i < n && !any; ++i)
        any = cells[i].region != kNoRegion;
    if (!any)
        return out;

    const float size = grid.CellSize();
    for (uint32_t i = 0; i < n; ++i)
    {
        const Vector2D ca = grid.CellCenter(i);
        for (uint32_t d = 0; d < 6u; ++d)
        {
            const int32_t nb = grid.Neighbour(i, d);
            if (nb < 0 || static_cast<uint32_t>(nb) <= i || cells[nb].region == cells[i].region)
                continue;
            // The shared side is the one of i's six whose midpoint is nearest the midpoint of the two centres.
            const Vector2D cb = grid.CellCenter(static_cast<uint32_t>(nb));
            const Vector2D m((ca.x + cb.x) * 0.5f, (ca.y + cb.y) * 0.5f);
            int best = 0;
            float bestDist = 0.0f;
            for (int k = 0; k < 6; ++k)
            {
                const float a = kPi / 3.0f * (static_cast<float>(k) + 0.5f);   // side k's midpoint direction
                const Vector2D sm = Polar(ca, size, a);
                const float dist = (sm.x - m.x) * (sm.x - m.x) + (sm.y - m.y) * (sm.y - m.y);
                if (k == 0 || dist < bestDist) { best = k; bestDist = dist; }
            }
            RegionEdge e;
            e.seg = { Polar(ca, size, kPi / 3.0f * static_cast<float>(best)),
                      Polar(ca, size, kPi / 3.0f * static_cast<float>(best + 1)) };
            e.regionA    = cells[i].region;
            e.regionB    = cells[nb].region;
            e.colourSlot = RegionPaletteSlot(e.regionA != kNoRegion ? e.regionA : e.regionB);
            out.push_back(e);
        }
    }
    return out;
}

void AppendSiteMarker(std::vector<LineSegment>& out, Vector2D c, float r, SiteShape shape)
{
    switch (shape)
    {
    case SiteShape::Dot:
    {
        const float inner = r * 0.4f;
        AppendPolygon(out, c, inner, 6, 0.0f);
        for (int k = 0; k < 3; ++k)   // spokes so the dot reads as filled
            out.push_back({ Polar(c, inner, kPi / 3.0f * static_cast<float>(k)), Polar(c, inner, kPi / 3.0f * static_cast<float>(k + 3)) });
        break;
    }
    case SiteShape::Diamond:  AppendPolygon(out, c, r, 4, 0.0f); break;
    case SiteShape::Square:   AppendPolygon(out, c, r, 4, kPi / 4.0f); break;
    case SiteShape::Triangle: AppendPolygon(out, c, r, 3, -kPi / 2.0f); break;
    case SiteShape::Star:
    {
        Vector2D p[10];
        for (int k = 0; k < 10; ++k)
            p[k] = Polar(c, (k % 2 == 0) ? r : r * 0.45f, -kPi / 2.0f + kPi / 5.0f * static_cast<float>(k));
        for (int k = 0; k < 10; ++k)
            out.push_back({ p[k], p[(k + 1) % 10] });
        break;
    }
    case SiteShape::Ring:     AppendPolygon(out, c, r, 12, 0.0f); break;
    }
}

std::vector<LineSegment> BuildSiteMarkers(const WorldStructure& world, const WorldHexGrid& grid, uint32_t kindCrc,
                                          SiteShape shape, float radius)
{
    std::vector<LineSegment> out;
    if (!SameDimensions(world, grid))
        return out;
    for (const Site& s : world.sites.Of(kindCrc))
    {
        if (s.cell < grid.CellCount())
            AppendSiteMarker(out, grid.CellCenter(s.cell), radius, shape);
    }
    return out;
}

std::vector<SiteMarker> BuildSiteMarkers(const WorldStructure& world, const WorldHexGrid& grid,
                                         std::span<const SiteKindInfo> kinds, float radius)
{
    std::vector<SiteMarker> out;
    if (!SameDimensions(world, grid))
        return out;
    for (const SiteKindInfo& k : kinds)
    {
        for (const Site& s : world.sites.Of(k.kindCrc))
        {
            if (s.cell >= grid.CellCount())
                continue;
            SiteMarker m;
            m.kindCrc = k.kindCrc;
            m.cell    = s.cell;
            m.shape   = k.debugShape;
            m.colour  = k.debugColour;
            m.centre  = grid.CellCenter(s.cell);
            AppendSiteMarker(m.lines, m.centre, radius, k.debugShape);
            out.push_back(std::move(m));
        }
    }
    return out;
}

std::vector<DynastyStartMarker> BuildDynastyStarts(const WorldStructure& world, const WorldHexGrid& grid,
                                                   float spacingParam, const GuaranteeResult* evidence, float markerRadius)
{
    std::vector<DynastyStartMarker> out;
    if (!SameDimensions(world, grid))
        return out;
    const float ringRadius = std::max(spacingParam, 0.0f) * std::sqrt(3.0f) * grid.CellSize();   // one hex step = sqrt(3) * size
    for (const Site& s : world.sites.Of(SiteKinds::DynastyStart()))
    {
        if (s.cell >= grid.CellCount())
            continue;
        DynastyStartMarker m;
        m.cell   = s.cell;
        m.centre = grid.CellCenter(s.cell);
        if (evidence)
        {
            for (const CellPair& p : evidence->violatingPairs)
                if (p.a == s.cell || p.b == s.cell)
                    m.violating = true;
        }
        AppendSiteMarker(m.marker, m.centre, markerRadius, SiteShape::Star);
        if (ringRadius > 0.0f)
            AppendPolygon(m.ring, m.centre, ringRadius, 24, 0.0f);
        out.push_back(std::move(m));
    }
    return out;
}

size_t GuaranteeEvidenceGeometry::LineCount() const
{
    size_t n = 0;
    for (const EvidencePolyline& p : polylines)
        n += p.points.size() > 1 ? p.points.size() - 1 : 0;
    for (const ViolatingCellMark& v : violations)
        n += v.outline.size() + v.cross.size();
    return n;
}

GuaranteeEvidenceGeometry BuildGuaranteeEvidence(const ValidationResult* evidence, const WorldHexGrid& grid,
                                                 uint32_t expandedGuaranteeId)
{
    GuaranteeEvidenceGeometry out;
    if (!evidence)
        return out;
    const uint32_t count = grid.CellCount();
    for (const GuaranteeResult& g : evidence->guarantees)
    {
        if (expandedGuaranteeId != 0 && g.idCrc == expandedGuaranteeId)
        {
            for (const ProofPath& p : g.proofPaths)
            {
                EvidencePolyline line;
                line.guaranteeCrc = g.idCrc;
                for (const uint32_t c : p.cells)
                    if (c < count)
                        line.points.push_back(grid.CellCenter(c));
                if (line.points.size() > 1)
                    out.polylines.push_back(std::move(line));
            }
        }
        if (g.satisfied)
            continue;
        for (const uint32_t c : g.violatingCells)
        {
            if (c >= count)
                continue;
            ViolatingCellMark v;
            v.guaranteeCrc = g.idCrc;
            v.cell         = c;
            const Vector2D ctr = grid.CellCenter(c);
            const float r = grid.CellSize();
            AppendHexOutline(v.outline, ctr, r);
            const float h = r * 0.5f;
            v.cross.push_back({ Vector2D(ctr.x - h, ctr.y - h), Vector2D(ctr.x + h, ctr.y + h) });
            v.cross.push_back({ Vector2D(ctr.x - h, ctr.y + h), Vector2D(ctr.x + h, ctr.y - h) });
            out.violations.push_back(std::move(v));
        }
    }
    return out;
}

bool EvidenceIsFresh(uint32_t reportWorldRevision, uint32_t freshValidateRevision, uint32_t currentRevision)
{
    return currentRevision != 0 && (reportWorldRevision == currentRevision || freshValidateRevision == currentRevision);
}

const ValidationResult* FreshEvidence(const GenerationReport* report, const ValidationResult* validate,
                                      uint32_t validateRevision, uint32_t currentRevision)
{
    if (currentRevision == 0)
        return nullptr;
    if (validate && validateRevision == currentRevision)
        return validate;
    if (report && report->worldRevision == currentRevision)
        return &report->finalValidation;
    return nullptr;
}

} // namespace WorldGeneration
} // namespace CoW
