#pragma once
#include <diaentitytemplate/IComponent.h>
#include <diaentitytemplate/ComponentMacros.h>

namespace DiaEntityTest {

    // Minimal test component for blueprint loader tests.
    // Type string: "bp-transform"
    class BpTransform : public Dia::Entity::IComponent {
        DIA_COMPONENT(BpTransform, "bp-transform", 1)

        FIELD(float, x, 0.0f)
        FIELD(float, y, 0.0f)
    };

    // Minimal test component for blueprint loader multi-component tests.
    // Type string: "bp-health"
    class BpHealth : public Dia::Entity::IComponent {
        DIA_COMPONENT(BpHealth, "bp-health", 1)

        FIELD(int32_t, maxHp, 100)
    };

} // namespace DiaEntityTest
