#include "DiaGeometry3D/Shapes/Capsule.h"

namespace Dia::Geometry3D
{
    IntersectionClassify Capsule::IsIntersecting(const Dia::Maths::Vector3D& point) const
    {
        const Dia::Maths::Vector3D axis   = mEndB - mStartA;
        const float                axisLenSq = axis.SquareMagnitude();

        float t = 0.0f;
        if (axisLenSq > 1e-12f)
        {
            t = (point - mStartA).Dot(axis) / axisLenSq;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
        }

        const Dia::Maths::Vector3D closest = mStartA + axis * t;
        const float sqDist = closest.SquareDistanceTo(point);

        if (sqDist > mRadius * mRadius)
            return IntersectionClassify::kNoIntersection;
        return IntersectionClassify::kPenetrating;
    }
}
