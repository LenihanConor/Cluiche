#pragma once
#ifndef DIA_GEOMETRY3D_AABB_H
#define DIA_GEOMETRY3D_AABB_H

#include "DiaMaths/Vector/Vector3D.h"
#include "DiaGeometry3D/Shapes/IntersectionClassify.h"

namespace Dia::Geometry3D
{
    class Sphere;

    class AABB
    {
    public:
        AABB();
        AABB(const AABB& rhs);
        AABB(const Dia::Maths::Vector3D& min, const Dia::Maths::Vector3D& max);

        AABB& operator=(const AABB& rhs);
        bool  operator==(const AABB& rhs) const;
        bool  operator!=(const AABB& rhs) const;

        static AABB FromCenterExtents(const Dia::Maths::Vector3D& center, const Dia::Maths::Vector3D& extents);

        const Dia::Maths::Vector3D& GetMin() const;
        const Dia::Maths::Vector3D& GetMax() const;

        Dia::Maths::Vector3D CalculateCenter()      const;
        Dia::Maths::Vector3D CalculateExtents()     const;
        float                CalculateSurfaceArea() const;
        float                CalculateVolume()      const;

        void Encapsulate(const Dia::Maths::Vector3D& point);
        void Encapsulate(const AABB& other);

        IntersectionClassify IsIntersecting(const Dia::Maths::Vector3D& point) const;
        bool                 Contains      (const Dia::Maths::Vector3D& point) const;

    private:
        Dia::Maths::Vector3D mMin;
        Dia::Maths::Vector3D mMax;
    };
}

#include "DiaGeometry3D/Shapes/AABB.inl"

#endif
