#pragma once
#include <cstdint>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>

namespace DiaEntityTesting {

    // Nested value struct used to exercise FieldKind::Nested defaults.
    struct TestDescribedNested {
        float   gain  = 2.0f;
        int32_t count = 4;
    };

    // Test fixture component with non-zero field defaults. The registration
    // (DIA_SERIALIZE / DIA_FIELD_ENTRY / DIA_COMPONENT_REGISTER) lives in the
    // consuming test project, in one .cpp (see TestDescribedComponent.cpp).
    class TestDescribedComponent : public Dia::Entity::IComponent {
        DIA_COMPONENT(TestDescribedComponent, "test-described-component", 1)

        FIELD(float,   speed,     3.5f)
        FIELD(int32_t, hitPoints, 7)
        FIELD(bool,    active,    true)
        FIELD(Dia::Core::StringCRC, tag, Dia::Core::StringCRC("default-tag"))
        FIELD(TestDescribedNested, nested, TestDescribedNested{})
    };

} // namespace DiaEntityTesting
