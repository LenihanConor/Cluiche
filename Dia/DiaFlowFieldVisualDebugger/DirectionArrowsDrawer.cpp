#include "DirectionArrowsDrawer.h"
#ifdef DIA_DEBUG

#include <DiaFlowField/FlowField.h>
#include <DiaPathfinding/CellCoord.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace FlowField {

DirectionArrowsDrawer::DirectionArrowsDrawer(const FlowField& field,
                                              const Dia::Core::IDebugContext& ctx,
                                              float cellSize)
    : mField(field), mCtx(ctx), mCellSize(cellSize) {}

Dia::Core::StringCRC DirectionArrowsDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("FlowField.DirectionArrows");
}

void DirectionArrowsDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    if (!IsEnabled()) return;

    const float scale = mCtx.GetDebugScale();
    const float len   = mCellSize * 0.4f * scale * mArrowLengthScale;

    for (int row = 0; row < mField.GetHeight(); ++row)
    {
        for (int col = 0; col < mField.GetWidth(); ++col)
        {
            const auto& cell = mField.Sample(Dia::Pathfinding::CellCoord{col, row});
            if (!cell.reachable) continue;

            const float cx = static_cast<float>(col) * mCellSize + mCellSize * 0.5f;
            const float cy = static_cast<float>(row) * mCellSize + mCellSize * 0.5f;
            const Dia::Maths::Vector2D origin(cx, cy);
            const Dia::Maths::Vector2D end(cx + cell.direction.X() * len,
                                            cy + cell.direction.Y() * len);

            draw.RequestDraw(origin, end, Dia::Debug::DebugColourPalette::kFlowDirection);
        }
    }
}

} }
#endif // DIA_DEBUG
