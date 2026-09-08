////////////////////////////////////////////////////////////////////////////////
// Filename: OORectDrawHelper.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGeometry2DVisualDebugger/OORectDrawHelper.h"

#ifdef DIA_DEBUG

#include <DiaGeometry2D/Shapes/OORect.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace Geometry2DVisualDebugger {

Dia::Geometry2D::ConvexPolygon OORectToConvexPolygon(const Dia::Geometry2D::OORect& rect)
{
    // OORect exposes 4 corners via GetPt(PtId) in order Pt0..Pt3.
    // Map them directly to a 4-vertex ConvexPolygon.
    const Dia::Maths::Vector2D verts[4] =
    {
        rect.GetPt(Dia::Geometry2D::OORect::kPt0),
        rect.GetPt(Dia::Geometry2D::OORect::kPt1),
        rect.GetPt(Dia::Geometry2D::OORect::kPt2),
        rect.GetPt(Dia::Geometry2D::OORect::kPt3),
    };
    return Dia::Geometry2D::ConvexPolygon(verts, 4);
}

} } // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
