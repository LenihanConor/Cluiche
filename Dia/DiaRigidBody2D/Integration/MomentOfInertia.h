#pragma once

#include "DiaGeometry2D/Shapes/ConvexPolygon.h"

namespace Dia::RigidBody2D {

struct MomentOfInertia {
    // Circle: I = 0.5 * m * r²
    static float ForCircle(float mass, float radius);

    // Axis-aligned rectangle: I = m * (w² + h²) / 12
    static float ForAARect(float mass, float width, float height);

    // Triangle (base × height): I = m * (b² + h²) / 18
    static float ForTriangle(float mass, float base, float height);

    // Convex polygon (decomposed into triangles from centroid)
    static float ForConvexPolygon(float mass, const Dia::Geometry2D::ConvexPolygon& poly);
};

} // namespace Dia::RigidBody2D
