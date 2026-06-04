#pragma once
#include <diaentitytemplate/IComponent.h>
#include <diaentitytemplate/ComponentMacros.h>

namespace DiaEntityTest {

    // Minimal test component used to verify the DIA_COMPONENT macro system,
    // ComponentRegistry, and Domain::ApplyAddComponent wiring.
    class TestComponent : public Dia::Entity::IComponent {
        DIA_COMPONENT(TestComponent, "test-component", 1)

        FIELD(float,   speed,     1.0f)
        FIELD(int32_t, hitPoints, 100)

    public:
        // Track OnAttach/OnDetach call counts for lifecycle assertions.
        int attachCount  = 0;
        int detachCount  = 0;

        void OnAttach(Dia::Entity::Domain& /*domain*/, Dia::Entity::Entity /*self*/) override {
            ++attachCount;
        }

        void OnDetach(Dia::Entity::Domain& /*domain*/, Dia::Entity::Entity /*self*/) override {
            ++detachCount;
        }
    };

} // namespace DiaEntityTest
