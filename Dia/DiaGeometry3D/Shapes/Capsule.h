#pragma once
#ifndef DIA_GEOMETRY3D_CAPSULE_H
#define DIA_GEOMETRY3D_CAPSULE_H

#include "DiaMaths/Vector/Vector3D.h"
#include "DiaGeometry3D/Shapes/IntersectionClassify.h"

namespace Dia::Geometry3D
{
    class Capsule
    {
    public:
        Capsule();
        Capsule(const Capsule& rhs);
        Capsule(const Dia::Maths::Vector3D& startA, const Dia::Maths::Vector3D& endB, float radius);

        Capsule& operator=(const Capsule& rhs);
        bool     operator==(const Capsule& rhs) const;
        bool     operator!=(const Capsule& rhs) const;

        const Dia::Maths::Vector3D& GetStartA() const;
        const Dia::Maths::Vector3D& GetEndB()   const;
        float                       GetRadius() const;

        float                CalculateLength()        const;
        Dia::Maths::Vector3D CalculateAxis()          const;
        Dia::Maths::Vector3D CalculateAxisDirection() const;

        IntersectionClassify IsIntersecting(const Dia::Maths::Vector3D& point) const;
        bool                 Contains      (const Dia::Maths::Vector3D& point) const;

    private:
        Dia::Maths::Vector3D mStartA;
        Dia::Maths::Vector3D mEndB;
        float                mRadius;
    };
}

#include "DiaGeometry3D/Shapes/Capsule.inl"

#endif
