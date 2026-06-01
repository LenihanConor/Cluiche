#pragma once
// Template method implementations for Domain.
// Included at the bottom of Domain.h.
#include <DiaEntity/ComponentPool.h>

namespace Dia::Entity {

    template<class TComponent>
    void Domain::QueueAddComponent(Entity entity, const Json::Value& config) {
        DIA_ASSERT(IsAlive(entity), "QueueAddComponent: entity is not alive");
        if (mMutationQueue.IsFull()) {
            DIA_LOG_WARNING("DiaEntity", "Mutation queue full — dropping QueueAddComponent");
            return;
        }
        MutationOp op;
        op.kind            = MutationKind::AddComponent;
        op.entity          = entity;
        op.componentTypeId = TComponent::GetStaticTypeId();
        op.config          = config;
        mMutationQueue.Add(op);
    }

    template<class TComponent>
    void Domain::QueueRemoveComponent(Entity entity) {
        DIA_ASSERT(IsAlive(entity), "QueueRemoveComponent: entity is not alive");
        if (mMutationQueue.IsFull()) {
            DIA_LOG_WARNING("DiaEntity", "Mutation queue full — dropping QueueRemoveComponent");
            return;
        }
        MutationOp op;
        op.kind            = MutationKind::RemoveComponent;
        op.entity          = entity;
        op.componentTypeId = TComponent::GetStaticTypeId();
        mMutationQueue.Add(op);
    }

    template<class TComponent>
    TComponent* Domain::GetComponent(Entity entity) {
        if (!IsAlive(entity)) return nullptr;
        IComponentPool* pool = FindPool(TComponent::GetStaticTypeId());
        if (!pool) return nullptr;
        return static_cast<TComponent*>(pool->GetRaw(entity.GetIndex()));
    }

    template<class TComponent>
    const TComponent* Domain::GetComponent(Entity entity) const {
        if (!IsAlive(entity)) return nullptr;
        const IComponentPool* pool = FindPool(TComponent::GetStaticTypeId());
        if (!pool) return nullptr;
        return static_cast<const TComponent*>(pool->GetRaw(entity.GetIndex()));
    }

    template<class TComponent>
    bool Domain::HasComponent(Entity entity) const {
        if (!IsAlive(entity)) return false;
        const IComponentPool* pool = FindPool(TComponent::GetStaticTypeId());
        if (!pool) return false;
        return pool->HasSlot(entity.GetIndex());
    }

    template<class... TComponents>
    QueryView<TComponents...> Domain::Query() {
        static_assert(sizeof...(TComponents) > 0, "Domain::Query requires at least one component type");

        Dia::Core::CRC sig = ComputeQuerySignature<TComponents...>();

        // Search for an existing cache entry with this signature.
        for (uint32_t i = 0; i < mQueryCaches.Size(); ++i) {
            if (mQueryCaches[i].signatureCRC == sig) {
                // Found — if dirty, rebuild immediately.
                if (mQueryCaches[i].dirty) {
                    RebuildQueryCache(mQueryCaches[i]);
                    mQueryCaches[i].dirty = false;
                }
                QueryView<TComponents...> view;
                view.SetSource(&mQueryCaches[i].entities, this);
                return view;
            }
        }

        // No existing cache — create a new one if capacity allows.
        if (mQueryCaches.IsFull()) {
            DIA_ASSERT(false, "Domain::Query — exceeded kMaxQueryTypes (%u); cannot cache new query signature", kMaxQueryTypes);
            DIA_LOG_WARNING("DiaEntity", "Domain::Query: exceeded kMaxQueryTypes (%u) — performing uncached live scan", kMaxQueryTypes);

            // Release: perform an uncached live scan.
            // Uses a static local to avoid heap allocation.  This path is intentionally
            // slow to encourage reducing the query count.
            static Dia::Core::Containers::DynamicArrayC<Entity, kMaxEntitiesPerDomain> sTempEntities;
            sTempEntities.RemoveAll();

            // Use type CRCs via kTypeId (available at template instantiation time).
            Dia::Core::StringCRC typeCRCs[] = { TComponents::kTypeId... };
            const uint32_t count = static_cast<uint32_t>(sizeof...(TComponents));

            for (uint32_t entityIdx = 0; entityIdx < kMaxEntitiesPerDomain; ++entityIdx) {
                uint32_t gen = mEntityPool.GetLiveGeneration(entityIdx);
                if (gen == 0) continue;
                bool hasAll = true;
                for (uint32_t t = 0; t < count; ++t) {
                    const IComponentPool* pool = FindPool(typeCRCs[t]);
                    if (pool == nullptr || !pool->HasSlot(entityIdx)) { hasAll = false; break; }
                }
                if (hasAll && !sTempEntities.IsFull()) {
                    sTempEntities.Add(Entity(entityIdx, gen));
                }
            }

            QueryView<TComponents...> view;
            view.SetSource(&sTempEntities, this);
            return view;
        }

        // Build a new cache entry.
        QueryCache newCache;
        newCache.signatureCRC = sig;
        newCache.dirty        = false;

        // Store individual type CRCs so InvalidateCachesForType can check membership.
        Dia::Core::StringCRC typeCRCArr[] = { TComponents::kTypeId... };
        for (uint32_t i = 0; i < static_cast<uint32_t>(sizeof...(TComponents)); ++i) {
            newCache.typeCRCs.Add(typeCRCArr[i]);
        }

        mQueryCaches.Add(newCache);
        QueryCache& cache = mQueryCaches[mQueryCaches.Size() - 1];
        RebuildQueryCache(cache);

        QueryView<TComponents...> view;
        view.SetSource(&cache.entities, this);
        return view;
    }

    // --- Service registry template implementations ---

    // Shared type key — identical signature regardless of which method calls it.
    template<class T>
    uint32_t Domain::ServiceTypeKey() {
        static const uint32_t key = Dia::Core::StringCRC(__FUNCSIG__).Value();
        return key;
    }

    template<class T>
    bool Domain::RegisterService(T* service) {
        const uint32_t key = ServiceTypeKey<T>();
        for (uint32_t i = 0; i < mServices.Size(); ++i) {
            if (mServices[i].typeKey == key) return false;
        }
        if (mServices.IsFull()) {
            DIA_LOG_WARNING("DiaEntity", "Domain::RegisterService — service table full (capacity %u)", kMaxServices);
            return false;
        }
        ServiceEntry entry;
        entry.typeKey = key;
        entry.ptr     = static_cast<void*>(service);
        mServices.Add(entry);
        return true;
    }

    template<class T>
    T* Domain::GetService() const {
        const uint32_t key = ServiceTypeKey<T>();
        for (uint32_t i = 0; i < mServices.Size(); ++i) {
            if (mServices[i].typeKey == key)
                return static_cast<T*>(mServices[i].ptr);
        }
        return nullptr;
    }

    template<class T>
    void Domain::UnregisterService() {
        const uint32_t key = ServiceTypeKey<T>();
        for (uint32_t i = 0; i < mServices.Size(); ++i) {
            if (mServices[i].typeKey == key) {
                mServices.RemoveAt(i);
                return;
            }
        }
    }

} // namespace Dia::Entity

// EntityRef<T>::Resolve() needs Domain fully defined — pull in after the closing brace.
#include <DiaEntity/EntityRef.inl>

// QueryView implementation (needs Domain fully defined for GetComponent<T> calls).
#include <DiaEntity/QueryView.inl>
