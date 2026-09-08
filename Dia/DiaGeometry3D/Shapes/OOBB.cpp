#include "DiaGeometry3D/Shapes/OOBB.h"

namespace Dia::Geometry3D
{
    IntersectionClassify OOBB::IsIntersecting(const Dia::Maths::Vector3D& point) const
    {
        // Transform point to local space via inverse orientation
        Dia::Maths::Quaternion invQ = mOrientation.Inverse();
        Dia::Maths::Vector3D   localPt = invQ.Rotate(point - mCenter);

        if (localPt.X() < -mHalfExtents.X() || localPt.X() > mHalfExtents.X()) return IntersectionClassify::kNoIntersection;
        if (localPt.Y() < -mHalfExtents.Y() || localPt.Y() > mHalfExtents.Y()) return IntersectionClassify::kNoIntersection;
        if (localPt.Z() < -mHalfExtents.Z() || localPt.Z() > mHalfExtents.Z()) return IntersectionClassify::kNoIntersection;
        return IntersectionClassify::kPenetrating;
    }
}
