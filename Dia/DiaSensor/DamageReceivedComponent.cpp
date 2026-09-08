#include <DiaSensor/DamageReceivedComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Reflect/ReflectMacros.h>

// Serialize free function — no serializable FIELDs on this component.
DIA_SERIALIZE(Dia::Sensor::DamageReceivedComponent, Dia::Sensor::DamageReceivedComponent::kVersion)
DIA_SERIALIZE_END

namespace Dia::Sensor {

DIA_COMPONENT_REGISTER(DamageReceivedComponent, "damage-received-component", false, true,
    nullptr, 0,
    nullptr, 0,
    nullptr, 0)

void DamageReceivedComponent::AddDamage(Dia::Entity::Entity source, float amount, int frameNumber)
{
    if (!pendingEvents.IsFull())
    {
        DamageEvent ev;
        ev.source    = source;
        ev.amount    = amount;
        ev.timestamp = frameNumber;
        pendingEvents.Add(ev);
    }
}

void DamageReceivedComponent::Clear()
{
    pendingEvents.RemoveAll();
}

} // namespace Dia::Sensor
