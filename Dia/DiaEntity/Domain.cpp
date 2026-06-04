#include <diaentitytemplate/Domain.h>
#include <diaentitytemplate/ComponentPool.h>
#include <diaentitytemplate/ComponentRegistry.h>
#include <diaentitytemplate/Messages/EntityDestroyedMessage.h>
#include <diaentitytemplate/EntityAddress.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <cstring>
#include <cstdlib> // strcmp via cstring

namespace Dia::Entity {

    Domain::Domain()
        : mMailbox()
        , mEntityRouter(*this)
    {
        mMailbox.RegisterRouter(&mEntityRouter);
        // Register EntityDestroyedMessage so Domain can broadcast it in ApplyDestroyEntity.
        mMailbox.RegisterType<EntityDestroyedMessage, kMaxEntitiesPerDomain>();
#ifdef DEBUG
        for (uint32_t i = 0; i < kMaxEntitiesPerDomain; ++i) {
            mDebugNames[i][0] = '\0';
            mDebugNameSet[i]  = false;
        }
#endif
        // Register metrics.
        auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
        mMetricEntityCount    = reg.RegisterGauge(Dia::Core::StringCRC("dia.entity.count"));
        mMetricComponentCount = reg.RegisterGauge(Dia::Core::StringCRC("dia.entity.components"));
        mMetricMutations      = reg.RegisterCounter(Dia::Core::StringCRC("dia.entity.mutations"));
        mMetricQueryRebuilds  = reg.RegisterCounter(Dia::Core::StringCRC("dia.entity.query_rebuilds"));

        // Register health reporter.
        Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
    }

    Domain::~Domain() {
        // Unregister health reporter.
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);

        // Release all registered component pools.
        for (uint32_t i = 0; i < mComponentPools.Size(); ++i) {
            delete mComponentPools[i];
        }
    }

    Entity Domain::CreateEntity() {
        DIA_ASSERT(!mEntityPool.IsFull(), "Entity pool full (capacity %u)", kMaxEntitiesPerDomain);
        if (mEntityPool.IsFull()) {
            return Entity::Invalid();
        }
        Dia::Core::Handle<EntitySlotData> slot = mEntityPool.Allocate();
        return Entity(slot.GetIndex(), slot.GetGeneration());
    }

    Entity Domain::CreateEntity(const char* debugName) {
        Entity e = CreateEntity();
#ifdef DEBUG
        if (e.IsValid() && debugName != nullptr) {
            strncpy_s(mDebugNames[e.GetIndex()], kMaxDebugNameLength, debugName, _TRUNCATE);
            mDebugNameSet[e.GetIndex()] = true;
        }
#else
        (void)debugName;
#endif
        return e;
    }

    void Domain::QueueAddComponentByTypeId(Entity entity, Dia::Core::StringCRC typeId, const Json::Value& config) {
        DIA_ASSERT(IsAlive(entity), "QueueAddComponentByTypeId: entity is not alive");
        if (mMutationQueue.IsFull()) {
            DIA_LOG_WARNING("diaentitytemplate", "Mutation queue full — dropping QueueAddComponentByTypeId");
            return;
        }
        MutationOp op;
        op.kind            = MutationKind::AddComponent;
        op.entity          = entity;
        op.componentTypeId = typeId;
        op.config          = config;
        mMutationQueue.Add(op);
    }

    void Domain::QueueDestroy(Entity entity) {
        DIA_ASSERT(IsAlive(entity), "QueueDestroy: entity is not alive");
        if (mMutationQueue.IsFull()) {
            DIA_LOG_WARNING("diaentitytemplate", "Mutation queue full — dropping QueueDestroy");
            return;
        }
        MutationOp op;
        op.kind   = MutationKind::DestroyEntity;
        op.entity = entity;
        mMutationQueue.Add(op);
    }

    bool Domain::IsAlive(Entity entity) const {
        if (!entity.IsValid()) return false;
        Dia::Core::Handle<EntitySlotData> slot(entity.GetIndex(), entity.GetGeneration());
        return mEntityPool.IsValid(slot);
    }

    const char* Domain::GetDebugName(Entity entity) const {
#ifdef DEBUG
        if (entity.IsValid() && mDebugNameSet[entity.GetIndex()]) {
            return mDebugNames[entity.GetIndex()];
        }
#else
        (void)entity;
#endif
        return nullptr;
    }

    bool Domain::RegisterPool(IComponentPool* pool) {
        DIA_ASSERT(pool != nullptr, "Domain::RegisterPool — pool must not be null");
        if (pool == nullptr) return false;
        if (FindPool(pool->GetTypeId()) != nullptr) {
            DIA_LOG_WARNING("diaentitytemplate", "Domain::RegisterPool — pool for type already registered");
            return false;
        }
        if (mComponentPools.IsFull()) {
            DIA_LOG_WARNING("diaentitytemplate", "Domain::RegisterPool — component pool table full (capacity %u)", kMaxComponentTypesPerDomain);
            return false;
        }
        mComponentPools.Add(pool);
        return true;
    }

    Entity Domain::GetAliveEntity(uint32_t index) const {
        if (index >= kMaxEntitiesPerDomain) {
            return Entity::Invalid();
        }
        uint32_t gen = mEntityPool.GetLiveGeneration(index);
        if (gen == 0) {
            return Entity::Invalid();
        }
        return Entity(index, gen);
    }

    bool Domain::HasComponentByTypeId(Entity entity, Dia::Core::StringCRC typeId) const {
        if (!IsAlive(entity)) return false;
        const IComponentPool* pool = FindPool(typeId);
        if (pool == nullptr) return false;
        return pool->HasSlot(entity.GetIndex());
    }

    void Domain::Update(float dt) {
        DIA_TRACE_ZONE("Domain::Update", Dia::Observation::Trace::Category::kdiaentitytemplate);
        DIA_PROFILE_SCOPE("Domain::Update", Dia::Observation::Profile::Category::kdiaentitytemplate);
        // Walk all component pools in registration order.
        // Call DoUpdate only on components whose type has kFlagOverridesDoUpdate set.
        for (uint32_t poolIdx = 0; poolIdx < mComponentPools.Size(); ++poolIdx) {
            IComponentPool* pool = mComponentPools[poolIdx];

            // Check if this pool's component type is registered as updatable.
            const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(pool->GetTypeId());
            if (desc == nullptr || !(desc->flags & kFlagOverridesDoUpdate)) {
                continue;
            }

            // Walk all entity indices and update those that have this component.
            for (uint32_t entityIdx = 0; entityIdx < kMaxEntitiesPerDomain; ++entityIdx) {
                if (!pool->HasSlot(entityIdx)) {
                    continue;
                }

                // Verify the entity is alive and reconstruct the handle.
                uint32_t gen = mEntityPool.GetLiveGeneration(entityIdx);
                if (gen == 0) {
                    // Slot in pool exists but entity slot is not live (should not happen in normal flow).
                    continue;
                }
                Entity e(entityIdx, gen);

                // Get the component and call its DoUpdate.
                IComponent* comp = pool->GetRaw(entityIdx);
                if (comp) {
                    comp->DoUpdate(*this, e, dt);
                }
            }
        }
    }

    void Domain::EndOfFrame() {
        DIA_TRACE_ZONE("Domain::EndOfFrame", Dia::Observation::Trace::Category::kdiaentitytemplate);
        DIA_PROFILE_SCOPE("Domain::EndOfFrame", Dia::Observation::Profile::Category::kdiaentitytemplate);

        ApplyMutations();

        // Rebuild all caches marked dirty during ApplyMutations.
        for (uint32_t i = 0; i < mQueryCaches.Size(); ++i) {
            if (mQueryCaches[i].dirty) {
                RebuildQueryCache(mQueryCaches[i]);
                mQueryCaches[i].dirty = false;
            }
        }

        // Update entity count and component count gauges.
        if (mMetricEntityCount) {
            uint32_t count = 0;
            for (uint32_t i = 0; i < kMaxEntitiesPerDomain; ++i) {
                if (mEntityPool.GetLiveGeneration(i) != 0) ++count;
            }
            mMetricEntityCount->Set(static_cast<double>(count));
        }
        if (mMetricComponentCount) {
            uint32_t total = 0;
            for (uint32_t i = 0; i < mComponentPools.Size(); ++i) {
                total += mComponentPools[i]->Size();
            }
            mMetricComponentCount->Set(static_cast<double>(total));
        }
    }

    IComponentPool* Domain::FindPool(Dia::Core::StringCRC typeId) {
        for (uint32_t i = 0; i < mComponentPools.Size(); ++i) {
            if (mComponentPools[i]->GetTypeId() == typeId) {
                return mComponentPools[i];
            }
        }
        return nullptr;
    }

    const IComponentPool* Domain::FindPool(Dia::Core::StringCRC typeId) const {
        for (uint32_t i = 0; i < mComponentPools.Size(); ++i) {
            if (mComponentPools[i]->GetTypeId() == typeId) {
                return mComponentPools[i];
            }
        }
        return nullptr;
    }

    void Domain::ApplyMutations() {
        DIA_TRACE_ZONE("Domain::ApplyMutations", Dia::Observation::Trace::Category::kdiaentitytemplate);

        // Process in deterministic order: AddComponent → RemoveComponent → DestroyEntity.
        // This ensures components are visible before any remove in the same frame,
        // and entities are alive for component removal before the slot is freed.

        uint32_t totalOps = 0;

        for (uint32_t i = 0; i < mMutationQueue.Size(); ++i) {
            if (mMutationQueue[i].kind == MutationKind::AddComponent) {
                ApplyAddComponent(mMutationQueue[i]);
                InvalidateCachesForType(mMutationQueue[i].componentTypeId);
                ++totalOps;
            }
        }
        for (uint32_t i = 0; i < mMutationQueue.Size(); ++i) {
            if (mMutationQueue[i].kind == MutationKind::RemoveComponent) {
                ApplyRemoveComponent(mMutationQueue[i]);
                InvalidateCachesForType(mMutationQueue[i].componentTypeId);
                ++totalOps;
            }
        }
        for (uint32_t i = 0; i < mMutationQueue.Size(); ++i) {
            if (mMutationQueue[i].kind == MutationKind::DestroyEntity) {
                // A destroy removes all components — invalidate all caches.
                for (uint32_t c = 0; c < mQueryCaches.Size(); ++c) {
                    mQueryCaches[c].dirty = true;
                }
                ApplyDestroyEntity(mMutationQueue[i]);
                ++totalOps;
            }
        }

        if (mMetricMutations && totalOps > 0) {
            mMetricMutations->Inc(totalOps);
        }

        // Reset Json::Value config fields before RemoveAll to avoid double-destruction.
        // DynamicArrayC::RemoveAll calls ~T() explicitly, and the member array destructor
        // also calls ~T() — safe only if the element is trivially destructible or already reset.
        for (uint32_t i = 0; i < mMutationQueue.Size(); ++i) {
            mMutationQueue[i].config = Json::Value();
        }
        mMutationQueue.RemoveAll();
    }

    void Domain::InvalidateCachesForType(Dia::Core::StringCRC typeId) {
        for (uint32_t i = 0; i < mQueryCaches.Size(); ++i) {
            QueryCache& cache = mQueryCaches[i];
            // Walk the type CRC list stored in the cache to check membership.
            for (uint32_t t = 0; t < cache.typeCRCs.Size(); ++t) {
                if (cache.typeCRCs[t] == typeId) {
                    cache.dirty = true;
                    break;
                }
            }
        }
    }

    void Domain::RebuildQueryCache(QueryCache& cache) {
        DIA_TRACE_ZONE("Domain::RebuildQueryCache", Dia::Observation::Trace::Category::kdiaentitytemplate);
        DIA_PROFILE_SCOPE("Domain::RebuildQueryCache", Dia::Observation::Profile::Category::kdiaentitytemplate);
        if (mMetricQueryRebuilds) mMetricQueryRebuilds->Inc();

        cache.entities.RemoveAll();

        // Walk every entity index and check whether it has all required component types.
        for (uint32_t entityIdx = 0; entityIdx < kMaxEntitiesPerDomain; ++entityIdx) {
            uint32_t gen = mEntityPool.GetLiveGeneration(entityIdx);
            if (gen == 0) {
                continue; // entity slot not live
            }

            // Entity must have all component types in this cache's signature.
            bool hasAll = true;
            for (uint32_t t = 0; t < cache.typeCRCs.Size(); ++t) {
                const IComponentPool* pool = FindPool(cache.typeCRCs[t]);
                if (pool == nullptr || !pool->HasSlot(entityIdx)) {
                    hasAll = false;
                    break;
                }
            }

            if (hasAll) {
                Entity e(entityIdx, gen);
                if (!cache.entities.IsFull()) {
                    cache.entities.Add(e);
                }
            }
        }
    }

    void Domain::ApplyAddComponent(const MutationOp& op) {
        if (!IsAlive(op.entity)) return;

        IComponentPool* pool = FindPool(op.componentTypeId);
        if (!pool) {
            DIA_LOG_WARNING("diaentitytemplate", "ApplyAddComponent: no pool registered for component type — call Domain::RegisterPool first");
            return;
        }
        if (pool->HasSlot(op.entity.GetIndex())) {
            DIA_LOG_WARNING("diaentitytemplate", "ApplyAddComponent: entity already has this component type");
            return;
        }

        // Look up the reflection descriptor to validate REQUIRES and drive JSON load.
        // If no descriptor is registered the component still attaches (fields keep defaults).
        const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(op.componentTypeId);

        // Validate REQUIRES — all required components must already be present.
        if (desc != nullptr) {
            for (uint16_t i = 0; i < desc->requiresCount; ++i) {
                const IComponentPool* reqPool = FindPool(desc->requires_[i]);
                const bool hasReq = (reqPool != nullptr) && reqPool->HasSlot(op.entity.GetIndex());
                DIA_ASSERT(hasReq,
                    "ApplyAddComponent: required component (CRC %u) not present on entity",
                    desc->requires_[i].Value());
                if (!hasReq) {
                    mHealth.IncrementErrors();
                    return;
                }
            }
        }

        // Allocate and default-construct the component inside the typed pool.
        // HandlePool::Allocate() calls T's default constructor; AllocateRaw wraps it.
        IComponent* comp = pool->AllocateRaw(op.entity.GetIndex());
        if (!comp) {
            DIA_LOG_WARNING("diaentitytemplate", "ApplyAddComponent: component pool full, cannot allocate");
            mHealth.IncrementWarnings();
            return;
        }

        // Load fields from JSON config (overwrites defaults set by the default constructor).
        if (!op.config.isNull() && desc != nullptr && desc->loadFromJson != nullptr) {
            desc->loadFromJson(comp, op.config);
        }

        // Skip lifecycle hooks for readonly components.
        if (desc == nullptr || !(desc->flags & kFlagReadOnly)) {
            comp->OnAttach(*this, op.entity);
        }

        // Validate single-writer rule: no two updatable components on this entity
        // may declare writes to the same target.
        if (desc != nullptr && desc->writesToCount > 0) {
            for (uint16_t w = 0; w < desc->writesToCount; ++w) {
                Dia::Core::StringCRC target = desc->writesTo[w];
                for (uint32_t p = 0; p < mComponentPools.Size(); ++p) {
                    IComponentPool* otherPool = mComponentPools[p];
                    if (otherPool->GetTypeId() == op.componentTypeId) continue;
                    if (!otherPool->HasSlot(op.entity.GetIndex())) continue;
                    const ComponentTypeDesc* otherDesc = ComponentRegistry::Get().Find(otherPool->GetTypeId());
                    if (otherDesc == nullptr) continue;
                    for (uint16_t ow = 0; ow < otherDesc->writesToCount; ++ow) {
                        DIA_ASSERT(otherDesc->writesTo[ow] != target,
                            "Single-writer violation: entity %u has two components writing to the same target (CRC %u)",
                            op.entity.GetIndex(), target.Value());
                    }
                }
            }
        }

        DIA_LOG_DEBUG("diaentitytemplate", "entity %u: attached component (CRC %u)", op.entity.GetIndex(), op.componentTypeId.Value());
    }

    void Domain::ApplyRemoveComponent(const MutationOp& op) {
        IComponentPool* pool = FindPool(op.componentTypeId);
        if (!pool) return;
        if (!pool->HasSlot(op.entity.GetIndex())) return;

        const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(op.componentTypeId);
        IComponent* comp = pool->GetRaw(op.entity.GetIndex());
        if (comp && (desc == nullptr || !(desc->flags & kFlagReadOnly))) {
            comp->OnDetach(*this, op.entity);
        }
        pool->Destroy(op.entity.GetIndex());
        DIA_LOG_DEBUG("diaentitytemplate", "entity %u: detached component (CRC %u)", op.entity.GetIndex(), op.componentTypeId.Value());
    }

    void Domain::ApplyDestroyEntity(const MutationOp& op) {
        if (!IsAlive(op.entity)) return;
        DIA_LOG_DEBUG("diaentitytemplate", "entity %u destroyed", op.entity.GetIndex());

        // Broadcast EntityDestroyedMessage before components are detached so subscribers
        // can still inspect the entity's state during their drain callbacks.
        {
            EntityDestroyedMessage msg;
            msg.destroyed = op.entity;
            mMailbox.Send(MakeAllAddress(), msg);
        }

        // Detach and destroy all components on this entity.
        for (uint32_t i = 0; i < mComponentPools.Size(); ++i) {
            IComponentPool* pool = mComponentPools[i];
            if (pool->HasSlot(op.entity.GetIndex())) {
                IComponent* comp = pool->GetRaw(op.entity.GetIndex());
                if (comp) {
                    const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(pool->GetTypeId());
                    if (desc == nullptr || !(desc->flags & kFlagReadOnly)) {
                        comp->OnDetach(*this, op.entity);
                    }
                }
                pool->Destroy(op.entity.GetIndex());
            }
        }

#ifdef DEBUG
        mDebugNameSet[op.entity.GetIndex()] = false;
        mDebugNames[op.entity.GetIndex()][0] = '\0';
#endif

        // Return the entity slot to the pool.
        Dia::Core::Handle<EntitySlotData> slot(op.entity.GetIndex(), op.entity.GetGeneration());
        mEntityPool.Free(slot);
    }

    // -------------------------------------------------------------------------
    // IEntityInspectable — F9: Editor Inspection
    // -------------------------------------------------------------------------

    uint32_t Domain::GetEntityCount() const {
        uint32_t count = 0;
        for (uint32_t i = 0; i < kMaxEntitiesPerDomain; ++i) {
            if (mEntityPool.GetLiveGeneration(i) != 0) {
                ++count;
            }
        }
        return count;
    }

    void Domain::GetAllEntities(
        Dia::Core::Containers::DynamicArrayC<Entity, kMaxEntitiesPerDomain>& out) const
    {
        out.RemoveAll();
        for (uint32_t i = 0; i < kMaxEntitiesPerDomain; ++i) {
            uint32_t gen = mEntityPool.GetLiveGeneration(i);
            if (gen != 0 && !out.IsFull()) {
                out.Add(Entity(i, gen));
            }
        }
    }

    void Domain::GetComponentTypeIds(
        Entity entity,
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& out) const
    {
        out.RemoveAll();
        if (!IsAlive(entity)) return;
        for (uint32_t i = 0; i < mComponentPools.Size(); ++i) {
            if (mComponentPools[i]->HasSlot(entity.GetIndex()) && !out.IsFull()) {
                out.Add(mComponentPools[i]->GetTypeId());
            }
        }
    }

    bool Domain::ReadField(
        Entity entity,
        Dia::Core::StringCRC componentTypeId,
        const char* fieldName,
        Json::Value& out) const
    {
        if (!IsAlive(entity)) {
            DIA_LOG_WARNING("diaentitytemplate", "ReadField: entity not alive");
            return false;
        }
        const IComponentPool* pool = FindPool(componentTypeId);
        if (pool == nullptr || !pool->HasSlot(entity.GetIndex())) {
            DIA_LOG_WARNING("diaentitytemplate", "ReadField: component not found on entity");
            return false;
        }
        const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(componentTypeId);
        if (desc == nullptr || desc->saveToJson == nullptr) {
            DIA_LOG_WARNING("diaentitytemplate", "ReadField: no reflection descriptor for component");
            return false;
        }
        const IComponent* comp = pool->GetRaw(entity.GetIndex());
        Json::Value fullJson;
        desc->saveToJson(comp, fullJson);
        if (!fullJson.isMember(fieldName)) {
            DIA_LOG_WARNING("diaentitytemplate", "ReadField: field not found in component JSON");
            return false;
        }
        out = fullJson[fieldName];
        return true;
    }

    uint32_t Domain::GetQueryCount() const {
        return mQueryCaches.Size();
    }

    void Domain::GetQuerySignature(uint32_t queryIndex,
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& out) const {
        out.RemoveAll();
        if (queryIndex >= mQueryCaches.Size()) return;
        const QueryCache& qc = mQueryCaches[queryIndex];
        for (uint32_t i = 0; i < qc.typeCRCs.Size(); ++i)
            out.Add(qc.typeCRCs[i]);
    }

    uint32_t Domain::GetQueryEntityCount(uint32_t queryIndex) const {
        if (queryIndex >= mQueryCaches.Size()) return 0;
        return mQueryCaches[queryIndex].entities.Size();
    }

    bool Domain::WriteField(
        Entity entity,
        Dia::Core::StringCRC componentTypeId,
        const char* fieldName,
        const Json::Value& value)
    {
        if (!IsAlive(entity)) {
            DIA_LOG_WARNING("diaentitytemplate", "WriteField: entity not alive");
            return false;
        }
        IComponentPool* pool = FindPool(componentTypeId);
        if (pool == nullptr || !pool->HasSlot(entity.GetIndex())) {
            DIA_LOG_WARNING("diaentitytemplate", "WriteField: component not found on entity");
            return false;
        }
        const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(componentTypeId);
        if (desc == nullptr || desc->saveToJson == nullptr || desc->loadFromJson == nullptr) {
            DIA_LOG_WARNING("diaentitytemplate", "WriteField: no reflection descriptor for component");
            return false;
        }

        // Linear scan to find FieldDesc by name.
        const FieldDesc* fieldDesc = nullptr;
        for (uint16_t i = 0; i < desc->fieldCount; ++i) {
            if (strcmp(desc->fields[i].name, fieldName) == 0) {
                fieldDesc = &desc->fields[i];
                break;
            }
        }
        if (fieldDesc == nullptr) {
            DIA_LOG_WARNING("diaentitytemplate", "WriteField: field not found in descriptor");
            return false;
        }

        // Type check: compare FieldDesc::kind against value type.
        bool typeOk = false;
        if (fieldDesc->kind == FieldKind::Primitive) {
            typeOk = value.isNumeric() || value.isBool();
        } else if (fieldDesc->kind == FieldKind::StringId) {
            typeOk = value.isString() || value.isInt() || value.isUInt();
        } else {
            // Nested, Math, AssetHandle, EntityRef, Container: accept any value.
            typeOk = true;
        }
        if (!typeOk) {
            DIA_LOG_WARNING("diaentitytemplate", "WriteField: type mismatch for field");
            return false;
        }

        // Read current state, update one field, write back.
        // This avoids resetting other fields that are not present in a partial JSON object.
        IComponent* comp = pool->GetRaw(entity.GetIndex());
        Json::Value fullJson;
        desc->saveToJson(comp, fullJson);
        fullJson[fieldName] = value;
        desc->loadFromJson(comp, fullJson);
        return true;
    }

} // namespace Dia::Entity
