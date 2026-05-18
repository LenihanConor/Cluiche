#pragma once
#ifndef DIA_GEOMETRY3D_INTERSECTIONTESTS_H
#define DIA_GEOMETRY3D_INTERSECTIONTESTS_H

#include "DiaMaths/Vector/Vector3D.h"
#include "DiaGeometry3D/Shapes/IntersectionClassify.h"
#include "DiaGeometry3D/Shapes/AABB.h"
#include "DiaGeometry3D/Shapes/OOBB.h"
#include "DiaGeometry3D/Shapes/Sphere.h"
#include "DiaGeometry3D/Shapes/Capsule.h"
#include "DiaGeometry3D/Shapes/Cylinder.h"
#include "DiaGeometry3D/Shapes/Triangle.h"
#include "DiaGeometry3D/Shapes/Ray.h"
#include "DiaGeometry3D/Shapes/Plane.h"
#include "DiaGeometry3D/Shapes/Frustum.h"

namespace Dia::Geometry3D
{
    class IntersectionTests
    {
    public:
        // Bounding-volume vs Frustum (priority — culling driver)
        static IntersectionClassify Test(const AABB&     a, const Frustum& b);
        static IntersectionClassify Test(const Sphere&   a, const Frustum& b);
        static IntersectionClassify Test(const Triangle& a, const Frustum& b);
        static IntersectionClassify Test(const Frustum& a, const AABB&     b);
        static IntersectionClassify Test(const Frustum& a, const Sphere&   b);
        static IntersectionClassify Test(const Frustum& a, const Triangle& b);

        // Bounding-volume pairs
        static IntersectionClassify Test(const AABB&   a, const AABB&   b);
        static IntersectionClassify Test(const AABB&   a, const Sphere& b);
        static IntersectionClassify Test(const Sphere& a, const AABB&   b);
        static IntersectionClassify Test(const Sphere& a, const Sphere& b);
        static IntersectionClassify Test(const AABB&   a, const OOBB&   b);
        static IntersectionClassify Test(const OOBB&   a, const AABB&   b);
        static IntersectionClassify Test(const OOBB&   a, const OOBB&   b);

        // Triangle-vs-volume (mesh culling)
        static IntersectionClassify Test(const Triangle& a, const AABB&     b);
        static IntersectionClassify Test(const AABB&     a, const Triangle& b);
        static IntersectionClassify Test(const Triangle& a, const Sphere&   b);
        static IntersectionClassify Test(const Sphere&   a, const Triangle& b);

        // Ray casts
        static IntersectionClassify Test(const Ray& a, const AABB&     b);
        static IntersectionClassify Test(const Ray& a, const Sphere&   b);
        static IntersectionClassify Test(const Ray& a, const Triangle& b);
        static IntersectionClassify Test(const Ray& a, const Plane&    b);

        // Point containment
        static bool Contains(const AABB&     shape, const Dia::Maths::Vector3D& point);
        static bool Contains(const Sphere&   shape, const Dia::Maths::Vector3D& point);
        static bool Contains(const OOBB&     shape, const Dia::Maths::Vector3D& point);
        static bool Contains(const Capsule&  shape, const Dia::Maths::Vector3D& point);
        static bool Contains(const Cylinder& shape, const Dia::Maths::Vector3D& point);
        static bool Contains(const Frustum&  shape, const Dia::Maths::Vector3D& point);

        // Closest point
        static Dia::Maths::Vector3D ClosestPoint(const AABB&     shape, const Dia::Maths::Vector3D& point);
        static Dia::Maths::Vector3D ClosestPoint(const Sphere&   shape, const Dia::Maths::Vector3D& point);
        static Dia::Maths::Vector3D ClosestPoint(const OOBB&     shape, const Dia::Maths::Vector3D& point);
        static Dia::Maths::Vector3D ClosestPoint(const Triangle& shape, const Dia::Maths::Vector3D& point);
        static Dia::Maths::Vector3D ClosestPoint(const Capsule&  shape, const Dia::Maths::Vector3D& point);
        static Dia::Maths::Vector3D ClosestPoint(const Cylinder& shape, const Dia::Maths::Vector3D& point);
    };
}

#endif
