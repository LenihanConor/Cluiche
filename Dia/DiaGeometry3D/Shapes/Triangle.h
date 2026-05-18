#pragma once
#ifndef DIA_GEOMETRY3D_TRIANGLE_H
#define DIA_GEOMETRY3D_TRIANGLE_H

#include "DiaMaths/Vector/Vector3D.h"

namespace Dia::Geometry3D
{
    class Triangle
    {
    public:
        Triangle();
        Triangle(const Triangle& rhs);
        Triangle(const Dia::Maths::Vector3D& v0,
                 const Dia::Maths::Vector3D& v1,
                 const Dia::Maths::Vector3D& v2);

        Triangle& operator=(const Triangle& rhs);
        bool      operator==(const Triangle& rhs) const;
        bool      operator!=(const Triangle& rhs) const;

        const Dia::Maths::Vector3D& GetV0() const;
        const Dia::Maths::Vector3D& GetV1() const;
        const Dia::Maths::Vector3D& GetV2() const;

        Dia::Maths::Vector3D CalculateNormal()   const;
        float                CalculateArea()     const;
        Dia::Maths::Vector3D CalculateCentroid() const;

    private:
        Dia::Maths::Vector3D mV0;
        Dia::Maths::Vector3D mV1;
        Dia::Maths::Vector3D mV2;
    };
}

#include "DiaGeometry3D/Shapes/Triangle.inl"

#endif
