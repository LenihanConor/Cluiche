#pragma once
#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>

namespace CluicheTest {

// Pure data component — position for visual placement.
// Module reads x/y directly for rendering and picking.
class TransformComponent : public Dia::Entity::IComponent {
    DIA_COMPONENT(TransformComponent, "cluichetest.transform", 1)

    FIELD(float, x, 0.0f)
    FIELD(float, y, 0.0f)
};

} // namespace CluicheTest
