#include "DiaGeometry3D/Shapes/Cylinder.h"

namespace Dia::Geometry3D
{
    IntersectionClassify Cylinder::IsIntersecting(const Dia::Maths::Vector3D& point) const
    {
        const Dia::Maths::Vector3D axis      = mEndB - mStartA;
        const float                axisLenSq = axis.SquareMagnitude();

        if (axisLenSq < 1e-12f)
            return IntersectionClassify::kNoIntersection;

        // Project point onto axis, check axial range
        const Dia::Maths::Vector3D toPoint = point - mStartA;
        const float                t       = toPoint.Dot(axis) / axisLenSq;

        if (t < 0.0f || t > 1.0f)
            return IntersectionClassify::kNoIntersection;

        // Distance from projected point to point on axis
        const Dia::Maths::Vector3D closest  = mStartA + axis * t;
        const float                sqDist   = closest.SquareDistanceTo(point);

        if (sqDist > mRadius * mRadius)
            return IntersectionClassify::kNoIntersection;
        return IntersectionClassify::kPenetrating;
    }
}
