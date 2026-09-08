#pragma once

#include <DiaEntity/Entity.h>
#include <DiaGeometry2D/Spatial/ISpatialStructure.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Spatial/HexGrid.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Handle.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <cstddef>
#include <new>

namespace Dia::EntitySpatial {

class EntitySpatialIndex {
public:
    enum class Topology { SquareGrid, HexGrid };

    struct SquareDef {
        Dia::Geometry2D::AARect worldBounds = {};
        float cellSize = 1.0f;
    };

    struct HexDef {
        Dia::Maths::Vector2D origin = {};
        int colCount = 1;
        int rowCount = 1;
        float hexRadius = 1.0f;
    };

    explicit EntitySpatialIndex(const SquareDef& def);
    explicit EntitySpatialIndex(const HexDef& def);
    ~EntitySpatialIndex();

    // Non-copyable, non-movable (owns placement-new storage)
    EntitySpatialIndex(const EntitySpatialIndex&) = delete;
    EntitySpatialIndex& operator=(const EntitySpatialIndex&) = delete;
    EntitySpatialIndex(EntitySpatialIndex&&) = delete;
    EntitySpatialIndex& operator=(EntitySpatialIndex&&) = delete;

    Topology GetTopology() const;

    Dia::Core::Handle<Dia::Entity::Entity> Insert(const Dia::Entity::Entity& entity, const Dia::Geometry2D::AARect& bounds);
    void Remove(Dia::Core::Handle<Dia::Entity::Entity> handle);
    void Update(Dia::Core::Handle<Dia::Entity::Entity> handle, const Dia::Geometry2D::AARect& newBounds);
    void Clear();

    void QueryRegion  (const Dia::Geometry2D::AARect&  region,
                       Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Dia::Entity::Entity>, Dia::Geometry2D::kMaxQueryResults>& out) const;
    void QueryCircle  (const Dia::Geometry2D::Circle&  circle,
                       Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Dia::Entity::Entity>, Dia::Geometry2D::kMaxQueryResults>& out) const;
    void QueryPoint   (const Dia::Maths::Vector2D&     point,
                       Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Dia::Entity::Entity>, Dia::Geometry2D::kMaxQueryResults>& out) const;
    void QueryRay     (const Dia::Geometry2D::Ray&     ray,
                       Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Dia::Entity::Entity>, Dia::Geometry2D::kMaxQueryResults>& out) const;
    void QueryKNearest(const Dia::Maths::Vector2D&     point, int k,
                       Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Dia::Entity::Entity>, Dia::Geometry2D::kMaxQueryResults>& out) const;

    const Dia::Entity::Entity* Resolve(Dia::Core::Handle<Dia::Entity::Entity> handle) const;

private:
    using SquareGridType = Dia::Geometry2D::SpatialGrid<Dia::Entity::Entity, Dia::Entity::kMaxEntitiesPerDomain>;
    using HexGridType    = Dia::Geometry2D::HexGrid   <Dia::Entity::Entity, Dia::Entity::kMaxEntitiesPerDomain>;

    static constexpr size_t kStorageSize  = sizeof(SquareGridType)   > sizeof(HexGridType)   ? sizeof(SquareGridType)   : sizeof(HexGridType);
    static constexpr size_t kStorageAlign = alignof(SquareGridType)  > alignof(HexGridType)  ? alignof(SquareGridType)  : alignof(HexGridType);

    alignas(kStorageAlign) unsigned char mStorage[kStorageSize];
    Dia::Geometry2D::ISpatialStructure<Dia::Entity::Entity>* mStructure = nullptr;
    Topology mTopology;
};

// ---------------------------------------------------------------------------
// Inline implementations
// ---------------------------------------------------------------------------

inline EntitySpatialIndex::EntitySpatialIndex(const SquareDef& def)
    : mTopology(Topology::SquareGrid)
{
    SquareGridType::Def gridDef;
    gridDef.worldBounds = def.worldBounds;
    gridDef.cellSize    = def.cellSize;
    mStructure = new (mStorage) SquareGridType(gridDef);
}

inline EntitySpatialIndex::EntitySpatialIndex(const HexDef& def)
    : mTopology(Topology::HexGrid)
{
    HexGridType::Def hexDef;
    hexDef.origin    = def.origin;
    hexDef.colCount  = def.colCount;
    hexDef.rowCount  = def.rowCount;
    hexDef.hexRadius = def.hexRadius;
    mStructure = new (mStorage) HexGridType(hexDef);
}

inline EntitySpatialIndex::~EntitySpatialIndex()
{
    if (mTopology == Topology::SquareGrid)
        reinterpret_cast<SquareGridType*>(mStorage)->~SquareGridType();
    else
        reinterpret_cast<HexGridType*>(mStorage)->~HexGridType();
    mStructure = nullptr;
}

inline EntitySpatialIndex::Topology EntitySpatialIndex::GetTopology() const
{
    return mTopology;
}

inline Dia::Core::Handle<Dia::Entity::Entity> EntitySpatialIndex::Insert(const Dia::Entity::Entity& entity, const Dia::Geometry2D::AARect& bounds)
{
    return mStructure->Insert(entity, bounds);
}

inline void EntitySpatialIndex::Remove(Dia::Core::Handle<Dia::Entity::Entity> handle)
{
    mStructure->Remove(handle);
}

inline void EntitySpatialIndex::Update(Dia::Core::Handle<Dia::Entity::Entity> handle, const Dia::Geometry2D::AARect& newBounds)
{
    mStructure->Update(handle, newBounds);
}

inline void EntitySpatialIndex::Clear()
{
    mStructure->Clear();
}

inline void EntitySpatialIndex::QueryRegion(
    const Dia::Geometry2D::AARect& region,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Dia::Entity::Entity>, Dia::Geometry2D::kMaxQueryResults>& out) const
{
    mStructure->QueryRegion(region, out);
}

inline void EntitySpatialIndex::QueryCircle(
    const Dia::Geometry2D::Circle& circle,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Dia::Entity::Entity>, Dia::Geometry2D::kMaxQueryResults>& out) const
{
    mStructure->QueryCircle(circle, out);
}

inline void EntitySpatialIndex::QueryPoint(
    const Dia::Maths::Vector2D& point,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Dia::Entity::Entity>, Dia::Geometry2D::kMaxQueryResults>& out) const
{
    mStructure->QueryPoint(point, out);
}

inline void EntitySpatialIndex::QueryRay(
    const Dia::Geometry2D::Ray& ray,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Dia::Entity::Entity>, Dia::Geometry2D::kMaxQueryResults>& out) const
{
    mStructure->QueryRay(ray, out);
}

inline void EntitySpatialIndex::QueryKNearest(
    const Dia::Maths::Vector2D& point, int k,
    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<Dia::Entity::Entity>, Dia::Geometry2D::kMaxQueryResults>& out) const
{
    mStructure->QueryKNearest(point, k, out);
}

inline const Dia::Entity::Entity* EntitySpatialIndex::Resolve(Dia::Core::Handle<Dia::Entity::Entity> handle) const
{
    return mStructure->Resolve(handle);
}

} // namespace Dia::EntitySpatial
