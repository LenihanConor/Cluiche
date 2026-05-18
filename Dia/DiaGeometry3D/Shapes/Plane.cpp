#include "DiaGeometry3D/Shapes/Plane.h"

namespace Dia::Geometry3D
{
    Plane Plane::FromPointAndNormal(const Dia::Maths::Vector3D& point,
                                    const Dia::Maths::Vector3D& normal)
    {
        return Plane(normal.AsNormal(), normal.AsNormal().Dot(point));
    }

    Plane Plane::FromThreePoints(const Dia::Maths::Vector3D& v0,
                                  const Dia::Maths::Vector3D& v1,
                                  const Dia::Maths::Vector3D& v2)
    {
        const Dia::Maths::Vector3D n = (v1 - v0).Cross(v2 - v0).AsNormal();
        return Plane(n, n.Dot(v0));
    }
}
