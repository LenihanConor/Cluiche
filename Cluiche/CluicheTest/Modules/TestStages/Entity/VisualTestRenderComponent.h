#pragma once
#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/CRC/StringCRC.h>

namespace CluicheTest {

class VisualTestRenderComponent : public Dia::Entity::IComponent {
    DIA_COMPONENT(VisualTestRenderComponent, "cluichetest.visual-test-render", 1)

    FIELD(float,    radius, 12.0f)
    FIELD(uint32_t, colour, 0x4FC3F7FFu)

public:
    void OnAttach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) override;
    void OnDetach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) override;

    static int* sAttachCounter;
    static int* sDetachCounter;
};

} // namespace CluicheTest
