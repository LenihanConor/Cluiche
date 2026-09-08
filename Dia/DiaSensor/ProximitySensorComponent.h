#pragma once
#ifndef DIA_SENSOR_PROXIMITYSENSORCOMPONENT_H
#define DIA_SENSOR_PROXIMITYSENSORCOMPONENT_H

#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace Entity { class Domain; } }
namespace Dia { namespace EntitySpatial { class EntitySpatialModule; } }
namespace Dia { namespace Sensor { class SensorResultsComponent; } }

namespace Dia::Sensor {

// ProximitySensorComponent — radius-based omnidirectional perception sensor.
// SensorModule calls Tick() once per tickInterval frames.
// Requires SpatialComponent on the same entity (for position).
// Writes results to SensorResultsComponent::proximityResults.
class ProximitySensorComponent : public Dia::Entity::IComponent {
    DIA_COMPONENT(ProximitySensorComponent, "proximity-sensor-component", 1)
    DIA_READONLY

    FIELD(float,    radius,       5.f)
    FIELD(uint32_t, layerMask,    0x7FFFFFFFu)
    FIELD(int,      tickInterval, 3)

public:
    // Called by SensorModule when tickCountdown reaches zero.
    // spatialModule is the domain's EntitySpatialModule.
    // domain is used to look up SpatialComponent on each result entity.
    // ownerPosition comes from the owner's SpatialComponent.
    // results is this entity's SensorResultsComponent.
    // frameNumber is the current frame (unused by proximity, reserved for symmetry).
    void Tick(Dia::EntitySpatial::EntitySpatialModule& spatialModule,
              Dia::Entity::Domain& domain,
              const Dia::Maths::Vector2D& ownerPosition,
              SensorResultsComponent& results,
              int frameNumber) const;
};

} // namespace Dia::Sensor
#endif // DIA_SENSOR_PROXIMITYSENSORCOMPONENT_H
