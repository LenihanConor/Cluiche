#pragma once
#ifndef DIA_SENSOR_SOUNDSENSORCOMPONENT_H
#define DIA_SENSOR_SOUNDSENSORCOMPONENT_H

#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace Sensor { class SensorResultsComponent; } }
namespace Dia { namespace Sensor { class SoundEventList; } }

namespace Dia::Sensor {

// SoundSensorComponent — radius-based sound perception sensor.
// SensorModule calls Tick() once per tickInterval frames.
// Queries the frame-local SoundEventList for events within hearingRadius.
// Writes matching events to SensorResultsComponent::soundEvents.
class SoundSensorComponent : public Dia::Entity::IComponent {
    DIA_COMPONENT(SoundSensorComponent, "sound-sensor-component", 1)
    DIA_READONLY

    FIELD(float, hearingRadius, 8.f)
    FIELD(int,   tickInterval,  2)

public:
    // Called by SensorModule when tickCountdown reaches zero.
    // soundList is the frame-local list of emitted sound events.
    // ownerPosition comes from the owner's SpatialComponent.
    // results is this entity's SensorResultsComponent.
    void Tick(const SoundEventList& soundList,
              const Dia::Maths::Vector2D& ownerPosition,
              SensorResultsComponent& results) const;
};

} // namespace Dia::Sensor
#endif // DIA_SENSOR_SOUNDSENSORCOMPONENT_H
