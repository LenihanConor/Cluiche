#include "DiaGeometry3D/Shapes/AABB.h"

namespace Dia::Geometry3D
{
    IntersectionClassify AABB::IsIntersecting(const Dia::Maths::Vector3D& point) const
    {
        if (point.X() < mMin.X() || point.X() > mMax.X()) return IntersectionClassify::kNoIntersection;
        if (point.Y() < mMin.Y() || point.Y() > mMax.Y()) return IntersectionClassify::kNoIntersection;
        if (point.Z() < mMin.Z() || point.Z() > mMax.Z()) return IntersectionClassify::kNoIntersection;
        return IntersectionClassify::kPenetrating;
    }
}
