#include "DiaPathfinding/HexPathGrid.h"
#include <cstdlib>

namespace Dia::Pathfinding {

    HexPathGrid::HexPathGrid(int radius)
        : mRadius(radius)
    {
        const int side = 2 * mRadius + 1;
        const int total = side * side;
        for (int i = 0; i < total; ++i)
        {
            mPassable.Add(true);
        }
    }

    void HexPathGrid::SetPassable(CellCoord cell, bool passable)
    {
        if (IsInBounds(cell))
        {
            mPassable[CellIndex(cell)] = passable;
        }
    }

    bool HexPathGrid::IsPassable(CellCoord cell) const
    {
        return IsInBounds(cell) && mPassable[CellIndex(cell)];
    }

    void HexPathGrid::GetNeighbours(CellCoord cell,
                                    Dia::Core::Containers::DynamicArrayC<CellCoord, 16>& outNeighbours) const
    {
        outNeighbours.RemoveAll();

        static const int kDq[] = { +1, -1,  0,  0, +1, -1 };
        static const int kDr[] = {  0,  0, +1, -1, -1, +1 };

        for (int i = 0; i < 6; ++i)
        {
            CellCoord nc{ cell.x + kDq[i], cell.y + kDr[i] };
            if (IsInBounds(nc) && IsPassable(nc))
            {
                outNeighbours.Add(nc);
            }
        }
    }

    int HexPathGrid::GetRadius() const
    {
        return mRadius;
    }

    bool HexPathGrid::IsInBounds(CellCoord cell) const
    {
        return std::abs(cell.x) <= mRadius
            && std::abs(cell.y) <= mRadius
            && std::abs(cell.x + cell.y) <= mRadius;
    }

    int HexPathGrid::CellIndex(CellCoord cell) const
    {
        const int side = 2 * mRadius + 1;
        return (cell.y + mRadius) * side + (cell.x + mRadius);
    }

} // namespace Dia::Pathfinding
