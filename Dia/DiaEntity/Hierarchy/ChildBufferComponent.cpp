#include <DiaEntity/Hierarchy/ChildBufferComponent.h>
#include <DiaEntity/Hierarchy/ParentComponent.h>
#include <DiaEntity/Domain.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

// Serialize free function — ChildBufferComponent has no serialised fields.
// The children array is bookkeeping managed at runtime.
DIA_SERIALIZE(Dia::Entity::Hierarchy::ChildBufferComponent, Dia::Entity::Hierarchy::ChildBufferComponent::kVersion)
DIA_SERIALIZE_END

namespace Dia::Entity::Hierarchy {

// ChildBufferComponent has no fields to register.
DIA_COMPONENT_REGISTER(ChildBufferComponent, "dia.hierarchy.children", false, false,
    nullptr, 0,
    nullptr, 0,
    nullptr, 0)
DIA_COMPONENT_DESCRIBE(Dia::Entity::Hierarchy::ChildBufferComponent, "Stores up to kMaxChildren child entity handles for a parent entity in the hierarchy.")

bool ChildBufferComponent::AddChild(Dia::Entity::Entity child) {
    if (children.IsFull()) {
#ifdef DEBUG
        DIA_ASSERT(false, "ChildBufferComponent: overflow — more than %u children on one entity",
            kMaxChildren);
#else
        DIA_LOG_WARNING("diaentitytemplate", "ChildBufferComponent: overflow — more than %u children on one entity; ignoring",
            kMaxChildren);
#endif
        return false;
    }
    children.Add(child);
    return true;
}

bool ChildBufferComponent::RemoveChild(Dia::Entity::Entity child) {
    int idx = children.FindIndex(child);
    if (idx < 0) {
        return false;
    }
    children.RemoveAt(static_cast<unsigned int>(idx));
    return true;
}

void ChildBufferComponent::OnAttach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) {
    // Scan for any live entities that have a ParentComponent pointing to `self`.
    // This handles the reparent case where ChildBufferComponent is freshly added to
    // a parent that already has children with updated ParentComponent fields in place.
    // O(kMaxEntitiesPerDomain) scan, fires only once per ChildBufferComponent add.
    for (uint32_t idx = 0; idx < kMaxEntitiesPerDomain; ++idx) {
        Dia::Entity::Entity candidate = domain.GetAliveEntity(idx);
        if (!candidate.IsValid()) {
            continue;
        }
        const ParentComponent* pc = domain.GetComponent<ParentComponent>(candidate);
        if (pc == nullptr) {
            continue;
        }
        if (pc->GetParentEntity() == self) {
            // This entity considers 'self' its parent — add it to the children buffer.
            AddChild(candidate);
        }
    }
}

} // namespace Dia::Entity::Hierarchy
