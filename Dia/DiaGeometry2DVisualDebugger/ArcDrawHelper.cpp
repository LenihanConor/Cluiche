////////////////////////////////////////////////////////////////////////////////
// Filename: ArcDrawHelper.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGeometry2DVisualDebugger/ArcDrawHelper.h"

#ifdef DIA_DEBUG

#include <DiaGeometry2D/Shapes/Arc.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Core/Trigonometry.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <DiaCore/Core/Assert.h>

namespace Dia { namespace Geometry2DVisualDebugger {

Dia::Geometry2D::ConvexPolygon ArcToConvexPolygon(const Dia::Geometry2D::Arc& arc, int segments)
{
    DIA_ASSERT(segments >= 2 && segments <= Dia::Geometry2D::ConvexPolygon::kMaxVertices,
               "ArcToConvexPolygon: segments must be in [2, 16]");

    const int clampedSegments = (segments < 2) ? 2
                              : (segments > Dia::Geometry2D::ConvexPolygon::kMaxVertices)
                                  ? Dia::Geometry2D::ConvexPolygon::kMaxVertices
                                  : segments;

    // The arc is symmetric about its axis. Total sweep = GetAngle().
    // Clockwise extent: axis rotated CW by halfAngle.
    // Counter-clockwise extent: axis rotated CCW by halfAngle.
    const Dia::Maths::Angle halfAngle = arc.GetAngle() * 0.5f;
    const Dia::Maths::Vector2D startDir = arc.GetAxis().AsRotateClockwiseBy(halfAngle);

    // Step size spans the full arc angle across (segments-1) steps.
    const float totalRad   = arc.GetAngle().AsRadians();
    const float stepRad    = (clampedSegments > 1) ? (totalRad / static_cast<float>(clampedSegments - 1)) : 0.0f;

    Dia::Maths::Vector2D verts[Dia::Geometry2D::ConvexPolygon::kMaxVertices];
    for (int i = 0; i < clampedSegments; ++i)
    {
        const float angleRad = stepRad * static_cast<float>(i);
        const Dia::Maths::Angle stepAngle = Dia::Maths::Angle::FromRadians(angleRad);
        const Dia::Maths::Vector2D dir = startDir.AsRotateCounterClockwiseBy(stepAngle);
        verts[i] = arc.GetFocal() + (dir * arc.GetRadius());
    }

    return Dia::Geometry2D::ConvexPolygon(verts, clampedSegments);
}

} } // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
