#include "DiaGeometry3D/Shapes/Frustum.h"
#include "DiaGeometry3D/Shapes/AABB.h"
#include "DiaMaths/Matrix/Matrix44.h"

namespace Dia::Geometry3D
{
    Frustum Frustum::FromMatrix44(const Dia::Maths::Matrix44& m)
    {
        // Gribb-Hartmann plane extraction from a row-major view-projection matrix.
        // Row indices: m[row][col]. Planes extracted as: row3 +/- row0/1/2.
        // Normals point inward; d is the plane constant from dot(n,p)=d form.

        auto extractPlane = [&](float a, float b, float c, float d) -> Plane
        {
            Dia::Maths::Vector3D n(a, b, c);
            const float len = n.Magnitude();
            if (len > 1e-8f)
            {
                n = n * (1.0f / len);
                return Plane(n, -d / len);
            }
            return Plane(n, -d);
        };

        // m.m[row][col] is row-major storage.
        // Gribb-Hartmann: extract planes from row3 +/- row0/1/2.

        return Frustum(
            // near:   row3 + row2
            extractPlane(m.m[3][0] + m.m[2][0], m.m[3][1] + m.m[2][1], m.m[3][2] + m.m[2][2], m.m[3][3] + m.m[2][3]),
            // far:    row3 - row2
            extractPlane(m.m[3][0] - m.m[2][0], m.m[3][1] - m.m[2][1], m.m[3][2] - m.m[2][2], m.m[3][3] - m.m[2][3]),
            // left:   row3 + row0
            extractPlane(m.m[3][0] + m.m[0][0], m.m[3][1] + m.m[0][1], m.m[3][2] + m.m[0][2], m.m[3][3] + m.m[0][3]),
            // right:  row3 - row0
            extractPlane(m.m[3][0] - m.m[0][0], m.m[3][1] - m.m[0][1], m.m[3][2] - m.m[0][2], m.m[3][3] - m.m[0][3]),
            // top:    row3 - row1
            extractPlane(m.m[3][0] - m.m[1][0], m.m[3][1] - m.m[1][1], m.m[3][2] - m.m[1][2], m.m[3][3] - m.m[1][3]),
            // bottom: row3 + row1
            extractPlane(m.m[3][0] + m.m[1][0], m.m[3][1] + m.m[1][1], m.m[3][2] + m.m[1][2], m.m[3][3] + m.m[1][3])
        );
    }

    AABB Frustum::CalculateAABB() const
    {
        // Compute the 8 frustum corners by intersecting plane triples,
        // then return the AABB that encapsulates all corners.
        // Efficient approach: solve each corner as intersection of 3 planes.
        // Corner = intersection of (near/far) x (left/right) x (top/bottom).

        auto intersect3 = [](const Plane& p0, const Plane& p1, const Plane& p2,
                              Dia::Maths::Vector3D& out) -> bool
        {
            const Dia::Maths::Vector3D& n0 = p0.GetNormal();
            const Dia::Maths::Vector3D& n1 = p1.GetNormal();
            const Dia::Maths::Vector3D& n2 = p2.GetNormal();

            const Dia::Maths::Vector3D cross01 = n0.Cross(n1);
            const Dia::Maths::Vector3D cross12 = n1.Cross(n2);
            const Dia::Maths::Vector3D cross20 = n2.Cross(n0);

            const float det = n0.Dot(cross12);
            if (det * det < 1e-12f) return false;

            out = (cross12 * p0.GetD() + cross20 * p1.GetD() + cross01 * p2.GetD()) * (1.0f / det);
            return true;
        };

        const Plane& pNear   = mPlanes[static_cast<int>(FrustumPlane::kNear)];
        const Plane& pFar    = mPlanes[static_cast<int>(FrustumPlane::kFar)];
        const Plane& pLeft   = mPlanes[static_cast<int>(FrustumPlane::kLeft)];
        const Plane& pRight  = mPlanes[static_cast<int>(FrustumPlane::kRight)];
        const Plane& pTop    = mPlanes[static_cast<int>(FrustumPlane::kTop)];
        const Plane& pBottom = mPlanes[static_cast<int>(FrustumPlane::kBottom)];

        Dia::Maths::Vector3D corners[8];
        bool ok = true;
        ok &= intersect3(pNear, pLeft,  pBottom, corners[0]);
        ok &= intersect3(pNear, pRight, pBottom, corners[1]);
        ok &= intersect3(pNear, pLeft,  pTop,    corners[2]);
        ok &= intersect3(pNear, pRight, pTop,    corners[3]);
        ok &= intersect3(pFar,  pLeft,  pBottom, corners[4]);
        ok &= intersect3(pFar,  pRight, pBottom, corners[5]);
        ok &= intersect3(pFar,  pLeft,  pTop,    corners[6]);
        ok &= intersect3(pFar,  pRight, pTop,    corners[7]);

        if (!ok)
            return AABB();

        AABB result(corners[0], corners[0]);
        for (int i = 1; i < 8; ++i)
            result.Encapsulate(corners[i]);
        return result;
    }

    IntersectionClassify Frustum::IsIntersecting(const Dia::Maths::Vector3D& point) const
    {
        for (int i = 0; i < 6; ++i)
        {
            if (mPlanes[i].DistanceTo(point) < 0.0f)
                return IntersectionClassify::kNoIntersection;
        }
        return IntersectionClassify::kPenetrating;
    }
}
