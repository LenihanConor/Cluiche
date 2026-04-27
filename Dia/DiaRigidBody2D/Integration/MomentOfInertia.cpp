#include "DiaRigidBody2D/Integration/MomentOfInertia.h"

#include "DiaGeometry2D/Shapes/ConvexPolygon.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::RigidBody2D {

float MomentOfInertia::ForCircle(float mass, float radius)
{
    return 0.5f * mass * radius * radius;
}

float MomentOfInertia::ForAARect(float mass, float width, float height)
{
    return mass * (width * width + height * height) / 12.0f;
}

float MomentOfInertia::ForTriangle(float mass, float base, float height)
{
    return mass * (base * base + height * height) / 18.0f;
}

float MomentOfInertia::ForConvexPolygon(float mass, const Dia::Geometry2D::ConvexPolygon& poly)
{
    const int count = poly.GetVertexCount();
    if (count < 3) return 0.0f;

    // Decompose into triangles from centroid, accumulate mass-weighted inertia
    float totalArea = 0.0f;
    float inertia   = 0.0f;

    for (int i = 0; i < count; ++i)
    {
        const Dia::Maths::Vector2D& a = poly.GetVertex(i);
        const Dia::Maths::Vector2D& b = poly.GetVertex((i + 1) % count);

        float triArea = 0.5f * (a.x * b.y - b.x * a.y);
        if (triArea < 0.0f) triArea = -triArea;

        float triInertia = triArea * (a.x * a.x + a.y * a.y +
                                      b.x * b.x + b.y * b.y +
                                      a.x * b.x + a.y * b.y) / 6.0f;

        totalArea += triArea;
        inertia   += triInertia;
    }

    if (totalArea > 0.0f)
        inertia *= mass / totalArea;

    return inertia;
}

} // namespace Dia::RigidBody2D
