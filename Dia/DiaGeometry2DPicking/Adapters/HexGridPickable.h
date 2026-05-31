////////////////////////////////////////////////////////////////////////////////
// Filename: HexGridPickable.h
// Description: IPickable2D adapter wrapping a HexGrid<T,Max>.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGeometry2DPicking/IPickable2D.h>
#include <DiaGeometry2DPicking/PickHit2D.h>
#include <DiaGeometry2D/Spatial/HexGrid.h>
#include <DiaGeometry2D/Shapes/AARect.h>

namespace Dia::Geometry2DPicking {

template<typename T, unsigned int MaxObjects>
class HexGridPickable : public IPickable2D
{
public:
    HexGridPickable(const Dia::Geometry2D::HexGrid<T, MaxObjects>& grid,
                    Dia::Core::StringCRC id,
                    int priority = 0,
                    Dia::Picking::PickLayer layer = Dia::Picking::PickLayer::kDefault)
        : mGrid(grid)
        , mId(id)
        , mPriority(priority)
        , mLayer(layer)
    {}

    Dia::Core::StringCRC    GetPickableId() const override { return mId; }
    int                     GetPriority()   const override { return mPriority; }
    Dia::Picking::PickLayer GetLayer()      const override { return mLayer; }

    bool TryPick(const Dia::Maths::Vector2D& worldPos, PickHit2D& out) const override
    {
        const Dia::Geometry2D::HexCoord hex = mGrid.WorldToHex(worldPos);
        if (!mGrid.IsValidHex(hex)) return false;

        out.pickableId = mId;
        out.kind       = PickHit2D::Kind::kHexCell;
        out.priority   = mPriority;
        out.worldPos   = worldPos;
        out.hexCell    = hex;
        return true;
    }

    void PickArea(const Dia::Geometry2D::AARect& worldRect,
                  Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits>& out) const override
    {
        const int cols = mGrid.GetColCount();
        const int rows = mGrid.GetRowCount();

        // Compute bounding row/col range from the query rect corners
        const Dia::Geometry2D::HexCoord blHex = mGrid.WorldToHex(worldRect.GetBottomLeft());
        const Dia::Geometry2D::HexCoord trHex = mGrid.WorldToHex(worldRect.GetTopRight());
        const int minR = (blHex.r > 0 ? blHex.r - 1 : 0);
        const int maxR = (trHex.r < rows - 1 ? trHex.r + 1 : rows - 1);
        const int minQ = (blHex.q > 0 ? blHex.q - 1 : 0);
        const int maxQ = (trHex.q < cols - 1 ? trHex.q + 1 : cols - 1);

        for (int r = minR; r <= maxR; ++r)
        {
            for (int q = minQ; q <= maxQ; ++q)
            {
                const Dia::Geometry2D::HexCoord coord{q, r};
                if (!mGrid.IsValidHex(coord)) continue;

                const Dia::Maths::Vector2D center = mGrid.HexToWorld(coord);
                if (worldRect.IsIntersecting(center).IsIntersecting())
                {
                    if (out.IsFull()) return;
                    PickHit2D hit;
                    hit.pickableId = mId;
                    hit.kind       = PickHit2D::Kind::kHexCell;
                    hit.priority   = mPriority;
                    hit.worldPos   = center;
                    hit.hexCell    = coord;
                    out.Add(hit);
                }
            }
        }
    }

private:
    const Dia::Geometry2D::HexGrid<T, MaxObjects>& mGrid;
    Dia::Core::StringCRC    mId;
    int                     mPriority;
    Dia::Picking::PickLayer mLayer;
};

} // namespace Dia::Geometry2DPicking
