#include "TestComponent.h"

// Serialize free function — drives JSON load/save via the loadFromJson/saveToJson thunks.
// Must be defined before DIA_COMPONENT_REGISTER in the same translation unit.
DIA_SERIALIZE(DiaEntityTest::TestComponent, DiaEntityTest::TestComponent::kVersion)
    DIA_FIELD(speed)
    DIA_FIELD(hitPoints)
DIA_SERIALIZE_END

namespace DiaEntityTest {

// Field metadata array — one entry per FIELD declared in the class.
static Dia::Entity::FieldDesc s_TestComponent_fields[] = {
    DIA_FIELD_ENTRY(float,   speed,     TestComponent)
    DIA_FIELD_ENTRY(int32_t, hitPoints, TestComponent)
};

// Registration — defines kTypeId, GetDesc(), and triggers ComponentRegistry entry.
// StringName must match the string passed to DIA_COMPONENT in the header.
// No REQUIRES, so pass nullptr/0 for requirements.
DIA_COMPONENT_REGISTER(TestComponent, "test-component", false,
    s_TestComponent_fields, DIA_ARRAY_COUNT(s_TestComponent_fields),
    nullptr, 0)

} // namespace DiaEntityTest
