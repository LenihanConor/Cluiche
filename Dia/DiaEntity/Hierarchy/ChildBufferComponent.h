#pragma once
#include <diaentitytemplate/IComponent.h>
#include <diaentitytemplate/ComponentMacros.h>
#include <diaentitytemplate/Entity.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Entity::Hierarchy {

    // ChildBufferComponent — stores up to kMaxChildren child entity handles.
    // Only entities that have children carry this component.
    // The children array is bookkeeping only — NOT serialised via FIELD.
    class ChildBufferComponent : public Dia::Entity::IComponent {
        DIA_COMPONENT(ChildBufferComponent, "dia.hierarchy.children", 1)

    public:
        static constexpr uint32_t kMaxChildren = 16;

        Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, kMaxChildren> children;

        // Returns true if child was added. False + assert/log if full.
        bool AddChild(Dia::Entity::Entity child);

        // Returns true if child was found and removed. False (silent) if not found.
        bool RemoveChild(Dia::Entity::Entity child);

        // OnAttach: scans for any entities with ParentComponent pointing to self
        // and adds them to the children buffer. This handles the reparent case
        // where ChildBufferComponent is freshly added to a parent that already
        // has children with live ParentComponent.
        void OnAttach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) override;
    };

} // namespace Dia::Entity::Hierarchy
