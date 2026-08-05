#pragma once
#ifndef DIA_ENTITYSPATIAL_ENTITYSPATIALMODULE_H
#define DIA_ENTITYSPATIAL_ENTITYSPATIALMODULE_H

#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaEntitySpatial/EntitySpatialIndex.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaGeometry2D/Spatial/ISpatialStructure.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/Handle.h>

namespace Dia::EntitySpatial {

// EntitySpatialModule — runtime module that owns an EntitySpatialIndex and maintains
// the dirty-flag sweep each frame. Entities opt in via SpatialComponent.
//
// Non-copyable, non-movable. One instance per domain.
// All query methods are non-reentrant — they share mResolveBuffer.
class EntitySpatialModule {
public:
    // Matches Dia::Geometry2D::kMaxQueryResults (1024)
    static constexpr unsigned int kMaxQueryResults = Dia::Geometry2D::kMaxQueryResults;

    // Construct with a SquareGrid index topology.
    EntitySpatialModule(Dia::Entity::Domain& domain,
                        const EntitySpatialIndex::SquareDef& indexDef);

    // Construct with a HexGrid index topology.
    EntitySpatialModule(Dia::Entity::Domain& domain,
                        const EntitySpatialIndex::HexDef& indexDef);

    ~EntitySpatialModule() = default;

    EntitySpatialModule(const EntitySpatialModule&) = delete;
    EntitySpatialModule& operator=(const EntitySpatialModule&) = delete;
    EntitySpatialModule(EntitySpatialModule&&) = delete;
    EntitySpatialModule& operator=(EntitySpatialModule&&) = delete;

    // Call once per frame: dirty-flag sweep (pass 1) then detach/destroy sweep (pass 2).
    void Update();

    // --- Query methods ---
    // All take caller-owned out buffers — no heap allocation on query path.
    // Results: live entities, geometrically exact, matching layer mask.
    // Non-reentrant: each call reuses the internal mResolveBuffer.

    // Returns all entities whose circles overlap the query circle.
    template<unsigned int N>
    void QueryCircle(const Dia::Geometry2D::Circle& circle,
                     uint32_t mask,
                     Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, N>& out) const;

    // Returns all entities whose AARect bounds overlap the query region.
    template<unsigned int N>
    void QueryRegion(const Dia::Geometry2D::AARect& region,
                     uint32_t mask,
                     Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, N>& out) const;

    // Returns the k-nearest entities to origin (ascending distance), applying mask and exact rejection.
    template<unsigned int N>
    void QueryKNearest(const Dia::Maths::Vector2D& origin,
                       int k,
                       uint32_t mask,
                       Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, N>& out) const;

    // Returns all entities whose circles are intersected by the ray segment [origin, origin+dir*maxDist].
    template<unsigned int N>
    void QueryRay(const Dia::Geometry2D::Ray& ray,
                  float maxDist,
                  uint32_t mask,
                  Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, N>& out) const;

    // Returns all entities within a circular sector: inside radius AND within halfAngle of dir.
    // dir must be a unit vector. halfAngle is in radians.
    template<unsigned int N>
    void QuerySector(const Dia::Maths::Vector2D& origin,
                     const Dia::Maths::Vector2D& dir,
                     float radius,
                     float halfAngle,
                     uint32_t mask,
                     Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, N>& out) const;

private:
    // Slot handle map: mSpatialHandles[entitySlotIndex] = structure's internal slot handle.
    // Invalid handle = entity not (yet) inserted.
    using SpatialHandle = Dia::Core::Handle<Dia::Entity::Entity>;
    Dia::Core::Containers::DynamicArrayC<SpatialHandle, Dia::Entity::kMaxEntitiesPerDomain> mSpatialHandles;

    // Reused intermediate buffer for all query broad-phase results (non-reentrant).
    mutable Dia::Core::Containers::DynamicArrayC<SpatialHandle, kMaxQueryResults> mResolveBuffer;

    Dia::Entity::Domain& mDomain;
    EntitySpatialIndex   mIndex;
};

} // namespace Dia::EntitySpatial

#include <DiaEntitySpatial/EntitySpatialModule.inl>

#endif // DIA_ENTITYSPATIAL_ENTITYSPATIALMODULE_H
