#pragma once
#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaEntity/Entity.h>

namespace Dia::Entity {
    class Domain;
}

namespace Dia::Entity::Hierarchy {

    // ParentComponent — marks an entity as a child in the hierarchy.
    // Root entities have no ParentComponent.
    // Added via Hierarchy::QueueSetParent; removed via QueueSetParent with Entity::Invalid parent
    // or QueueDestroySubtree.
    class ParentComponent : public Dia::Entity::IComponent {
        DIA_COMPONENT(ParentComponent, "dia.hierarchy.parent", 1)

        FIELD(uint32_t, parentIndex, 0xFFFFFFFFu)
        FIELD(uint32_t, parentGen,   0u)

    public:
        Entity GetParentEntity() const { return Entity(parentIndex, parentGen); }

        void OnAttach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) override;
        void OnDetach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) override;
    };

} // namespace Dia::Entity::Hierarchy
