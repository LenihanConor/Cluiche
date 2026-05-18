#pragma once
#ifndef DIA_GEOMETRY3D_FRUSTUM_H
#define DIA_GEOMETRY3D_FRUSTUM_H

#include "DiaMaths/Vector/Vector3D.h"
#include "DiaGeometry3D/Shapes/Plane.h"
#include "DiaGeometry3D/Shapes/IntersectionClassify.h"

namespace Dia::Maths { class Matrix44; }
namespace Dia::Geometry3D { class AABB; }

namespace Dia::Geometry3D
{
    enum class FrustumPlane : int
    {
        kNear   = 0,
        kFar    = 1,
        kLeft   = 2,
        kRight  = 3,
        kTop    = 4,
        kBottom = 5,
    };

    class Frustum
    {
    public:
        Frustum();
        Frustum(const Frustum& rhs);
        Frustum(const Plane& near, const Plane& far,
                const Plane& left, const Plane& right,
                const Plane& top,  const Plane& bottom);

        Frustum& operator=(const Frustum& rhs);
        bool     operator==(const Frustum& rhs) const;
        bool     operator!=(const Frustum& rhs) const;

        static Frustum FromMatrix44(const Dia::Maths::Matrix44& viewProjection);

        const Plane& GetPlane(FrustumPlane slot) const;

        AABB CalculateAABB() const;

        IntersectionClassify IsIntersecting(const Dia::Maths::Vector3D& point) const;
        bool                 Contains      (const Dia::Maths::Vector3D& point) const;

    private:
        Plane mPlanes[6];
    };
}

#include "DiaGeometry3D/Shapes/Frustum.inl"

#endif
