#pragma once
#ifndef DIA_SENSOR_SENSORBLACKBOARDADAPTER_H
#define DIA_SENSOR_SENSORBLACKBOARDADAPTER_H

#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>

namespace Dia { namespace Blackboard { class BlackboardComponent; } }
namespace Dia::Sensor { class SensorResultsComponent; }

namespace Dia::Sensor {

// Abstract base for sensor-to-blackboard distillation — carried as an entity component.
// T7 provides DefaultSensorBlackboardAdapter (concrete subclass).
//
// Each entity archetype that wants AI-readable perception data attaches a subclass of
// SensorBlackboardAdapter. SensorModule::RunAdapters() calls Distil() on every entity
// that carries one, after all sensor ticks for the frame have completed (AC-8).
//
// NOTE: DIA_COMPONENT_REGISTER is NOT called for this abstract base.
//       T7 handles concrete subclass registration.
class SensorBlackboardAdapter : public Dia::Entity::IComponent {
    DIA_COMPONENT(SensorBlackboardAdapter, "sensor-blackboard-adapter", 1)

public:
    // Distil raw sensor results into AI-readable blackboard slots.
    // Called exactly once per frame per entity by SensorModule::RunAdapters().
    // Must NOT be called from sensor component Tick methods (AC-8).
    virtual void Distil(const SensorResultsComponent& results,
                        Dia::Blackboard::BlackboardComponent& blackboard,
                        int frameNumber) const = 0;

    // Returns the BlackboardComponent this adapter writes into.
    // Concrete subclasses provide the blackboard — T7 wires this in OnAttach.
    virtual Dia::Blackboard::BlackboardComponent& GetBlackboard() = 0;
};

} // namespace Dia::Sensor

#endif // DIA_SENSOR_SENSORBLACKBOARDADAPTER_H
