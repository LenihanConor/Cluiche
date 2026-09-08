#pragma once
#ifndef DIA_GEOMETRY3D_SHAPEFACTORY_H
#define DIA_GEOMETRY3D_SHAPEFACTORY_H

#include "DiaGeometry3D/Shapes/AABB.h"
#include "DiaGeometry3D/Shapes/Sphere.h"
#include "DiaGeometry3D/Shapes/Triangle.h"
#include "DiaGeometry3D/Shapes/Frustum.h"
#include "DiaGeometry3D/Shapes/Plane.h"
#include "DiaGeometry3D/Shapes/Capsule.h"
#include "DiaGeometry3D/Shapes/Ray.h"
#include "DiaGeometry3D/Shapes/OOBB.h"
#include "DiaGeometry3D/Shapes/Cylinder.h"
#include "DiaMaths/Quaternion/Quaternion.h"

namespace Dia::Geometry3D::Testing
{
    // unit AABB: min=(-1,-1,-1), max=(1,1,1)
    inline AABB MakeUnitAABB()
    {
        return AABB(Dia::Maths::Vector3D(-1.0f, -1.0f, -1.0f),
                    Dia::Maths::Vector3D( 1.0f,  1.0f,  1.0f));
    }

    // unit sphere: center=(0,0,0), radius=1
    inline Sphere MakeUnitSphere()
    {
        return Sphere(Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f), 1.0f);
    }

    // triangle in the XZ-plane, CCW winding, +Y normal
    inline Triangle MakeAxisAlignedTriangle()
    {
        return Triangle(
            Dia::Maths::Vector3D(0.0f, 0.0f,  0.0f),
            Dia::Maths::Vector3D(1.0f, 0.0f,  0.0f),
            Dia::Maths::Vector3D(0.0f, 0.0f, -1.0f)
        );
    }

    // identity frustum: unit cube as six axis-aligned planes, inward normals
    inline Frustum MakeIdentityFrustum()
    {
        return Frustum(); // default constructor is already a unit cube
    }

    // XZ-plane (Y-up), passes through origin
    inline Plane MakeXZPlane()
    {
        return Plane(Dia::Maths::Vector3D(0.0f, 1.0f, 0.0f), 0.0f);
    }

    // Y-axis capsule of given length, centered at origin, radius 0.5
    inline Capsule MakeYAxisCapsule(float length)
    {
        const float half = length * 0.5f;
        return Capsule(
            Dia::Maths::Vector3D(0.0f, -half, 0.0f),
            Dia::Maths::Vector3D(0.0f,  half, 0.0f),
            0.5f
        );
    }
}

#endif
