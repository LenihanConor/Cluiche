#pragma once
#ifndef DIA_SENSOR_SIGHTSENSORCOMPONENT_H
#define DIA_SENSOR_SIGHTSENSORCOMPONENT_H

#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace Entity { class Domain; } }
namespace Dia { namespace EntitySpatial { class EntitySpatialModule; } }
namespace Dia { namespace Sensor { class SensorResultsComponent; } }

namespace Dia::Sensor {

// SightSensorComponent — cone-based perception sensor.
// SensorModule calls Tick() once per tickInterval frames.
// Requires SpatialComponent on the same entity (for position).
// Writes results to SensorResultsComponent::sightResults.
class SightSensorComponent : public Dia::Entity::IComponent {
    DIA_COMPONENT(SightSensorComponent, "sight-sensor-component", 1)
    DIA_READONLY

    FIELD(float,    range,        10.f)
    FIELD(float,    halfAngle,    0.785398f)  // ~45 degrees in radians
    FIELD(uint32_t, layerMask,    0x7FFFFFFFu)
    FIELD(int,      tickInterval, 3)

public:
    // Called by SensorModule when tickCountdown reaches zero.
    // spatialModule is the domain's EntitySpatialModule.
    // domain is used to look up SpatialComponent on each result entity.
    // ownerPosition and forwardDir come from the owner's SpatialComponent.
    // results is this entity's SensorResultsComponent.
    // frameNumber is the current frame (written to SightResult::timestamp).
    void Tick(Dia::EntitySpatial::EntitySpatialModule& spatialModule,
              Dia::Entity::Domain& domain,
              const Dia::Maths::Vector2D& ownerPosition,
              const Dia::Maths::Vector2D& forwardDir,
              SensorResultsComponent& results,
              int frameNumber) const;
};

} // namespace Dia::Sensor
#endif // DIA_SENSOR_SIGHTSENSORCOMPONENT_H
