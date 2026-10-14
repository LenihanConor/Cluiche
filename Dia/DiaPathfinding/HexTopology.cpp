#include "DiaPathfinding/HexTopology.h"
#include <DiaCore/Core/Assert.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace Dia::Pathfinding {
    namespace {
        // {dx, dy} per direction, indexed [columnParity][dir]. Parity 0 = even column, 1 = odd.
        // Order NE, N, NW, SW, S, SE.
        constexpr int kOffsets[2][kHexDirections][2] = {
            { {+1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {0, +1}, {+1, 0} },
            { {+1, 0}, {0, -1}, {-1, 0}, {-1, +1}, {0, +1}, {+1, +1} },
        };

        // (cos, -sin) of 30deg + d*60deg (on-screen CCW angle, y-down world space).
        constexpr float kHalfSqrt3 = 0.8660254037844386f;
        constexpr float kDirVectors[kHexDirections][2] = {
            {  kHalfSqrt3, -0.5f },
            {  0.0f,       -1.0f },
            { -kHalfSqrt3, -0.5f },
            { -kHalfSqrt3,  0.5f },
            {  0.0f,        1.0f },
            {  kHalfSqrt3,  0.5f },
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
        const int* off = kOffsets[c.x & 1][dir];
        const CellCoord n{ c.x + off[0], c.y + off[1] };
        return IsInBounds(n) ? ToIndex(n) : kNoCell;
    }

    uint32_t HexTopology::Distance(CellCoord a, CellCoord b) {
        // odd-q offset -> cube (q = column, r = row - (col - (col&1)) / 2; exact division)
        const int aq = a.x, bq = b.x;
        const int ar = a.y - (a.x - (a.x & 1)) / 2;
        const int br = b.y - (b.x - (b.x & 1)) / 2;
        const int dq = aq - bq;
        const int dr = ar - br;
        return (uint32_t)((std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2);
    }

    uint32_t HexTopology::DirectionFromAngle(float radians) {
        // round((angle - 30deg) / 60deg) == floor(angle / 60deg)
        constexpr double kSector = 3.14159265358979323846 / 3.0;
        const long long sector = (long long)std::floor((double)radians / kSector);
        const long long m = ((sector % 6) + 6) % 6;
        return (uint32_t)m;
    }

    Dia::Maths::Vector2D HexTopology::DirectionVector(uint32_t dir) {
        DIA_ASSERT(dir < kHexDirections, "HexTopology direction out of range");
        return Dia::Maths::Vector2D(kDirVectors[dir][0], kDirVectors[dir][1]);
    }

    Dia::Maths::Vector2D HexTopology::CellCenter(CellCoord coord, float size) {
        constexpr float kSqrt3 = 1.7320508075688772f;
        return Dia::Maths::Vector2D(1.5f * size * (float)coord.x,
                                    kSqrt3 * size * ((float)coord.y + 0.5f * (float)(coord.x & 1)));
    }

    CellCoord HexTopology::CellFromPoint(Dia::Maths::Vector2D point, float size) {
        constexpr float kSqrt3Over3 = 0.5773502691896258f;
        const float qf = (2.0f / 3.0f * point.X()) / size;
        const float rf = (-1.0f / 3.0f * point.X() + kSqrt3Over3 * point.Y()) / size;
        const float sf = -qf - rf;
        float q = std::round(qf), r = std::round(rf), s = std::round(sf);
        const float dq = std::fabs(q - qf), dr = std::fabs(r - rf), ds = std::fabs(s - sf);
        if (dq > dr && dq > ds)      q = -r - s;
        else if (dr > ds)            r = -q - s;
        const int qi = (int)q, ri = (int)r;
        return CellCoord{ qi, ri + (qi - (qi & 1)) / 2 };
    }
}
