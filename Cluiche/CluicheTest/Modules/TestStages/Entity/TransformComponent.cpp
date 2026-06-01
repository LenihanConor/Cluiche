#include "Modules/TestStages/Entity/TransformComponent.h"
#include <DiaCore/Reflect/ReflectMacros.h>
#include <DiaEntity/Domain.h>

DIA_SERIALIZE(CluicheTest::TransformComponent, CluicheTest::TransformComponent::kVersion)
    DIA_FIELD(x)
    DIA_FIELD(y)
DIA_SERIALIZE_END

namespace CluicheTest {

static Dia::Entity::FieldDesc s_TransformComponent_fields[] = {
    DIA_FIELD_ENTRY(float, x, TransformComponent)
    DIA_FIELD_ENTRY(float, y, TransformComponent)
};

DIA_COMPONENT_REGISTER(TransformComponent, "cluichetest.transform", false,
    s_TransformComponent_fields, DIA_ARRAY_COUNT(s_TransformComponent_fields),
    nullptr, 0)

} // namespace CluicheTest
