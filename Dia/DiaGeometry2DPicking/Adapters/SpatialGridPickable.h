////////////////////////////////////////////////////////////////////////////////
// Filename: SpatialGridPickable.h
// Description: IPickable2D adapter wrapping a SpatialGrid<T,Max>.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGeometry2DPicking/IPickable2D.h>
#include <DiaGeometry2DPicking/PickHit2D.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Shapes/AARect.h>

namespace Dia::Geometry2DPicking {

template<typename T, unsigned int MaxObjects>
class SpatialGridPickable : public IPickable2D
{
public:
    SpatialGridPickable(const Dia::Geometry2D::SpatialGrid<T, MaxObjects>& grid,
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
        const Dia::Geometry2D::AARect& wb = mGrid.GetWorldBounds();
        if (!wb.IsIntersecting(worldPos).IsIntersecting()) return false;

        const float blX = wb.GetBottomLeft().x;
        const float blY = wb.GetBottomLeft().y;
        const float cs  = mGrid.GetCellSize();

        const int cx = static_cast<int>((worldPos.x - blX) / cs);
        const int cy = static_cast<int>((worldPos.y - blY) / cs);

        if (cx < 0 || cx >= mGrid.GetCellCountX()) return false;
        if (cy < 0 || cy >= mGrid.GetCellCountY()) return false;

        out.pickableId       = mId;
        out.kind             = PickHit2D::Kind::kSpatialCell;
        out.priority         = mPriority;
        out.worldPos         = worldPos;
        out.spatialCell.x    = cx;
        out.spatialCell.y    = cy;
        return true;
    }

    void PickArea(const Dia::Geometry2D::AARect& worldRect,
                  Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits>& out) const override
    {
        const Dia::Geometry2D::AARect& wb = mGrid.GetWorldBounds();
        const float blX = wb.GetBottomLeft().x;
        const float blY = wb.GetBottomLeft().y;
        const float cs  = mGrid.GetCellSize();
        const int countX = mGrid.GetCellCountX();
        const int countY = mGrid.GetCellCountY();

        const float qMinX = worldRect.GetBottomLeft().x;
        const float qMinY = worldRect.GetBottomLeft().y;
        const float qMaxX = worldRect.GetTopRight().x;
        const float qMaxY = worldRect.GetTopRight().y;

        const int startX = static_cast<int>((qMinX - blX) / cs);
        const int startY = static_cast<int>((qMinY - blY) / cs);
        const int endX   = static_cast<int>((qMaxX - blX) / cs);
        const int endY   = static_cast<int>((qMaxY - blY) / cs);

        for (int cy = startY; cy <= endY; ++cy)
        {
            for (int cx = startX; cx <= endX; ++cx)
            {
                if (cx < 0 || cx >= countX || cy < 0 || cy >= countY) continue;
                if (out.IsFull()) return;

                const float cellMinX = blX + cx * cs;
                const float cellMinY = blY + cy * cs;

                PickHit2D hit;
                hit.pickableId       = mId;
                hit.kind             = PickHit2D::Kind::kSpatialCell;
                hit.priority         = mPriority;
                hit.worldPos         = Dia::Maths::Vector2D(cellMinX + cs * 0.5f, cellMinY + cs * 0.5f);
                hit.spatialCell.x    = cx;
                hit.spatialCell.y    = cy;
                out.Add(hit);
            }
        }
    }

private:
    const Dia::Geometry2D::SpatialGrid<T, MaxObjects>& mGrid;
    Dia::Core::StringCRC    mId;
    int                     mPriority;
    Dia::Picking::PickLayer mLayer;
};

} // namespace Dia::Geometry2DPicking
