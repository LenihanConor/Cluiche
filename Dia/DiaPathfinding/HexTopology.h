#pragma once
#include "DiaPathfinding/CellCoord.h"
#include <DiaMaths/Vector/Vector2D.h>
#include <cstdint>

namespace Dia::Pathfinding {
    constexpr uint32_t kNoCell        = 0xFFFFFFFFu;
    constexpr uint32_t kHexDirections = 6;

    // Rectangular flat-top odd-q hex topology: odd COLUMNS are shifted half a cell down,
    // row index grows downward (south). CellCoord x = column, y = row. Index = y * width + x.
    // World space is y-DOWN; with size = centre-to-corner distance a hex is 2*size wide and
    // sqrt(3)*size tall, columns are 1.5*size apart.
    // Direction d (0..5) is at on-screen angle 30deg + d*60deg counter-clockwise:
    // 0=NE 1=N 2=NW 3=SW 4=S 5=SE (flat-top has no E/W neighbour).
    // Opposite of d is (d + 3) % 6.
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

        // d = round((angle - 30deg) / 60deg) mod 6. Angle is the on-screen counter-clockwise
        // angle (same convention as DirectionVector); negative angles wrap correctly.
        static uint32_t DirectionFromAngle(float radians);

        // Unit vector (cos(theta), -sin(theta)), theta = 30deg + d*60deg; y-DOWN world space,
        // so d=1 (N) is (0,-1).
        static Dia::Maths::Vector2D DirectionVector(uint32_t dir);

        // World-space centre of a cell: (1.5*size*col, sqrt(3)*size*(row + 0.5*(col&1))).
        static Dia::Maths::Vector2D CellCenter(CellCoord coord, float size);

        // Inverse of CellCenter (cube rounding). May return out-of-bounds coords.
        static CellCoord CellFromPoint(Dia::Maths::Vector2D point, float size);

        int Width() const  { return mWidth; }
        int Height() const { return mHeight; }

    private:
        int mWidth;
        int mHeight;
    };
}
