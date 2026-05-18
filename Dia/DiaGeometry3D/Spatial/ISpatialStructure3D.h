#pragma once
#ifndef DIA_GEOMETRY3D_ISPATIALSTRUCTURE3D_H
#define DIA_GEOMETRY3D_ISPATIALSTRUCTURE3D_H

#include "DiaCore/Containers/Handle.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaMaths/Vector/Vector3D.h"
#include "DiaGeometry3D/Shapes/AABB.h"
#include "DiaGeometry3D/Shapes/Sphere.h"
#include "DiaGeometry3D/Shapes/Ray.h"
#include "DiaGeometry3D/Shapes/Frustum.h"

namespace Dia::Geometry3D
{
    static constexpr unsigned int kMaxQueryResults = 1024;

    template<typename T>
    class ISpatialStructure3D
    {
    public:
        virtual ~ISpatialStructure3D() = default;

        virtual Dia::Core::Handle<T> Insert(const T& object, const AABB& bounds) = 0;
        virtual void Remove(Dia::Core::Handle<T> handle) = 0;
        virtual void Update(Dia::Core::Handle<T> handle, const AABB& newBounds) = 0;
        virtual void Clear() = 0;

        virtual void QueryRegion  (const AABB&                    region,  Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
        virtual void QuerySphere  (const Sphere&                  sphere,  Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
        virtual void QueryPoint   (const Dia::Maths::Vector3D&    point,   Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
        virtual void QueryRay     (const Ray&                     ray,     Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
        virtual void QueryFrustum (const Frustum&                 frustum, Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
        virtual void QueryKNearest(const Dia::Maths::Vector3D&    point, int k, Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;

        virtual const T* Resolve(Dia::Core::Handle<T> handle) const = 0;
    };
}

#endif
