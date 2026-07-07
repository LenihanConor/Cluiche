#include "DiaPathfinding/SquarePathGrid.h"

namespace Dia::Pathfinding {

    SquarePathGrid::SquarePathGrid(int width, int height, SquareConnectivity connectivity)
        : mWidth(width)
        , mHeight(height)
        , mConnectivity(connectivity)
    {
        const int total = width * height;
        for (int i = 0; i < total; ++i)
        {
            mPassable.Add(true);
        }
    }

    void SquarePathGrid::SetPassable(CellCoord cell, bool passable)
    {
        mPassable[CellIndex(cell)] = passable;
    }

    bool SquarePathGrid::IsPassable(CellCoord cell) const
    {
        if (!IsInBounds(cell))
        {
            return false;
        }
        return mPassable[CellIndex(cell)];
    }

    void SquarePathGrid::GetNeighbours(CellCoord cell,
                                       Dia::Core::Containers::DynamicArrayC<CellCoord, 16>& outNeighbours) const
    {
        outNeighbours.RemoveAll();

        static const int kCardinalDx[] = {  0,  0, -1, +1 };
        static const int kCardinalDy[] = { -1, +1,  0,  0 };

        static const int kDiagonalDx[] = { -1, +1, -1, +1 };
        static const int kDiagonalDy[] = { -1, -1, +1, +1 };

        for (int i = 0; i < 4; ++i)
        {
            CellCoord nc{ cell.x + kCardinalDx[i], cell.y + kCardinalDy[i] };
            if (IsInBounds(nc) && IsPassable(nc))
            {
                outNeighbours.Add(nc);
            }
        }

        if (mConnectivity == SquareConnectivity::k8Connected)
        {
            for (int i = 0; i < 4; ++i)
            {
                CellCoord nc{ cell.x + kDiagonalDx[i], cell.y + kDiagonalDy[i] };
                if (IsInBounds(nc) && IsPassable(nc))
                {
                    outNeighbours.Add(nc);
                }
            }
        }
    }

    int SquarePathGrid::GetWidth() const
    {
        return mWidth;
    }

    int SquarePathGrid::GetHeight() const
    {
        return mHeight;
    }

    bool SquarePathGrid::IsInBounds(CellCoord cell) const
    {
        return cell.x >= 0 && cell.x < mWidth && cell.y >= 0 && cell.y < mHeight;
    }

    int SquarePathGrid::CellIndex(CellCoord cell) const
    {
        return cell.y * mWidth + cell.x;
    }

} // namespace Dia::Pathfinding
