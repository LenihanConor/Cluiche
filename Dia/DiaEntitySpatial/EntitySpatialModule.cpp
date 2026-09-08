#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaCore/Core/Assert.h>

namespace Dia::EntitySpatial {

EntitySpatialModule::EntitySpatialModule(Dia::Entity::Domain& domain,
                                         const EntitySpatialIndex::SquareDef& indexDef)
    : mDomain(domain)
    , mIndex(indexDef)
{
    // Pre-fill mSpatialHandles with Invalid handles — one slot per possible entity index.
    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain; ++i)
        mSpatialHandles.Add(SpatialHandle::Invalid());
}

EntitySpatialModule::EntitySpatialModule(Dia::Entity::Domain& domain,
                                         const EntitySpatialIndex::HexDef& indexDef)
    : mDomain(domain)
    , mIndex(indexDef)
{
    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain; ++i)
        mSpatialHandles.Add(SpatialHandle::Invalid());
}

void EntitySpatialModule::Update()
{
    auto view = mDomain.Query<SpatialComponent>();

    // --- Pass 1: dirty sweep ---
    // Re-index any entity whose dirty bit (bit 31 of mLayerMask) is set.
    for (auto it = view.begin(); it != view.end(); ++it)
    {
        const auto entry = *it;
        const Dia::Entity::Entity entity = entry.entity;
        SpatialComponent* spatial = std::get<0>(entry.components);

        if (spatial->IsDirty())
        {
            const uint32_t idx = entity.GetIndex();

            // Remove from index if previously inserted.
            if (mSpatialHandles[idx].IsValid())
                mIndex.Remove(mSpatialHandles[idx]);

            // Build AARect as circle AABB: bottomLeft = pos - radius, topRight = pos + radius.
            const Dia::Maths::Vector2D offset(spatial->radius, spatial->radius);
            const Dia::Geometry2D::AARect bounds(spatial->position - offset,
                                                 spatial->position + offset);

            mSpatialHandles[idx] = mIndex.Insert(entity, bounds);
            spatial->ClearDirty();
        }
    }

    // --- Pass 2: detach/destroy sweep ---
    // Remove entities from the index that no longer have a SpatialComponent
    // (component detached or entity destroyed since last frame).
    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain; ++i)
    {
        if (!mSpatialHandles[i].IsValid())
            continue;

        const Dia::Entity::Entity e = mDomain.GetAliveEntity(i);
        if (!e.IsValid() || !mDomain.HasComponent<SpatialComponent>(e))
        {
            mIndex.Remove(mSpatialHandles[i]);
            mSpatialHandles[i] = SpatialHandle::Invalid();
        }
    }
}

} // namespace Dia::EntitySpatial
