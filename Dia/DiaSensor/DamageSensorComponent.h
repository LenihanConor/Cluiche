#pragma once
#ifndef DIA_SENSOR_DAMAGESENSORCOMPONENT_H
#define DIA_SENSOR_DAMAGESENSORCOMPONENT_H

#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>

namespace Dia { namespace Sensor { class DamageReceivedComponent; } }
namespace Dia { namespace Sensor { class SensorResultsComponent; } }

namespace Dia::Sensor {

// DamageSensorComponent — polls DamageReceivedComponent each tick and appends
// new events to SensorResultsComponent::damageEvents, pruning stale events
// older than pruneWindowFrames.
// SensorModule calls Tick() once per tickInterval frames.
class DamageSensorComponent : public Dia::Entity::IComponent {
    DIA_COMPONENT(DamageSensorComponent, "damage-sensor-component", 1)
    DIA_READONLY

    FIELD(int, pruneWindowFrames, 30)
    FIELD(int, tickInterval,       1)

public:
    // Called by SensorModule when tickCountdown reaches zero.
    // Prunes stale events from results.damageEvents, appends new events from
    // damageReceived.pendingEvents, then clears damageReceived.
    void Tick(DamageReceivedComponent& damageReceived,
              SensorResultsComponent&  results,
              int                      frameNumber) const;
};

} // namespace Dia::Sensor

#endif // DIA_SENSOR_DAMAGESENSORCOMPONENT_H
