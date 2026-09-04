#include <DiaAttribute/AttributeSetComponent.h>
#include <DiaEntity/Domain.h>

// Serialize free function — reads schema_name from JSON config.
DIA_SERIALIZE(Dia::Attribute::AttributeSetComponent, Dia::Attribute::AttributeSetComponent::kVersion)
    DIA_FIELD(schema_name)
DIA_SERIALIZE_END

namespace Dia::Attribute {

// Field metadata array — one per FIELD in AttributeSetComponent.
static Dia::Entity::FieldDesc s_AttributeSetComponent_fields[] = {
    DIA_FIELD_ENTRY(Dia::Core::StringCRC, schema_name, AttributeSetComponent)
};

DIA_COMPONENT_REGISTER(AttributeSetComponent, "dia.attribute.set", false, false,
    s_AttributeSetComponent_fields, DIA_ARRAY_COUNT(s_AttributeSetComponent_fields),
    nullptr, 0,
    nullptr, 0)
DIA_COMPONENT_DESCRIBE(Dia::Attribute::AttributeSetComponent, "Wraps one AttributeSet — resolved base+modifier attribute values for this entity.")

AttributeSet& AttributeSetComponent::GetAttributeSet()
{
    return mAttributeSet;
}

const AttributeSet& AttributeSetComponent::GetAttributeSet() const
{
    return mAttributeSet;
}

void AttributeSetComponent::InitializeFromSchema(const AttributeSchema& schema)
{
    mAttributeSet.InitializeFromSchema(schema);
}

void AttributeSetComponent::OnAttach(Dia::Entity::Domain& /*domain*/, Dia::Entity::Entity /*self*/)
{
    // Schema resolution from schema_name requires a schema-asset registry, which does not
    // exist yet. No-op for now; callers populate mAttributeSet directly via InitializeFromSchema.
}

} // namespace Dia::Attribute
