#pragma once
#ifndef DIA_GEOMETRY3D_OOBB_H
#define DIA_GEOMETRY3D_OOBB_H

#include "DiaMaths/Vector/Vector3D.h"
#include "DiaMaths/Quaternion/Quaternion.h"
#include "DiaGeometry3D/Shapes/IntersectionClassify.h"

namespace Dia::Geometry3D
{
    class OOBB
    {
    public:
        OOBB();
        OOBB(const OOBB& rhs);
        OOBB(const Dia::Maths::Vector3D& center,
             const Dia::Maths::Vector3D& halfExtents,
             const Dia::Maths::Quaternion& orientation);

        OOBB& operator=(const OOBB& rhs);
        bool  operator==(const OOBB& rhs) const;
        bool  operator!=(const OOBB& rhs) const;

        const Dia::Maths::Vector3D&   GetCenter()      const;
        const Dia::Maths::Vector3D&   GetHalfExtents() const;
        const Dia::Maths::Quaternion& GetOrientation() const;

        void GetAxes(Dia::Maths::Vector3D& outX,
                     Dia::Maths::Vector3D& outY,
                     Dia::Maths::Vector3D& outZ) const;

        IntersectionClassify IsIntersecting(const Dia::Maths::Vector3D& point) const;
        bool                 Contains      (const Dia::Maths::Vector3D& point) const;

    private:
        Dia::Maths::Vector3D   mCenter;
        Dia::Maths::Vector3D   mHalfExtents;
        Dia::Maths::Quaternion mOrientation;
    };
}

#include "DiaGeometry3D/Shapes/OOBB.inl"

#endif
