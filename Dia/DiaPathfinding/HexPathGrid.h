#pragma once
#include "DiaGeometry2D/Grid/CellCoord.h"
#include "DiaGeometry2D/Grid/HexTopology.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <vector>

namespace Dia::Pathfinding {

    enum class HexLayout { kAxialHexagon, kOffsetRectangle };

    class HexPathGrid {
    public:
        // Axial hexagon of the given radius (centre at 0,0).
        explicit HexPathGrid(int radius);
        // Rectangular flat-top odd-q offset grid; x = column, y = row.
        HexPathGrid(int width, int height);

        void SetPassable(Dia::Geometry2D::CellCoord cell, bool passable);
        bool IsPassable(Dia::Geometry2D::CellCoord cell) const;
        // Offset mode: passable in-bounds neighbours in direction order 0..5 (NE,N,NW,SW,S,SE).
        void GetNeighbours(Dia::Geometry2D::CellCoord cell, Dia::Core::Containers::DynamicArrayC<Dia::Geometry2D::CellCoord, 16>& outNeighbours) const;

        HexLayout GetLayout() const;
        int GetRadius() const;  // axial mode only
        int GetWidth() const;   // offset mode only
        int GetHeight() const;  // offset mode only
    private:
        bool IsInBounds(Dia::Geometry2D::CellCoord cell) const;
        int  CellIndex(Dia::Geometry2D::CellCoord cell) const;

        HexLayout mLayout;
        int mRadius;
        static const unsigned int kMaxCells = 1681;
        Dia::Core::Containers::DynamicArrayC<bool, kMaxCells> mPassable;  // axial mode
        Dia::Geometry2D::HexTopology mTopology;                                             // offset mode (1x1 placeholder in axial mode)
        std::vector<unsigned char> mOffsetPassable;                        // offset mode, width*height
    };
}
