#include "ReachabilityOverlayDrawer.h"
#ifdef DIA_DEBUG

#include <DiaFlowField/FlowField.h>
#include <DiaPathfinding/CellCoord.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace FlowField {

ReachabilityOverlayDrawer::ReachabilityOverlayDrawer(const FlowField& field,
                                                      const Dia::Core::IDebugContext& ctx,
                                                      float cellSize)
    : mField(field), mCtx(ctx), mCellSize(cellSize) {}

Dia::Core::StringCRC ReachabilityOverlayDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("FlowField.ReachabilityOverlay");
}

void ReachabilityOverlayDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    if (!IsEnabled()) return;

    const float scale    = mCtx.GetDebugScale();
    const float halfCell = mCellSize * scale * 0.5f;

    for (int row = 0; row < mField.GetHeight(); ++row)
    {
        for (int col = 0; col < mField.GetWidth(); ++col)
        {
            const auto& cell = mField.Sample(Dia::Pathfinding::CellCoord{col, row});

            const float cx = static_cast<float>(col) * mCellSize + mCellSize * 0.5f;
            const float cy = static_cast<float>(row) * mCellSize + mCellSize * 0.5f;
            const Dia::Maths::Vector2D min(cx - halfCell, cy - halfCell);
            const Dia::Maths::Vector2D max(cx + halfCell, cy + halfCell);

            if (cell.reachable)
                draw.RequestDrawRect(min, max,
                                     Dia::Debug::DebugColourPalette::kHealthy,
                                     Dia::Debug::DebugColourPalette::kGridCellPassable);
            else
                draw.RequestDrawRect(min, max,
                                     Dia::Debug::DebugColourPalette::kError,
                                     Dia::Debug::DebugColourPalette::kGridCellImpassable);
        }
    }
}

} }
#endif // DIA_DEBUG
