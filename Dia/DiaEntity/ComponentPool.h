#pragma once
#include <DiaCore/Containers/HandlePool.h>
#include <DiaCore/Core/Assert.h>
#include <diaentitytemplate/IComponentPool.h>
#include <diaentitytemplate/Entity.h>

namespace Dia::Entity {

    // Typed pool for a single component type TComponent.
    // One pool per component type, owned by Domain.
    template<class TComponent, uint32_t kCapacity = kMaxEntitiesPerDomain>
    class ComponentPool final : public IComponentPool {
    public:
        // Internal tag for the pool's own Handle type, distinct from Entity handles.
        class SlotTag {};
        using SlotHandle = Dia::Core::Handle<TComponent>;

        explicit ComponentPool(Dia::Core::StringCRC typeId)
            : mTypeId(typeId)
        {
            for (uint32_t i = 0; i < kMaxEntitiesPerDomain; ++i) {
                mEntityToSlot[i] = SlotHandle::Invalid();
            }
        }

        Dia::Core::StringCRC GetTypeId() const override { return mTypeId; }

        // --- IComponentPool interface ---

        IComponent* GetRaw(uint32_t entityIndex) override {
            SlotHandle h = mEntityToSlot[entityIndex];
            if (!h.IsValid()) return nullptr;
            return mPool.Get(h);
        }

        const IComponent* GetRaw(uint32_t entityIndex) const override {
            SlotHandle h = mEntityToSlot[entityIndex];
            if (!h.IsValid()) return nullptr;
            return mPool.Get(h);
        }

        void Destroy(uint32_t entityIndex) override {
            DIA_ASSERT(mEntityToSlot[entityIndex].IsValid(),
                "ComponentPool::Destroy — entity %u has no slot in this pool", entityIndex);
            mPool.Free(mEntityToSlot[entityIndex]);
            mEntityToSlot[entityIndex] = SlotHandle::Invalid();
        }

        bool HasSlot(uint32_t entityIndex) const override {
            return mEntityToSlot[entityIndex].IsValid();
        }

        uint32_t GetSlotForEntity(uint32_t entityIndex) const override {
            SlotHandle h = mEntityToSlot[entityIndex];
            if (!h.IsValid()) return kInvalidPoolSlot;
            return h.GetIndex();
        }

        void SetSlotForEntity(uint32_t entityIndex, uint32_t slotIndex) override {
            // Used only when a pre-existing slot needs to be wired (rare).
            // Normal path is Allocate().
            mEntityToSlot[entityIndex] = SlotHandle(slotIndex, 1u);
        }

        void ClearSlotForEntity(uint32_t entityIndex) override {
            mEntityToSlot[entityIndex] = SlotHandle::Invalid();
        }

        // --- IComponentPool interface (allocation) ---

        // Allocate and default-construct a component for an entity.
        // Returns the new IComponent* or nullptr on failure.
        // HandlePool::Allocate() calls T's default constructor — no separate
        // placement-new is needed after this call.
        IComponent* AllocateRaw(uint32_t entityIndex) override {
            TComponent* ptr = Allocate(entityIndex);
            return ptr; // implicit upcast to IComponent*
        }

        // --- Typed helpers ---

        // Allocate a new component slot for an entity.
        // Returns nullptr if pool is full or entity already has this component.
        TComponent* Allocate(uint32_t entityIndex) {
            DIA_ASSERT(!mEntityToSlot[entityIndex].IsValid(),
                "ComponentPool::Allocate — entity %u already has component of type %s",
                entityIndex, mTypeId.AsChar());
            if (mPool.IsFull()) return nullptr;
            SlotHandle h = mPool.Allocate();
            mEntityToSlot[entityIndex] = h;
            return mPool.Get(h);
        }

        TComponent* GetTyped(uint32_t entityIndex) {
            SlotHandle h = mEntityToSlot[entityIndex];
            if (!h.IsValid()) return nullptr;
            return mPool.Get(h);
        }

        const TComponent* GetTyped(uint32_t entityIndex) const {
            SlotHandle h = mEntityToSlot[entityIndex];
            if (!h.IsValid()) return nullptr;
            return mPool.Get(h);
        }

        uint32_t Size() const override {
            return mPool.GetSize();
        }

        template<typename Visitor>
        void ForEach(const Visitor& visitor) {
            mPool.ForEach(visitor);
        }

    private:
        Dia::Core::StringCRC               mTypeId;
        Dia::Core::HandlePool<TComponent, kCapacity> mPool;
        SlotHandle                         mEntityToSlot[kMaxEntitiesPerDomain];
    };

} // namespace Dia::Entity
