////////////////////////////////////////////////////////////////////////////////
// Filename: SpatialGridRenderer.h
// Description: Fixed-layer renderer for SpatialGrid — emits one Rect2D per cell
//              in kInactive (grey). Header-only template; callers must also depend
//              on DiaGeometry2D.
// Feature spec: docs/specs/features/dia/diavisualdebugger/fixed-draw-layer.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IObjectRenderer.h>
#include <DiaVisualDebugger/IFixedPrimitiveBuffer.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
    namespace Debug
    {
        template<typename T, unsigned int MaxObjects = 2048>
        class SpatialGridRenderer
            : public TypedObjectRenderer<Dia::Geometry2D::SpatialGrid<T, MaxObjects>>
        {
        protected:
            void DoBuild(const Dia::Geometry2D::SpatialGrid<T, MaxObjects>& grid,
                         IFixedPrimitiveBuffer& out) const override
            {
                const int   countX   = grid.GetCellCountX();
                const int   countY   = grid.GetCellCountY();
                const float cellSize = grid.GetCellSize();
                const Dia::Geometry2D::AARect& worldBounds = grid.GetWorldBounds();
                const float blX = worldBounds.GetBottomLeft().x;
                const float blY = worldBounds.GetBottomLeft().y;

                const Dia::Graphics::RGBA colour = DebugColourPalette::kInactive;

                for (int cy = 0; cy < countY && !out.IsFull(); ++cy)
                {
                    for (int cx = 0; cx < countX && !out.IsFull(); ++cx)
                    {
                        const float minX = blX + static_cast<float>(cx)     * cellSize;
                        const float minY = blY + static_cast<float>(cy)     * cellSize;
                        const float maxX = blX + static_cast<float>(cx + 1) * cellSize;
                        const float maxY = blY + static_cast<float>(cy + 1) * cellSize;
                        out.RequestDrawRect(
                            Dia::Maths::Vector2D(minX, minY),
                            Dia::Maths::Vector2D(maxX, maxY),
                            colour);
                    }
                }
            }
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
