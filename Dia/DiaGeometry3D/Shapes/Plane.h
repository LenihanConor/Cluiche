#pragma once
#ifndef DIA_GEOMETRY3D_PLANE_H
#define DIA_GEOMETRY3D_PLANE_H

#include "DiaMaths/Vector/Vector3D.h"

namespace Dia::Geometry3D
{
    enum class PlaneSide { kFront, kBehind, kOnPlane };

    class Plane
    {
    public:
        Plane();
        Plane(const Plane& rhs);
        Plane(const Dia::Maths::Vector3D& normal, float d);

        Plane& operator=(const Plane& rhs);
        bool   operator==(const Plane& rhs) const;
        bool   operator!=(const Plane& rhs) const;

        static Plane FromPointAndNormal(const Dia::Maths::Vector3D& point,
                                        const Dia::Maths::Vector3D& normal);
        static Plane FromThreePoints(const Dia::Maths::Vector3D& v0,
                                     const Dia::Maths::Vector3D& v1,
                                     const Dia::Maths::Vector3D& v2);

        const Dia::Maths::Vector3D& GetNormal() const;
        float                       GetD()      const;

        float     DistanceTo   (const Dia::Maths::Vector3D& point) const;
        PlaneSide ClassifyPoint(const Dia::Maths::Vector3D& point, float epsilon = 1e-5f) const;

    private:
        Dia::Maths::Vector3D mNormal;
        float                mD;
    };
}

#include "DiaGeometry3D/Shapes/Plane.inl"

#endif
