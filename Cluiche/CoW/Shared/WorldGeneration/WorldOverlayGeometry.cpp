// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#include <Shared/WorldGeneration/WorldOverlayGeometry.h>

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
            out.push_back({ Polar(ca, size, kPi / 3.0f * static_cast<float>(best)),
                            Polar(ca, size, kPi / 3.0f * static_cast<float>(best + 1)) });
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

} // namespace WorldGeneration
} // namespace CoW
