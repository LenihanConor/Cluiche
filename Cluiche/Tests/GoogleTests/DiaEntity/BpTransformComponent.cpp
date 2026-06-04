#include "BpTransformComponent.h"
#include <DiaCore/Reflect/ReflectMacros.h>
#include <diaentitytemplate/ComponentMacros.h>

// ---------------------------------------------------------------------------
// BpTransform registration
// ---------------------------------------------------------------------------

DIA_SERIALIZE(DiaEntityTest::BpTransform, DiaEntityTest::BpTransform::kVersion)
    DIA_FIELD(x)
    DIA_FIELD(y)
DIA_SERIALIZE_END

namespace DiaEntityTest {

static Dia::Entity::FieldDesc s_BpTransform_fields[] = {
    DIA_FIELD_ENTRY(float, x, BpTransform)
    DIA_FIELD_ENTRY(float, y, BpTransform)
};

DIA_COMPONENT_REGISTER(BpTransform, "bp-transform", false, false,
    s_BpTransform_fields, DIA_ARRAY_COUNT(s_BpTransform_fields),
    nullptr, 0,
    nullptr, 0)

} // namespace DiaEntityTest

// ---------------------------------------------------------------------------
// BpHealth registration
// ---------------------------------------------------------------------------

DIA_SERIALIZE(DiaEntityTest::BpHealth, DiaEntityTest::BpHealth::kVersion)
    DIA_FIELD(maxHp)
DIA_SERIALIZE_END

namespace DiaEntityTest {

static Dia::Entity::FieldDesc s_BpHealth_fields[] = {
    DIA_FIELD_ENTRY(int32_t, maxHp, BpHealth)
};

DIA_COMPONENT_REGISTER(BpHealth, "bp-health", false, false,
    s_BpHealth_fields, DIA_ARRAY_COUNT(s_BpHealth_fields),
    nullptr, 0,
    nullptr, 0)

} // namespace DiaEntityTest
