#pragma once
#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>

namespace CluicheTest {

class TransformComponent : public Dia::Entity::IComponent {
    DIA_COMPONENT(TransformComponent, "cluichetest.transform", 1)

    FIELD(float, x, 0.0f)
    FIELD(float, y, 0.0f)

    DIA_UPDATABLE

public:
    void OnAttach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) override;
    void OnDetach(Dia::Entity::Domain& domain, Dia::Entity::Entity self) override;
    void DoUpdate(Dia::Entity::Domain& domain, Dia::Entity::Entity self, float dt) override;

    // Injected by EntityTestStageModule before pool registration
    static int* sAttachCounter;
    static int* sDetachCounter;
    static int* sMailboxReceiveCounter;
};

} // namespace CluicheTest
