#pragma once

#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaEntity/Entity.h>

#include <DiaAttribute/AttributeSet.h>
#include <DiaAttribute/AttributeSchema.h>

namespace Dia::Entity {
    class Domain;
}

namespace Dia::Attribute {

    // AttributeSetComponent — single diaentitytemplate component wrapping one
    // AttributeSet. Attaching a schema with N attributes registers exactly one
    // component type (this one), never one component per attribute (AC-14).
    class AttributeSetComponent : public Dia::Entity::IComponent {
        DIA_COMPONENT(AttributeSetComponent, "dia.attribute.set", 1)

        FIELD(Dia::Core::StringCRC, schema_name, Dia::Core::StringCRC())

    public:
        AttributeSet&       GetAttributeSet();
        const AttributeSet& GetAttributeSet() const;

        // Populates the wrapped AttributeSet from a schema. Callers (tests today;
        // a future schema-asset-registry-aware caller later) invoke this explicitly.
        // See OnAttach() comment for why OnAttach itself does not do this yet.
        void InitializeFromSchema(const AttributeSchema& schema);

        void OnAttach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) override;

    private:
        AttributeSet mAttributeSet;
    };

} // namespace Dia::Attribute
