////////////////////////////////////////////////////////////////////////////////
// Filename: QuadtreeRenderer.h
// Description: Fixed-layer renderer for Quadtree — leaf nodes kActive (white),
//              internal nodes kInactive (grey). Header-only template.
// Feature spec: docs/specs/features/dia/diavisualdebugger/fixed-draw-layer.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IObjectRenderer.h>
#include <DiaVisualDebugger/IFixedPrimitiveBuffer.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaGeometry2D/Spatial/Quadtree.h>

namespace Dia
{
    namespace Debug
    {
        template<typename T, unsigned int MaxObjects = 2048>
        class QuadtreeRenderer
            : public TypedObjectRenderer<Dia::Geometry2D::Quadtree<T, MaxObjects>>
        {
        protected:
            void DoBuild(const Dia::Geometry2D::Quadtree<T, MaxObjects>& tree,
                         IFixedPrimitiveBuffer& out) const override
            {
                tree.VisitNodes([&](const Dia::Geometry2D::AARect& bounds, bool isLeaf)
                {
                    if (out.IsFull()) return;
                    const Dia::Graphics::RGBA colour = isLeaf
                        ? DebugColourPalette::kActive
                        : DebugColourPalette::kInactive;
                    out.RequestDrawRect(bounds.GetBottomLeft(), bounds.GetTopRight(), colour);
                });
            }
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
