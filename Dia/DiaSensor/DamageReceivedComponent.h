#pragma once
#ifndef DIA_SENSOR_DAMAGERECEIVEDCOMPONENT_H
#define DIA_SENSOR_DAMAGERECEIVEDCOMPONENT_H

#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaSensor/SensorResults.h>

namespace Dia::Sensor {

// DamageReceivedComponent — written by combat/ability systems when an entity
// takes damage. DiaSensor reads this each tick to populate DamageEvents.
// This component is intentionally defined in DiaSensor to avoid coupling
// DiaSensor to a combat system that doesn't exist yet.
class DamageReceivedComponent : public Dia::Entity::IComponent {
    DIA_COMPONENT(DamageReceivedComponent, "damage-received-component", 1)
    DIA_READONLY

public:
    static constexpr unsigned int kMaxPendingDamage = 8;
    Dia::Core::Containers::DynamicArrayC<DamageEvent, kMaxPendingDamage> pendingEvents;

    void AddDamage(Dia::Entity::Entity source, float amount, int frameNumber);
    void Clear();
};

} // namespace Dia::Sensor

#endif // DIA_SENSOR_DAMAGERECEIVEDCOMPONENT_H
