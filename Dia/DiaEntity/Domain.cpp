#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntity/ComponentRegistry.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <cstring>

namespace Dia::Entity {

    Domain::Domain()
        : mMailbox()
    {
#ifdef DEBUG
        for (uint32_t i = 0; i < kMaxEntitiesPerDomain; ++i) {
            mDebugNames[i][0] = '\0';
            mDebugNameSet[i]  = false;
        }
#endif
    }

    Domain::~Domain() {
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

    void Domain::QueueDestroy(Entity entity) {
        DIA_ASSERT(IsAlive(entity), "QueueDestroy: entity is not alive");
        if (mMutationQueue.IsFull()) {
            DIA_LOG_WARNING("DiaEntity", "Mutation queue full — dropping QueueDestroy");
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
            DIA_LOG_WARNING("DiaEntity", "Domain::RegisterPool — pool for type already registered");
            return false;
        }
        if (mComponentPools.IsFull()) {
            DIA_LOG_WARNING("DiaEntity", "Domain::RegisterPool — component pool table full (capacity %u)", kMaxComponentTypesPerDomain);
            return false;
        }
        mComponentPools.Add(pool);
        return true;
    }

    void Domain::Update(float dt) {
        // Update-loop feature (T14) walks DoUpdate-opted components here.
        // Foundation provides the stub.
        (void)dt;
    }

    void Domain::EndOfFrame() {
        ApplyMutations();
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
        // Process in deterministic order: AddComponent → RemoveComponent → DestroyEntity.
        // This ensures components are visible before any remove in the same frame,
        // and entities are alive for component removal before the slot is freed.

        for (uint32_t i = 0; i < mMutationQueue.Size(); ++i) {
            if (mMutationQueue[i].kind == MutationKind::AddComponent) {
                ApplyAddComponent(mMutationQueue[i]);
            }
        }
        for (uint32_t i = 0; i < mMutationQueue.Size(); ++i) {
            if (mMutationQueue[i].kind == MutationKind::RemoveComponent) {
                ApplyRemoveComponent(mMutationQueue[i]);
            }
        }
        for (uint32_t i = 0; i < mMutationQueue.Size(); ++i) {
            if (mMutationQueue[i].kind == MutationKind::DestroyEntity) {
                ApplyDestroyEntity(mMutationQueue[i]);
            }
        }

        // Reset Json::Value config fields before RemoveAll to avoid double-destruction.
        // DynamicArrayC::RemoveAll calls ~T() explicitly, and the member array destructor
        // also calls ~T() — safe only if the element is trivially destructible or already reset.
        for (uint32_t i = 0; i < mMutationQueue.Size(); ++i) {
            mMutationQueue[i].config = Json::Value();
        }
        mMutationQueue.RemoveAll();
    }

    void Domain::ApplyAddComponent(const MutationOp& op) {
        if (!IsAlive(op.entity)) return;

        IComponentPool* pool = FindPool(op.componentTypeId);
        if (!pool) {
            DIA_LOG_WARNING("DiaEntity", "ApplyAddComponent: no pool registered for component type — call Domain::RegisterPool first");
            return;
        }
        if (pool->HasSlot(op.entity.GetIndex())) {
            DIA_LOG_WARNING("DiaEntity", "ApplyAddComponent: entity already has this component type");
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
                if (!hasReq) return;
            }
        }

        // Allocate and default-construct the component inside the typed pool.
        // HandlePool::Allocate() calls T's default constructor; AllocateRaw wraps it.
        IComponent* comp = pool->AllocateRaw(op.entity.GetIndex());
        if (!comp) {
            DIA_LOG_WARNING("DiaEntity", "ApplyAddComponent: component pool full, cannot allocate");
            return;
        }

        // Load fields from JSON config (overwrites defaults set by the default constructor).
        if (!op.config.isNull() && desc != nullptr && desc->loadFromJson != nullptr) {
            desc->loadFromJson(comp, op.config);
        }

        // Notify the component that it has been attached.
        comp->OnAttach(*this, op.entity);
    }

    void Domain::ApplyRemoveComponent(const MutationOp& op) {
        IComponentPool* pool = FindPool(op.componentTypeId);
        if (!pool) return;
        if (!pool->HasSlot(op.entity.GetIndex())) return;

        IComponent* comp = pool->GetRaw(op.entity.GetIndex());
        if (comp) {
            comp->OnDetach(*this, op.entity);
        }
        pool->Destroy(op.entity.GetIndex());
    }

    void Domain::ApplyDestroyEntity(const MutationOp& op) {
        if (!IsAlive(op.entity)) return;

        // Detach and destroy all components on this entity.
        for (uint32_t i = 0; i < mComponentPools.Size(); ++i) {
            IComponentPool* pool = mComponentPools[i];
            if (pool->HasSlot(op.entity.GetIndex())) {
                IComponent* comp = pool->GetRaw(op.entity.GetIndex());
                if (comp) {
                    comp->OnDetach(*this, op.entity);
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

} // namespace Dia::Entity
