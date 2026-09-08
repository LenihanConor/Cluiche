#include "TestPosComponent.h"

DIA_SERIALIZE(DiaEntityVisualDebuggerTest::TestPosComponent, DiaEntityVisualDebuggerTest::TestPosComponent::kVersion)
    DIA_FIELD(x)
    DIA_FIELD(y)
DIA_SERIALIZE_END

namespace DiaEntityVisualDebuggerTest {

static Dia::Entity::FieldDesc s_TestPosComponent_fields[] = {
    DIA_FIELD_ENTRY(float, x, TestPosComponent)
    DIA_FIELD_ENTRY(float, y, TestPosComponent)
};

DIA_COMPONENT_REGISTER(TestPosComponent, "evd-test.pos", false, false,
    s_TestPosComponent_fields, DIA_ARRAY_COUNT(s_TestPosComponent_fields),
    nullptr, 0, nullptr, 0)

} // namespace DiaEntityVisualDebuggerTest
