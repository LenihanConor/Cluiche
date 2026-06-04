#include "Modules/TestStages/Entity/VisualTestRenderComponent.h"
#include <DiaCore/Reflect/ReflectMacros.h>
#include <diaentitytemplate/Domain.h>

DIA_SERIALIZE(CluicheTest::VisualTestRenderComponent, CluicheTest::VisualTestRenderComponent::kVersion)
    DIA_FIELD(radius)
    DIA_FIELD(colour)
DIA_SERIALIZE_END

namespace CluicheTest {

static Dia::Entity::FieldDesc s_VisualTestRenderComponent_fields[] = {
    DIA_FIELD_ENTRY(float,    radius, VisualTestRenderComponent)
    DIA_FIELD_ENTRY(uint32_t, colour, VisualTestRenderComponent)
};

DIA_COMPONENT_REGISTER(VisualTestRenderComponent, "cluichetest.visual-test-render", false, false,
    s_VisualTestRenderComponent_fields, DIA_ARRAY_COUNT(s_VisualTestRenderComponent_fields),
    nullptr, 0,
    nullptr, 0)
DIA_COMPONENT_DESCRIBE(CluicheTest::VisualTestRenderComponent, "Stores render properties (radius, colour) for circle-based visual representation in the entity test stage.")

} // namespace CluicheTest
