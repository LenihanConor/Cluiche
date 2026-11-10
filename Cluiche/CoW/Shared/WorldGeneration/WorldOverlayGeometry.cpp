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

} // namespace WorldGeneration
} // namespace CoW
