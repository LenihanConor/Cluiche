#include "GridPassabilityDrawer.h"
#ifdef DIA_DEBUG

#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaPathfinding/CellCoord.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace Pathfinding {

GridPassabilityDrawer::GridPassabilityDrawer(const SquarePathGrid& grid,
                                              const Dia::Core::IDebugContext& ctx,
                                              float cellSize)
    : mGrid(grid), mCtx(ctx), mCellSize(cellSize) {}

Dia::Core::StringCRC GridPassabilityDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("Pathfinding.GridPassability");
}

void GridPassabilityDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    if (!IsEnabled()) return;

    const float scale    = mCtx.GetDebugScale();
    const float halfCell = mCellSize * scale * 0.5f;

    mGrid.VisitCells([&](Dia::Pathfinding::CellCoord coord, bool passable)
    {
        const float cx = static_cast<float>(coord.x) * mCellSize;
        const float cy = static_cast<float>(coord.y) * mCellSize;
        const Dia::Maths::Vector2D min(cx - halfCell, cy - halfCell);
        const Dia::Maths::Vector2D max(cx + halfCell, cy + halfCell);

        if (passable)
            draw.RequestDrawRect(min, max,
                                 Dia::Debug::DebugColourPalette::kHealthy,
                                 Dia::Debug::DebugColourPalette::kGridCellPassable);
        else
            draw.RequestDrawRect(min, max,
                                 Dia::Debug::DebugColourPalette::kError,
                                 Dia::Debug::DebugColourPalette::kGridCellImpassable);
    });
}

} }
#endif // DIA_DEBUG
