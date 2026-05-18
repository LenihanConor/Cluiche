#include "DiaGeometry3D/Shapes/Sphere.h"

namespace Dia::Geometry3D
{
    void Sphere::Encapsulate(const Dia::Maths::Vector3D& point)
    {
        const float dist = mCenter.DistanceTo(point);
        if (dist > mRadius)
            mRadius = dist;
    }

    IntersectionClassify Sphere::IsIntersecting(const Dia::Maths::Vector3D& point) const
    {
        const float sqDist = mCenter.SquareDistanceTo(point);
        if (sqDist > mRadius * mRadius)
            return IntersectionClassify::kNoIntersection;
        return IntersectionClassify::kPenetrating;
    }
}
