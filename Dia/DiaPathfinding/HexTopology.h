#pragma once
#include "DiaPathfinding/CellCoord.h"
#include <DiaMaths/Vector/Vector2D.h>
#include <cstdint>

namespace Dia::Pathfinding {
    constexpr uint32_t kNoCell        = 0xFFFFFFFFu;
    constexpr uint32_t kHexDirections = 6;

    // Rectangular pointy-top odd-r hex topology: odd rows are shifted half a cell right,
    // row index grows downward. CellCoord x = column, y = row. Index = y * width + x.
    // Direction d (0..5) is at d * 60 degrees counter-clockwise from east:
    // 0=E 1=NE 2=NW 3=W 4=SW 5=SE. Opposite of d is (d + 3) % 6.
    // Pure value type; deterministic and allocation-free.
    class HexTopology {
    public:
        HexTopology(int width, int height);

        uint32_t  CellCount() const;
        bool      IsInBounds(CellCoord coord) const;
        uint32_t  ToIndex(CellCoord coord) const;
        CellCoord ToCoord(uint32_t index) const;

        // Neighbour of 'cell' in direction 'dir' (0..5); kNoCell past the edge.
        uint32_t  NeighbourAt(uint32_t cell, uint32_t dir) const;

        // Hex steps between two in-grid or virtual coords, via cube coordinates.
        static uint32_t Distance(CellCoord a, CellCoord b);

        // round(angle / 60deg) mod 6. Angle is the abstract counter-clockwise angle
        // (same convention as DirectionVector); negative angles wrap correctly.
        static uint32_t DirectionFromAngle(float radians);

        // Unit vector (cos(d*60deg), sin(d*60deg)); y is NOT flipped.
        static Dia::Maths::Vector2D DirectionVector(uint32_t dir);

        int Width() const  { return mWidth; }
        int Height() const { return mHeight; }

    private:
        int mWidth;
        int mHeight;
    };
}
