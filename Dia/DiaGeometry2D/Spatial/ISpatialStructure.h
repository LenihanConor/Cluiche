#pragma once
#ifndef DIA_GEOMETRY2D_ISPATIALSTRUCTURE_H
#define DIA_GEOMETRY2D_ISPATIALSTRUCTURE_H

#include "DiaCore/Containers/Handle.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/Ray.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia { namespace Geometry2D {

static constexpr unsigned int kMaxQueryResults = 1024;

template<typename T>
class ISpatialStructure
{
public:
    virtual ~ISpatialStructure() = default;

    virtual Dia::Core::Handle<T> Insert(const T& object, const AARect& bounds) = 0;
    virtual void Remove(Dia::Core::Handle<T> handle) = 0;
    virtual void Update(Dia::Core::Handle<T> handle, const AARect& newBounds) = 0;
    virtual void Clear() = 0;

    virtual void QueryRegion  (const AARect& region,               Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
    virtual void QueryCircle  (const Circle& circle,               Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
    virtual void QueryPoint   (const Dia::Maths::Vector2D& point,  Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
    virtual void QueryRay     (const Ray& ray,                     Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
    virtual void QueryKNearest(const Dia::Maths::Vector2D& point, int k, Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;

    virtual const T* Resolve(Dia::Core::Handle<T> handle) const = 0;
};

}} // namespace Dia::Geometry2D

#endif // DIA_GEOMETRY2D_ISPATIALSTRUCTURE_H
