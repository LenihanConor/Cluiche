#pragma once
#include <diaentitytemplate/IComponent.h>
#include <diaentitytemplate/ComponentMacros.h>

namespace DiaEntityVisualDebuggerTest {

// Minimal component with x/y fields — used to test EntityPositionHelper.
class TestPosComponent : public Dia::Entity::IComponent {
    DIA_COMPONENT(TestPosComponent, "evd-test.pos", 1)
    FIELD(float, x, 0.0f)
    FIELD(float, y, 0.0f)
};

} // namespace DiaEntityVisualDebuggerTest
