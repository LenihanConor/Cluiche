#include <DiaSensor/SoundSensorComponent.h>
#include <DiaSensor/SoundEventList.h>
#include <DiaSensor/SensorResultsComponent.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Reflect/ReflectMacros.h>
#include <cmath>

// Serialize free function — FIELD members serialized by name.
DIA_SERIALIZE(Dia::Sensor::SoundSensorComponent, Dia::Sensor::SoundSensorComponent::kVersion)
    DIA_FIELD(hearingRadius)
    DIA_FIELD(tickInterval)
DIA_SERIALIZE_END

namespace Dia::Sensor {

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

static Dia::Entity::FieldDesc s_SoundSensorComponent_fields[] = {
    DIA_FIELD_ENTRY(float,   hearingRadius, SoundSensorComponent)
    DIA_FIELD_ENTRY(int32_t, tickInterval,  SoundSensorComponent)
};

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static Dia::Core::StringCRC s_SoundSensorComponent_reqs[] = {
    DIA_REQ_ENTRY(Dia::EntitySpatial::SpatialComponent)
    DIA_REQ_ENTRY(Dia::Sensor::SensorResultsComponent)
};

DIA_COMPONENT_REGISTER(SoundSensorComponent, "sound-sensor-component", false, true,
    s_SoundSensorComponent_fields, DIA_ARRAY_COUNT(s_SoundSensorComponent_fields),
    s_SoundSensorComponent_reqs, DIA_ARRAY_COUNT(s_SoundSensorComponent_reqs),
    nullptr, 0)

void SoundSensorComponent::Tick(
    const SoundEventList& soundList,
    const Dia::Maths::Vector2D& ownerPosition,
    SensorResultsComponent& results) const
{
    // Overwrite results on every tick (AC-5).
    results.soundEvents.RemoveAll();

    const auto& events = soundList.GetEvents();

    for (unsigned int i = 0; i < events.Size(); ++i)
    {
        if (results.soundEvents.IsFull())
        {
            break;
        }

        const SoundEmission& emission = events[i];

        const Dia::Maths::Vector2D delta = emission.event.position - ownerPosition;
        const float distance = sqrtf(delta.x * delta.x + delta.y * delta.y);

        if (distance <= emission.emissionRadius && distance <= hearingRadius)
        {
            results.soundEvents.Add(emission.event);
        }
    }
}

} // namespace Dia::Sensor
