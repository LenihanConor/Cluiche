#pragma once
#ifndef DIA_GEOMETRY3D_SPHERE_H
#define DIA_GEOMETRY3D_SPHERE_H

#include "DiaMaths/Vector/Vector3D.h"
#include "DiaGeometry3D/Shapes/IntersectionClassify.h"

namespace Dia::Geometry3D
{
    class Sphere
    {
    public:
        Sphere();
        Sphere(const Sphere& rhs);
        Sphere(const Dia::Maths::Vector3D& center, float radius);

        Sphere& operator=(const Sphere& rhs);
        bool    operator==(const Sphere& rhs) const;
        bool    operator!=(const Sphere& rhs) const;

        const Dia::Maths::Vector3D& GetCenter() const;
        float                       GetRadius() const;

        float CalculateVolume()      const;
        float CalculateSurfaceArea() const;

        void Encapsulate(const Dia::Maths::Vector3D& point);

        IntersectionClassify IsIntersecting(const Dia::Maths::Vector3D& point) const;
        bool                 Contains      (const Dia::Maths::Vector3D& point) const;

    private:
        Dia::Maths::Vector3D mCenter;
        float                mRadius;
    };
}

#include "DiaGeometry3D/Shapes/Sphere.inl"

#endif
