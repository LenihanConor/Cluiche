#include "DiaPathfinding/HexTopology.h"
#include <DiaCore/Core/Assert.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace Dia::Pathfinding {
    namespace {
        // {dx, dy} per direction, indexed [rowParity][dir]. Parity 0 = even row, 1 = odd row.
        constexpr int kOffsets[2][kHexDirections][2] = {
            { {+1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, +1}, {0, +1} },
            { {+1, 0}, {+1, -1}, {0, -1}, {-1, 0}, {0, +1}, {+1, +1} },
        };

        // cos/sin of d * 60 degrees (abstract counter-clockwise angle; y not flipped).
        constexpr float kHalfSqrt3 = 0.8660254037844386f;
        constexpr float kDirVectors[kHexDirections][2] = {
            {  1.0f,  0.0f },
            {  0.5f,  kHalfSqrt3 },
            { -0.5f,  kHalfSqrt3 },
            { -1.0f,  0.0f },
            { -0.5f, -kHalfSqrt3 },
            {  0.5f, -kHalfSqrt3 },
        };
    }

    HexTopology::HexTopology(int width, int height)
        : mWidth(width), mHeight(height) {
        DIA_ASSERT(width >= 1 && height >= 1, "HexTopology dimensions must be >= 1");
        DIA_ASSERT((long long)width * (long long)height <= 0x7FFFFFFFll,
                   "HexTopology width * height must fit in int");
    }

    uint32_t HexTopology::CellCount() const {
        return (uint32_t)(mWidth * mHeight);
    }

    bool HexTopology::IsInBounds(CellCoord coord) const {
        return coord.x >= 0 && coord.x < mWidth && coord.y >= 0 && coord.y < mHeight;
    }

    uint32_t HexTopology::ToIndex(CellCoord coord) const {
        return (uint32_t)(coord.y * mWidth + coord.x);
    }

    CellCoord HexTopology::ToCoord(uint32_t index) const {
        return CellCoord{ (int)index % mWidth, (int)index / mWidth };
    }

    uint32_t HexTopology::NeighbourAt(uint32_t cell, uint32_t dir) const {
        DIA_ASSERT(dir < kHexDirections, "HexTopology direction out of range");
        const CellCoord c = ToCoord(cell);
        const int* off = kOffsets[c.y & 1][dir];
        const CellCoord n{ c.x + off[0], c.y + off[1] };
        return IsInBounds(n) ? ToIndex(n) : kNoCell;
    }

    uint32_t HexTopology::Distance(CellCoord a, CellCoord b) {
        // odd-r offset -> cube
        const int aq = a.x - (a.y - (a.y & 1)) / 2;
        const int bq = b.x - (b.y - (b.y & 1)) / 2;
        const int dx = aq - bq;
        const int dz = a.y - b.y;
        const int dy = -dx - dz;
        return (uint32_t)std::max({ std::abs(dx), std::abs(dy), std::abs(dz) });
    }

    uint32_t HexTopology::DirectionFromAngle(float radians) {
        constexpr double kSector = 3.14159265358979323846 / 3.0;
        const long long sector = (long long)std::floor((double)radians / kSector + 0.5);
        const long long m = ((sector % 6) + 6) % 6;
        return (uint32_t)m;
    }

    Dia::Maths::Vector2D HexTopology::DirectionVector(uint32_t dir) {
        DIA_ASSERT(dir < kHexDirections, "HexTopology direction out of range");
        return Dia::Maths::Vector2D(kDirVectors[dir][0], kDirVectors[dir][1]);
    }
}
