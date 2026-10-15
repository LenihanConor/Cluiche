#pragma once
#include "DiaPathfinding/CellCoord.h"
#include "DiaPathfinding/HexTopology.h"
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

        void SetPassable(CellCoord cell, bool passable);
        bool IsPassable(CellCoord cell) const;
        // Offset mode: passable in-bounds neighbours in direction order 0..5 (NE,N,NW,SW,S,SE).
        void GetNeighbours(CellCoord cell, Dia::Core::Containers::DynamicArrayC<CellCoord, 16>& outNeighbours) const;

        HexLayout GetLayout() const;
        int GetRadius() const;  // axial mode only
        int GetWidth() const;   // offset mode only
        int GetHeight() const;  // offset mode only
    private:
        bool IsInBounds(CellCoord cell) const;
        int  CellIndex(CellCoord cell) const;

        HexLayout mLayout;
        int mRadius;
        static const unsigned int kMaxCells = 1681;
        Dia::Core::Containers::DynamicArrayC<bool, kMaxCells> mPassable;  // axial mode
        HexTopology mTopology;                                             // offset mode (1x1 placeholder in axial mode)
        std::vector<unsigned char> mOffsetPassable;                        // offset mode, width*height
    };
}
