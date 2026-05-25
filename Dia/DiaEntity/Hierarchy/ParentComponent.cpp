#include <DiaEntity/Hierarchy/ParentComponent.h>
#include <DiaEntity/Hierarchy/ChildBufferComponent.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentPool.h>

// Serialize free function — reads parentIndex and parentGen from JSON config.
DIA_SERIALIZE(Dia::Entity::Hierarchy::ParentComponent, Dia::Entity::Hierarchy::ParentComponent::kVersion)
    DIA_FIELD(parentIndex)
    DIA_FIELD(parentGen)
DIA_SERIALIZE_END

namespace Dia::Entity::Hierarchy {

// Field metadata array — one per FIELD in ParentComponent.
static Dia::Entity::FieldDesc s_ParentComponent_fields[] = {
    DIA_FIELD_ENTRY(uint32_t, parentIndex, ParentComponent)
    DIA_FIELD_ENTRY(uint32_t, parentGen,   ParentComponent)
};

DIA_COMPONENT_REGISTER(ParentComponent, "dia.hierarchy.parent", false,
    s_ParentComponent_fields, DIA_ARRAY_COUNT(s_ParentComponent_fields),
    nullptr, 0)

void ParentComponent::OnAttach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) {
    Entity parent(parentIndex, parentGen);
    if (!domain.IsAlive(parent)) {
        return;
    }

    // Try to get an existing ChildBufferComponent on the parent.
    ChildBufferComponent* cb = domain.GetComponent<ChildBufferComponent>(parent);
    if (cb != nullptr) {
        cb->AddChild(self);
    }
    // If cb is null, parent's ChildBufferComponent was queued in the same EndOfFrame
    // pass. Because QueueSetParent queues ChildBufferComponent BEFORE ParentComponent,
    // cb will have been constructed before this OnAttach fires; cb should be non-null.
    // If it is null (caller error or direct use), the child won't be in the buffer —
    // that is acceptable for v1 and documented in Hierarchy::QueueSetParent.
}

void ParentComponent::OnDetach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) {
    Entity parent(parentIndex, parentGen);
    if (!domain.IsAlive(parent)) {
        return;
    }

    ChildBufferComponent* cb = domain.GetComponent<ChildBufferComponent>(parent);
    if (cb != nullptr) {
        cb->RemoveChild(self);
    }
}

} // namespace Dia::Entity::Hierarchy
