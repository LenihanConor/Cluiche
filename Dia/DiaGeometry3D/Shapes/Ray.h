#pragma once
#ifndef DIA_GEOMETRY3D_RAY_H
#define DIA_GEOMETRY3D_RAY_H

#include "DiaMaths/Vector/Vector3D.h"

namespace Dia::Geometry3D
{
    class Ray
    {
    public:
        Ray();
        Ray(const Ray& rhs);
        Ray(const Dia::Maths::Vector3D& origin, const Dia::Maths::Vector3D& direction);

        Ray& operator=(const Ray& rhs);
        bool operator==(const Ray& rhs) const;
        bool operator!=(const Ray& rhs) const;

        const Dia::Maths::Vector3D& GetOrigin()    const;
        const Dia::Maths::Vector3D& GetDirection() const;

        Dia::Maths::Vector3D GetPointAt(float t) const;

    private:
        Dia::Maths::Vector3D mOrigin;
        Dia::Maths::Vector3D mDirection;
    };
}

#include "DiaGeometry3D/Shapes/Ray.inl"

#endif
