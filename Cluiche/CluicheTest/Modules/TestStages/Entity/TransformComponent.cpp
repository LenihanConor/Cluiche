#include "Modules/TestStages/Entity/TransformComponent.h"
#include <DiaCore/Reflect/ReflectMacros.h>
#include <DiaEntity/Domain.h>
#include <DiaMailbox/MailboxTypes.h>

DIA_SERIALIZE(CluicheTest::TransformComponent, CluicheTest::TransformComponent::kVersion)
    DIA_FIELD(x)
    DIA_FIELD(y)
DIA_SERIALIZE_END

namespace CluicheTest {

int* TransformComponent::sAttachCounter         = nullptr;
int* TransformComponent::sDetachCounter         = nullptr;
int* TransformComponent::sMailboxReceiveCounter = nullptr;

void TransformComponent::OnAttach(Dia::Entity::Domain& /*domain*/, Dia::Entity::Entity /*self*/)
{
    if (sAttachCounter)
        ++(*sAttachCounter);
}

void TransformComponent::OnDetach(Dia::Entity::Domain& /*domain*/, Dia::Entity::Entity /*self*/)
{
    if (sDetachCounter)
        ++(*sDetachCounter);
}

void TransformComponent::DoUpdate(Dia::Entity::Domain& domain, Dia::Entity::Entity /*self*/, float /*dt*/)
{
    // Drain any entity.ping mailbox messages and count them
    domain.GetMailbox().Drain<Dia::Core::StringCRC>(
        [](const Dia::Mailbox::Address& /*addr*/, const Dia::Core::StringCRC& /*msg*/) {
            if (TransformComponent::sMailboxReceiveCounter)
                ++(*TransformComponent::sMailboxReceiveCounter);
        });
}

static Dia::Entity::FieldDesc s_TransformComponent_fields[] = {
    DIA_FIELD_ENTRY(float, x, TransformComponent)
    DIA_FIELD_ENTRY(float, y, TransformComponent)
};

DIA_COMPONENT_REGISTER(TransformComponent, "cluichetest.transform", true,
    s_TransformComponent_fields, DIA_ARRAY_COUNT(s_TransformComponent_fields),
    nullptr, 0)

} // namespace CluicheTest
