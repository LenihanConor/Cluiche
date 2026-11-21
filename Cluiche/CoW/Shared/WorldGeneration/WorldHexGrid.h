// S-1 World Generation map view: hex grid geometry for the DiaCellLayers viewer.
// Pure CoW-Shared code: owns the centre, neighbour and bounds storage that a CellGridDesc borrows.
// Convention: flat-top odd-q, cell index = row * width + col; neighbour d equals HexTopology::NeighbourAt(cell, d).
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#pragma once

#include <Shared/WorldGeneration/TerrainTypes.h>

#include <DiaGraphics/CellField/CellGridDesc.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <cstdint>
#include <vector>

namespace CoW {
namespace WorldGeneration {

class WorldHexGrid
{
public:
    static constexpr float kCellSize = 24.0f;   // centre-to-corner, world units

    explicit WorldHexGrid(GridDimensions dims, float cellSize = kCellSize);

    // The desc borrows pointers into this object's vectors, so moving or copying would dangle them.
    WorldHexGrid(const WorldHexGrid&)            = delete;
    WorldHexGrid& operator=(const WorldHexGrid&) = delete;
    WorldHexGrid(WorldHexGrid&&)                 = delete;
    WorldHexGrid& operator=(WorldHexGrid&&)      = delete;

    GridDimensions Dimensions() const { return mDims; }
    uint32_t       CellCount() const  { return static_cast<uint32_t>(mDims.width) * static_cast<uint32_t>(mDims.height); }
    float          CellSize() const   { return mCellSize; }

    Dia::Maths::Vector2D CellCenter(uint32_t cell) const;
    // Cell index containing the point, or -1 when the point falls outside the grid.
    int32_t CellFromPoint(Dia::Maths::Vector2D point) const;
    // Neighbour in direction 0..5, -1 past the edge.
    int32_t Neighbour(uint32_t cell, uint32_t dir) const { return mNeighbours[6u * cell + dir]; }

    Dia::Maths::Vector2D BoundsMin() const { return mBoundsMin; }
    Dia::Maths::Vector2D BoundsMax() const { return mBoundsMax; }

    const Dia::Graphics::CellGridDesc& Desc() const { return mDesc; }
    Dia::Graphics::CellGridDesc*       MutableDesc() { return &mDesc; }

    const std::vector<float>&   Centres() const    { return mCentres; }
    const std::vector<int32_t>& Neighbours() const { return mNeighbours; }

private:
    GridDimensions              mDims;
    float                       mCellSize;
    std::vector<float>          mCentres;      // 2 per cell
    std::vector<int32_t>        mNeighbours;   // 6 per cell
    Dia::Maths::Vector2D        mBoundsMin;
    Dia::Maths::Vector2D        mBoundsMax;
    Dia::Graphics::CellGridDesc mDesc;
};

} // namespace WorldGeneration
} // namespace CoW
