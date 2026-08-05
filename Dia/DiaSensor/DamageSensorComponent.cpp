#include <DiaSensor/DamageSensorComponent.h>
#include <DiaSensor/DamageReceivedComponent.h>
#include <DiaSensor/SensorResultsComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Reflect/ReflectMacros.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

// Serialize free function — FIELD members serialized by name.
DIA_SERIALIZE(Dia::Sensor::DamageSensorComponent, Dia::Sensor::DamageSensorComponent::kVersion)
    DIA_FIELD(pruneWindowFrames)
    DIA_FIELD(tickInterval)
DIA_SERIALIZE_END

namespace Dia::Sensor {

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

static Dia::Entity::FieldDesc s_DamageSensorComponent_fields[] = {
    DIA_FIELD_ENTRY(int32_t, pruneWindowFrames, DamageSensorComponent)
    DIA_FIELD_ENTRY(int32_t, tickInterval,      DamageSensorComponent)
};

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static Dia::Core::StringCRC s_DamageSensorComponent_reqs[] = {
    DIA_REQ_ENTRY(Dia::Sensor::DamageReceivedComponent)
    DIA_REQ_ENTRY(Dia::Sensor::SensorResultsComponent)
};

DIA_COMPONENT_REGISTER(DamageSensorComponent, "damage-sensor-component", false, true,
    s_DamageSensorComponent_fields, DIA_ARRAY_COUNT(s_DamageSensorComponent_fields),
    s_DamageSensorComponent_reqs,   DIA_ARRAY_COUNT(s_DamageSensorComponent_reqs),
    nullptr, 0)

void DamageSensorComponent::Tick(
    DamageReceivedComponent& damageReceived,
    SensorResultsComponent&  results,
    int                      frameNumber) const
{
    // Step 1: Prune stale events — keep only events within the window.
    Dia::Core::Containers::DynamicArrayC<DamageEvent, 4> kept;
    for (unsigned int i = 0; i < results.damageEvents.Size(); ++i)
    {
        const DamageEvent& ev = results.damageEvents[i];
        if ((frameNumber - ev.timestamp) <= pruneWindowFrames)
        {
            kept.Add(ev);
        }
    }
    results.damageEvents = kept;

    // Step 2: Append new events from pendingEvents (stop if full).
    for (unsigned int i = 0; i < damageReceived.pendingEvents.Size(); ++i)
    {
        if (results.damageEvents.IsFull())
        {
            break;
        }
        results.damageEvents.Add(damageReceived.pendingEvents[i]);
    }

    // Step 3: Clear the source buffer now that events have been consumed.
    damageReceived.Clear();
}

} // namespace Dia::Sensor
