#pragma once
#ifndef DIA_GEOMETRY2D_TESTING_SHAPEFACTORY_H
#define DIA_GEOMETRY2D_TESTING_SHAPEFACTORY_H

#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <random>

namespace Dia { namespace Geometry2D { namespace Testing {

// ---------------------------------------------------------------------------
// Named shape factories for common test scenarios
// ---------------------------------------------------------------------------

inline Circle MakeCircle(float x, float y, float radius = 1.0f)
{
    return Circle(radius, Maths::Vector2D(x, y));
}

inline AARect MakeAARect(float minX, float minY, float maxX, float maxY)
{
    return AARect(Maths::Vector2D(minX, minY), Maths::Vector2D(maxX, maxY));
}

inline AARect MakeCentredRect(float cx, float cy, float halfW, float halfH)
{
    return AARect(Maths::Vector2D(cx - halfW, cy - halfH),
                  Maths::Vector2D(cx + halfW, cy + halfH));
}

inline Ray MakeRay(float ox, float oy, float dx, float dy)
{
    return Ray(Maths::Vector2D(ox, oy), Maths::Vector2D(dx, dy));
}

// ---------------------------------------------------------------------------
// Random shape generators for property-based / invariant tests
// Caller supplies an mt19937 so tests are reproducible from a fixed seed.
// ---------------------------------------------------------------------------

inline Circle MakeRandomCircle(std::mt19937& rng, float regionSize = 50.0f, float maxRadius = 5.0f)
{
    std::uniform_real_distribution<float> posRange(-regionSize, regionSize);
    std::uniform_real_distribution<float> radRange(0.1f, maxRadius);
    return Circle(radRange(rng), Maths::Vector2D(posRange(rng), posRange(rng)));
}

inline AARect MakeRandomAARect(std::mt19937& rng, float regionSize = 50.0f, float maxHalf = 5.0f)
{
    std::uniform_real_distribution<float> posRange(-regionSize, regionSize);
    std::uniform_real_distribution<float> sizeRange(0.1f, maxHalf);
    float cx = posRange(rng), cy = posRange(rng);
    float hw = sizeRange(rng), hh = sizeRange(rng);
    return MakeCentredRect(cx, cy, hw, hh);
}

inline Maths::Vector2D MakeRandomPoint(std::mt19937& rng, float regionSize = 50.0f)
{
    std::uniform_real_distribution<float> range(-regionSize, regionSize);
    return Maths::Vector2D(range(rng), range(rng));
}

// Point guaranteed to be inside the given circle
inline Maths::Vector2D MakePointInsideCircle(const Circle& c, std::mt19937& rng)
{
    std::uniform_real_distribution<float> angle(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> radFrac(0.0f, 0.99f);
    float a = angle(rng);
    float r = c.GetRadius() * radFrac(rng);
    return Maths::Vector2D(c.GetCentre().x + r * std::cos(a),
                           c.GetCentre().y + r * std::sin(a));
}

// Point guaranteed to be inside the given rect
inline Maths::Vector2D MakePointInsideAARect(const AARect& rect, std::mt19937& rng)
{
    Maths::Vector2D mn = rect.GetMin();
    Maths::Vector2D mx = rect.GetMax();
    std::uniform_real_distribution<float> rx(mn.x + 1e-4f, mx.x - 1e-4f);
    std::uniform_real_distribution<float> ry(mn.y + 1e-4f, mx.y - 1e-4f);
    return Maths::Vector2D(rx(rng), ry(rng));
}

}}} // namespace Dia::Geometry2D::Testing

#endif // DIA_GEOMETRY2D_TESTING_SHAPEFACTORY_H
