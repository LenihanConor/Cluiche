#include "DiaPathfinding/HexPathGrid.h"
#include <DiaCore/Core/Assert.h>
#include <cstdlib>

namespace Dia::Pathfinding {

    HexPathGrid::HexPathGrid(int radius)
        : mLayout(HexLayout::kAxialHexagon)
        , mRadius(radius)
        , mTopology(1, 1)
    {
        const int side = 2 * mRadius + 1;
        const int total = side * side;
        for (int i = 0; i < total; ++i)
        {
            mPassable.Add(true);
        }
    }

    HexPathGrid::HexPathGrid(int width, int height)
        : mLayout(HexLayout::kOffsetRectangle)
        , mRadius(0)
        , mTopology(width, height)
        , mOffsetPassable(static_cast<size_t>(mTopology.CellCount()), 1)
    {
    }

    void HexPathGrid::SetPassable(CellCoord cell, bool passable)
    {
        if (IsInBounds(cell))
        {
            if (mLayout == HexLayout::kOffsetRectangle)
            {
                mOffsetPassable[CellIndex(cell)] = passable ? 1 : 0;
            }
            else
            {
                mPassable[CellIndex(cell)] = passable;
            }
        }
    }

    bool HexPathGrid::IsPassable(CellCoord cell) const
    {
        if (!IsInBounds(cell))
        {
            return false;
        }
        if (mLayout == HexLayout::kOffsetRectangle)
        {
            return mOffsetPassable[CellIndex(cell)] != 0;
        }
        return mPassable[CellIndex(cell)];
    }

    void HexPathGrid::GetNeighbours(CellCoord cell,
                                    Dia::Core::Containers::DynamicArrayC<CellCoord, 16>& outNeighbours) const
    {
        outNeighbours.RemoveAll();

        if (mLayout == HexLayout::kOffsetRectangle)
        {
            if (!mTopology.IsInBounds(cell))
            {
                return;
            }
            const uint32_t index = mTopology.ToIndex(cell);
            for (uint32_t d = 0; d < kHexDirections; ++d)
            {
                const uint32_t n = mTopology.NeighbourAt(index, d);
                if (n != kNoCell && mOffsetPassable[n] != 0)
                {
                    outNeighbours.Add(mTopology.ToCoord(n));
                }
            }
            return;
        }

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

    HexLayout HexPathGrid::GetLayout() const
    {
        return mLayout;
    }

    int HexPathGrid::GetRadius() const
    {
        DIA_ASSERT(mLayout == HexLayout::kAxialHexagon, "GetRadius is only valid for axial hexagon grids");
        return mRadius;
    }

    int HexPathGrid::GetWidth() const
    {
        DIA_ASSERT(mLayout == HexLayout::kOffsetRectangle, "GetWidth is only valid for offset rectangle grids");
        return mTopology.Width();
    }

    int HexPathGrid::GetHeight() const
    {
        DIA_ASSERT(mLayout == HexLayout::kOffsetRectangle, "GetHeight is only valid for offset rectangle grids");
        return mTopology.Height();
    }

    bool HexPathGrid::IsInBounds(CellCoord cell) const
    {
        if (mLayout == HexLayout::kOffsetRectangle)
        {
            return mTopology.IsInBounds(cell);
        }
        return std::abs(cell.x) <= mRadius
            && std::abs(cell.y) <= mRadius
            && std::abs(cell.x + cell.y) <= mRadius;
    }

    int HexPathGrid::CellIndex(CellCoord cell) const
    {
        if (mLayout == HexLayout::kOffsetRectangle)
        {
            return static_cast<int>(mTopology.ToIndex(cell));
        }
        const int side = 2 * mRadius + 1;
        return (cell.y + mRadius) * side + (cell.x + mRadius);
    }

} // namespace Dia::Pathfinding
