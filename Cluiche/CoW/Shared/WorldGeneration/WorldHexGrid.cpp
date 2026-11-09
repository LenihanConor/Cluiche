// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#include <Shared/WorldGeneration/WorldHexGrid.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>

namespace CoW {
namespace WorldGeneration {

using Dia::Maths::Vector2D;
using Dia::Pathfinding::HexTopology;

WorldHexGrid::WorldHexGrid(GridDimensions dims, float cellSize)
    : mDims(dims)
    , mCellSize(cellSize)
    , mBoundsMin(0.0f, 0.0f)
    , mBoundsMax(0.0f, 0.0f)
{
    if (mDims.width < 0) mDims.width = 0;
    if (mDims.height < 0) mDims.height = 0;

    const HexTopology topo(mDims.width, mDims.height);
    const uint32_t count = topo.CellCount();
    mCentres.assign(2u * count, 0.0f);
    mNeighbours.assign(6u * count, -1);

    const float halfH = std::sqrt(3.0f) * cellSize * 0.5f;
    Vector2D bmin(1e30f, 1e30f);
    Vector2D bmax(-1e30f, -1e30f);
    for (uint32_t i = 0; i < count; ++i)
    {
        const Vector2D c = HexTopology::CellCenter(topo.ToCoord(i), cellSize);
        mCentres[2u * i]      = c.x;
        mCentres[2u * i + 1u] = c.y;
        bmin.x = (std::min)(bmin.x, c.x - cellSize);
        bmin.y = (std::min)(bmin.y, c.y - halfH);
        bmax.x = (std::max)(bmax.x, c.x + cellSize);
        bmax.y = (std::max)(bmax.y, c.y + halfH);
        for (uint32_t d = 0; d < 6u; ++d)
        {
            const uint32_t n = topo.NeighbourAt(i, d);
            mNeighbours[6u * i + d] = (n == Dia::Pathfinding::kNoCell) ? -1 : static_cast<int32_t>(n);
        }
    }
    if (count > 0)
    {
        mBoundsMin = bmin;
        mBoundsMax = bmax;
    }

    mDesc = Dia::Graphics::CellGridDesc();
    mDesc.id         = Dia::Core::StringCRC("world.hex");
    mDesc.topology   = Dia::Core::StringCRC("hex.flat.oddq");
    mDesc.width      = static_cast<uint16_t>(mDims.width);
    mDesc.height     = static_cast<uint16_t>(mDims.height);
    mDesc.cellSize   = cellSize;
    mDesc.centresXY  = mCentres.data();
    mDesc.neighbours = mNeighbours.data();
}

Vector2D WorldHexGrid::CellCenter(uint32_t cell) const
{
    return Vector2D(mCentres[2u * cell], mCentres[2u * cell + 1u]);
}

int32_t WorldHexGrid::CellFromPoint(Vector2D point) const
{
    const Dia::Pathfinding::CellCoord c = HexTopology::CellFromPoint(point, mCellSize);
    if (c.x < 0 || c.y < 0 || c.x >= mDims.width || c.y >= mDims.height)
        return -1;
    return static_cast<int32_t>(c.y) * mDims.width + c.x;
}

} // namespace WorldGeneration
} // namespace CoW
