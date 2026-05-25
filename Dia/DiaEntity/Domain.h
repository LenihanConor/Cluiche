#pragma once
#include <cstdint>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Handle.h>
#include <DiaCore/Containers/HandlePool.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaMailbox/Mailbox.h>
#include <DiaEntity/Entity.h>
#include <DiaEntity/IComponent.h>
#include <DiaEntity/IComponentPool.h>
#include <DiaEntity/MutationOp.h>
#include <DiaEntity/QueryCache.h>
#include <DiaEntity/QueryView.h>

namespace Dia::Entity {

    class Domain {
    public:
        Domain();
        ~Domain();

        Domain(const Domain&)            = delete;
        Domain& operator=(const Domain&) = delete;
        Domain(Domain&&)                 = delete;
        Domain& operator=(Domain&&)      = delete;

        // --- Entity lifecycle ---

        // Creates a fresh entity (slot allocated immediately, components queued).
        // Returns Entity::Invalid() if the pool is full.
        Entity CreateEntity();
        Entity CreateEntity(const char* debugName);

        // Queue a destroy. Applied at EndOfFrame.
        void QueueDestroy(Entity entity);

        bool        IsAlive(Entity entity) const;
        const char* GetDebugName(Entity entity) const; // nullptr in non-debug builds

        // --- Component queuing ---

        // Queue a component add. Applied at EndOfFrame.
        // Requires a registered pool for TComponent (wired at reflection task T7).
        template<class TComponent>
        void QueueAddComponent(Entity entity, const Json::Value& config);

        // Queue a component add by type ID (non-template version for blueprint loader).
        // Applied at EndOfFrame.
        void QueueAddComponentByTypeId(Entity entity, Dia::Core::StringCRC typeId, const Json::Value& config);

        // Queue a component remove. Applied at EndOfFrame.
        template<class TComponent>
        void QueueRemoveComponent(Entity entity);

        // --- Synchronous read ---

        // Returns nullptr if entity is not alive or has no such component.
        // Pointer stable until next EndOfFrame structural change on this entity.
        template<class TComponent>
        TComponent* GetComponent(Entity entity);

        template<class TComponent>
        const TComponent* GetComponent(Entity entity) const;

        template<class TComponent>
        bool HasComponent(Entity entity) const;

        // --- Query ---

        // Returns a QueryView of all entities carrying every listed component type.
        // Cache keyed by combined (sorted-XOR) StringCRC of all type CRCs.
        // Cache is rebuilt at EndOfFrame when any relevant type was mutated that frame.
        // kMaxQueryTypes = 64: exceeding → DIA_ASSERT in debug, DIA_LOG_WARNING + live scan in release.
        template<class... TComponents>
        QueryView<TComponents...> Query();

        // --- Per-frame entry points ---

        // Walks DoUpdate-opted components (wired at update-loop task T14; stub here).
        void Update(float dt);

        // Applies all queued structural mutations. Must be called once per frame.
        void EndOfFrame();

        // --- Mailbox access ---

        Dia::Mailbox::Mailbox&       GetMailbox()       { return mMailbox; }
        const Dia::Mailbox::Mailbox& GetMailbox() const { return mMailbox; }

        // --- Internal pool registration (called by reflection feature T7) ---

        // Register a pre-constructed component pool. Returns false if type already registered
        // or the pool table is full. Takes ownership.
        bool RegisterPool(IComponentPool* pool);

    private:
        // Entity slot tag type — empty struct used as the HandlePool element.
        struct EntitySlotData {};
        Dia::Core::HandlePool<EntitySlotData, kMaxEntitiesPerDomain> mEntityPool;

#ifdef DEBUG
        char mDebugNames[kMaxEntitiesPerDomain][kMaxDebugNameLength];
        bool mDebugNameSet[kMaxEntitiesPerDomain];
#endif

        // Per-type component pools (type-erased, owned by Domain).
        Dia::Core::Containers::DynamicArrayC<IComponentPool*, kMaxComponentTypesPerDomain> mComponentPools;

        // End-of-frame mutation queue.
        Dia::Core::Containers::DynamicArrayC<MutationOp, kMaxMutationsPerFrame> mMutationQueue;

        // Per-realm mailbox.
        Dia::Mailbox::Mailbox mMailbox;

        // Query cache table — one entry per unique query signature.
        Dia::Core::Containers::DynamicArrayC<QueryCache, kMaxQueryTypes> mQueryCaches;

        // Internal helpers.
        IComponentPool*       FindPool(Dia::Core::StringCRC typeId);
        const IComponentPool* FindPool(Dia::Core::StringCRC typeId) const;

        void ApplyMutations();
        void ApplyAddComponent(const MutationOp& op);
        void ApplyRemoveComponent(const MutationOp& op);
        void ApplyDestroyEntity(const MutationOp& op);

        // Mark all caches dirty whose signature includes the given component type.
        void InvalidateCachesForType(Dia::Core::StringCRC typeId);

        // Rebuild a single dirty cache by walking entity indices.
        void RebuildQueryCache(QueryCache& cache);
    };

} // namespace Dia::Entity

#include <DiaEntity/Domain.inl>
