#pragma once
#ifndef DIA_SENSOR_SENSORRESULTSCOMPONENT_H
#define DIA_SENSOR_SENSORRESULTSCOMPONENT_H

#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaSensor/SensorResults.h>

namespace Dia::Sensor {

// SensorResultsComponent — per-entity raw perception store.
// Owned by DiaSensor; attached to any entity with at least one sensor component.
// Arrays are NOT cleared between ticks — stale results persist until the next
// tick overwrites them. This is intentional: consumers always see the last known state.
class SensorResultsComponent : public Dia::Entity::IComponent {
    DIA_COMPONENT(SensorResultsComponent, "sensor-results-component", 1)
    DIA_READONLY

public:
    Dia::Core::Containers::DynamicArrayC<SightResult, 8>      sightResults;
    Dia::Core::Containers::DynamicArrayC<ProximityResult, 16> proximityResults;
    Dia::Core::Containers::DynamicArrayC<DamageEvent, 4>      damageEvents;
    Dia::Core::Containers::DynamicArrayC<SoundEvent, 4>       soundEvents;
};

} // namespace Dia::Sensor

#endif // DIA_SENSOR_SENSORRESULTSCOMPONENT_H
