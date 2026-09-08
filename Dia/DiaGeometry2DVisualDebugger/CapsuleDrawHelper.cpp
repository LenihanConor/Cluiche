////////////////////////////////////////////////////////////////////////////////
// Filename: CapsuleDrawHelper.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGeometry2DVisualDebugger/CapsuleDrawHelper.h"

#ifdef DIA_DEBUG

#include <DiaGeometry2D/Shapes/Capsule.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Core/Trigonometry.h>
#include <DiaMaths/Core/MathsDefines.h>

namespace Dia { namespace Geometry2DVisualDebugger {

Dia::Geometry2D::ConvexPolygon CapsuleToConvexPolygon(const Dia::Geometry2D::Capsule& capsule)
{
    // Build a 12-vertex convex outline: 6 verts for the cap at Pt1 (facing away
    // from Pt2), then 6 verts for the cap at Pt2 (facing away from Pt1).
    //
    // Winding order (CCW looking at the shape):
    //   Cap1: from +perp at Pt1 sweeping 180° through the -axis direction.
    //   Cap2: from -perp at Pt2 sweeping 180° through the +axis direction.
    //
    // 6 verts per cap means 5 angular steps across pi radians (step = pi/5).

    constexpr int kVertsPerCap = 6;
    constexpr int kTotalVerts  = kVertsPerCap * 2;
    static_assert(kTotalVerts <= Dia::Geometry2D::ConvexPolygon::kMaxVertices, "Capsule vertex count exceeds ConvexPolygon limit");

    const Dia::Maths::Vector2D pt1 = capsule.GetPoint1();
    const Dia::Maths::Vector2D pt2 = capsule.GetPoint2();
    const float radius             = capsule.GetRadius();

    // Axis direction pt1→pt2, and the perpendicular.
    const Dia::Maths::Vector2D axisDir = (pt2 - pt1).AsNormalSafe();
    const Dia::Maths::Vector2D perp    = axisDir.AsRotated90DegreeCounterClockwise();

    const float stepRad = Dia::Maths::PI / static_cast<float>(kVertsPerCap - 1);

    Dia::Maths::Vector2D verts[kTotalVerts];

    // Cap at pt1: start at +perp, sweep CCW (through -axis) to -perp.
    // Starting direction is +perp. Each step rotates CCW by stepRad.
    for (int i = 0; i < kVertsPerCap; ++i)
    {
        const Dia::Maths::Angle stepAngle = Dia::Maths::Angle::FromRadians(stepRad * static_cast<float>(i));
        const Dia::Maths::Vector2D dir    = perp.AsRotateCounterClockwiseBy(stepAngle);
        verts[i] = pt1 + (dir * radius);
    }

    // Cap at pt2: start at -perp, sweep CCW (through +axis) to +perp.
    // Starting direction is -perp. Each step rotates CCW by stepRad.
    const Dia::Maths::Vector2D negPerp = perp.AsInverse();
    for (int i = 0; i < kVertsPerCap; ++i)
    {
        const Dia::Maths::Angle stepAngle = Dia::Maths::Angle::FromRadians(stepRad * static_cast<float>(i));
        const Dia::Maths::Vector2D dir    = negPerp.AsRotateCounterClockwiseBy(stepAngle);
        verts[kVertsPerCap + i] = pt2 + (dir * radius);
    }

    return Dia::Geometry2D::ConvexPolygon(verts, kTotalVerts);
}

} } // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
