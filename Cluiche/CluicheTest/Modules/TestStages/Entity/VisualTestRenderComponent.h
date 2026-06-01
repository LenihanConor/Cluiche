#pragma once
#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>

namespace CluicheTest {

// Pure data component — visual appearance for debug rendering.
// Module/drawer reads radius/colour directly.
class VisualTestRenderComponent : public Dia::Entity::IComponent {
    DIA_COMPONENT(VisualTestRenderComponent, "cluichetest.visual-test-render", 1)

    FIELD(float,    radius, 12.0f)
    FIELD(uint32_t, colour, 0x4FC3F7FFu)
};

} // namespace CluicheTest
