////////////////////////////////////////////////////////////////////////////////
// Filename: SectorDrawHelper.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGeometry2DVisualDebugger/SectorDrawHelper.h"

#ifdef DIA_DEBUG

#include <DiaGeometry2D/Shapes/Sector.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Core/Trigonometry.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <DiaCore/Core/Assert.h>

namespace Dia { namespace Geometry2DVisualDebugger {

Dia::Geometry2D::ConvexPolygon SectorToConvexPolygon(const Dia::Geometry2D::Sector& sector, int arcSegments)
{
    // Total verts = arcSegments + 1 (centre + arc rim).
    // We need at least 1 arc segment (giving a triangle: centre + 2 rim points).
    const int maxArcSegments = Dia::Geometry2D::ConvexPolygon::kMaxVertices - 1;
    DIA_ASSERT(arcSegments >= 1 && arcSegments <= maxArcSegments,
               "SectorToConvexPolygon: arcSegments must be in [1, 15]");

    const int clamped = (arcSegments < 1) ? 1
                      : (arcSegments > maxArcSegments) ? maxArcSegments
                      : arcSegments;

    // Sector: axis is the centre direction, halfAngle is half the total sweep.
    // CW extent  = axis rotated CW  by halfAngle
    // CCW extent = axis rotated CCW by halfAngle
    const Dia::Maths::Angle  halfAngle  = sector.GetHalfAngle();
    const Dia::Maths::Vector2D startDir = sector.GetAxis().AsRotateClockwiseBy(halfAngle);
    // Compute total sweep in raw radians to avoid Angle normalisation wrapping
    // angles > PI (e.g. a 350° sector would wrap to -10° inside Angle).
    const float totalRad  = halfAngle.AsRadians() * 2.0f;
    const float stepRad   = (clamped > 1) ? (totalRad / static_cast<float>(clamped - 1)) : 0.0f;

    // Layout: [0] = centre, [1..clamped] = arc rim CW→CCW.
    // No explicit close-back vertex: the polygon renderer wraps verts[clamped]→verts[0],
    // which draws the CCW-extent-to-centre radius line automatically.
    const int totalVerts = clamped + 1;
    Dia::Maths::Vector2D verts[Dia::Geometry2D::ConvexPolygon::kMaxVertices];

    verts[0] = sector.GetCenter();

    for (int i = 0; i < clamped; ++i)
    {
        const float angleRad  = stepRad * static_cast<float>(i);
        const Dia::Maths::Angle stepAngle = Dia::Maths::Angle::FromRadians(angleRad);
        const Dia::Maths::Vector2D dir = startDir.AsRotateCounterClockwiseBy(stepAngle);
        verts[1 + i] = sector.GetCenter() + (dir * sector.GetRadius());
    }

    return Dia::Geometry2D::ConvexPolygon(verts, totalVerts);
}

} } // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
