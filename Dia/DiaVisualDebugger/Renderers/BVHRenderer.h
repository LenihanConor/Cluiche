////////////////////////////////////////////////////////////////////////////////
// Filename: BVHRenderer.h
// Description: Fixed-layer renderer for BVH — no-op if !IsBuilt(); colours nodes
//              by depth % 4: kActive→kGoal→kHealthy→kWarning. Header-only template.
// Feature spec: docs/specs/features/dia/diavisualdebugger/fixed-draw-layer.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IObjectRenderer.h>
#include <DiaVisualDebugger/IFixedPrimitiveBuffer.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaGeometry2D/Spatial/BVH.h>

namespace Dia
{
    namespace Debug
    {
        template<typename T, unsigned int MaxObjects = 2048>
        class BVHRenderer
            : public TypedObjectRenderer<Dia::Geometry2D::BVH<T, MaxObjects>>
        {
        protected:
            void DoBuild(const Dia::Geometry2D::BVH<T, MaxObjects>& bvh,
                         IFixedPrimitiveBuffer& out) const override
            {
                if (!bvh.IsBuilt()) return;

                static const Dia::Graphics::RGBA kDepthColours[4] =
                {
                    DebugColourPalette::kActive,   // depth 0
                    DebugColourPalette::kGoal,     // depth 1
                    DebugColourPalette::kHealthy,  // depth 2
                    DebugColourPalette::kWarning,  // depth 3
                };

                bvh.VisitNodes([&](const Dia::Geometry2D::AARect& bounds, int depth)
                {
                    if (out.IsFull()) return;
                    const Dia::Graphics::RGBA colour = kDepthColours[depth % 4];
                    out.RequestDrawRect(bounds.GetBottomLeft(), bounds.GetTopRight(), colour);
                });
            }
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
