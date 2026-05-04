////////////////////////////////////////////////////////////////////////////////
// Filename: HexGridRenderer.h
// Description: Fixed-layer renderer for HexGrid — draws 6 edge lines per valid
//              hex cell in kInactive (grey). Header-only template.
// Feature spec: docs/specs/features/dia/diavisualdebugger/fixed-draw-layer.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IObjectRenderer.h>
#include <DiaVisualDebugger/IFixedPrimitiveBuffer.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaGeometry2D/Spatial/HexGrid.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <cmath>

namespace Dia
{
    namespace Debug
    {
        template<typename T, unsigned int MaxObjects = 2048>
        class HexGridRenderer
            : public TypedObjectRenderer<Dia::Geometry2D::HexGrid<T, MaxObjects>>
        {
        protected:
            void DoBuild(const Dia::Geometry2D::HexGrid<T, MaxObjects>& grid,
                         IFixedPrimitiveBuffer& out) const override
            {
                const Dia::Graphics::RGBA colour = DebugColourPalette::kInactive;

                static constexpr float kAngles[6] =
                {
                    Dia::Maths::PI / 6.0f,
                    Dia::Maths::PI / 2.0f,
                    5.0f * Dia::Maths::PI / 6.0f,
                    7.0f * Dia::Maths::PI / 6.0f,
                    3.0f * Dia::Maths::PI / 2.0f,
                    11.0f * Dia::Maths::PI / 6.0f
                };

                const float hexRadius = grid.GetHexRadius();
                const int   minQ      = grid.GetMinQ();
                const int   minR      = grid.GetMinR();
                const int   colCount  = grid.GetColCount();
                const int   rowCount  = grid.GetRowCount();

                for (int r = 0; r < rowCount; ++r)
                {
                    for (int q = 0; q < colCount; ++q)
                    {
                        const Dia::Geometry2D::HexCoord coord{ minQ + q, minR + r };
                        if (!grid.IsValidHex(coord)) continue;

                        const Dia::Maths::Vector2D center = grid.HexToWorld(coord);

                        Dia::Maths::Vector2D corners[6];
                        for (int k = 0; k < 6; ++k)
                        {
                            corners[k] = Dia::Maths::Vector2D(
                                center.x + hexRadius * std::cos(kAngles[k]),
                                center.y + hexRadius * std::sin(kAngles[k]));
                        }

                        for (int k = 0; k < 6 && !out.IsFull(); ++k)
                        {
                            out.RequestDraw(corners[k], corners[(k + 1) % 6], colour);
                        }
                    }
                }
            }
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
