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

} // namespace Dia::Entity
