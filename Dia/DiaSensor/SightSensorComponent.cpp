#include <DiaSensor/SightSensorComponent.h>
#include <DiaSensor/SensorResultsComponent.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Reflect/ReflectMacros.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <cmath>

// Serialize free function — FIELD members serialized by name.
DIA_SERIALIZE(Dia::Sensor::SightSensorComponent, Dia::Sensor::SightSensorComponent::kVersion)
    DIA_FIELD(range)
    DIA_FIELD(halfAngle)
    DIA_FIELD(layerMask)
    DIA_FIELD(tickInterval)
DIA_SERIALIZE_END

namespace Dia::Sensor {

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

static Dia::Entity::FieldDesc s_SightSensorComponent_fields[] = {
    DIA_FIELD_ENTRY(float,    range,        SightSensorComponent)
    DIA_FIELD_ENTRY(float,    halfAngle,    SightSensorComponent)
    DIA_FIELD_ENTRY(uint32_t, layerMask,    SightSensorComponent)
    DIA_FIELD_ENTRY(int32_t,  tickInterval, SightSensorComponent)
};

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static Dia::Core::StringCRC s_SightSensorComponent_reqs[] = {
    DIA_REQ_ENTRY(Dia::EntitySpatial::SpatialComponent)
    DIA_REQ_ENTRY(Dia::Sensor::SensorResultsComponent)
};

DIA_COMPONENT_REGISTER(SightSensorComponent, "sight-sensor-component", false, true,
    s_SightSensorComponent_fields, DIA_ARRAY_COUNT(s_SightSensorComponent_fields),
    s_SightSensorComponent_reqs, DIA_ARRAY_COUNT(s_SightSensorComponent_reqs),
    nullptr, 0)

void SightSensorComponent::Tick(
    Dia::EntitySpatial::EntitySpatialModule& spatialModule,
    Dia::Entity::Domain& domain,
    const Dia::Maths::Vector2D& ownerPosition,
    const Dia::Maths::Vector2D& forwardDir,
    SensorResultsComponent& results,
    int frameNumber) const
{
    // Query spatial index for entities in the cone.
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 8> rawOut;
    spatialModule.QuerySector(ownerPosition, forwardDir, range, halfAngle, layerMask, rawOut);

    // Overwrite results on every tick (AC-2).
    results.sightResults.RemoveAll();

    const float forwardAngle = atan2f(forwardDir.y, forwardDir.x);

    for (unsigned int i = 0; i < rawOut.Size(); ++i)
    {
        if (results.sightResults.IsFull())
        {
            break;
        }

        const Dia::Entity::Entity candidate = rawOut[i];
        const Dia::EntitySpatial::SpatialComponent* spatial =
            domain.GetComponent<Dia::EntitySpatial::SpatialComponent>(candidate);

        if (spatial == nullptr)
        {
            continue;
        }

        const Dia::Maths::Vector2D toTarget = spatial->position - ownerPosition;
        const float distance = toTarget.Magnitude();
        const float targetAngle = atan2f(toTarget.y, toTarget.x);
        float angle = targetAngle - forwardAngle;

        // Normalize to [-pi, pi].
        while (angle >  3.14159265f) { angle -= 6.28318530f; }
        while (angle < -3.14159265f) { angle += 6.28318530f; }

        SightResult result;
        result.entity    = candidate;
        result.distance  = distance;
        result.angle     = angle;
        result.timestamp = frameNumber;
        results.sightResults.Add(result);
    }
}

} // namespace Dia::Sensor
