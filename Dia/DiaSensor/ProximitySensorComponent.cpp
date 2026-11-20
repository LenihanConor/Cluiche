#include <DiaSensor/ProximitySensorComponent.h>
#include <DiaSensor/SensorResultsComponent.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Reflect/ReflectMacros.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaGeometry2D/Shapes/Circle.h>

// Serialize free function — FIELD members serialized by name.
DIA_SERIALIZE(Dia::Sensor::ProximitySensorComponent, Dia::Sensor::ProximitySensorComponent::kVersion)
    DIA_FIELD(radius)
    DIA_FIELD(layerMask)
    DIA_FIELD(tickInterval)
DIA_SERIALIZE_END

namespace Dia::Sensor {

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

static Dia::Entity::FieldDesc s_ProximitySensorComponent_fields[] = {
    DIA_FIELD_ENTRY(float,    radius,       ProximitySensorComponent)
    DIA_FIELD_ENTRY(uint32_t, layerMask,    ProximitySensorComponent)
    DIA_FIELD_ENTRY(int32_t,  tickInterval, ProximitySensorComponent)
};

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static Dia::Core::StringCRC s_ProximitySensorComponent_reqs[] = {
    DIA_REQ_ENTRY(Dia::EntitySpatial::SpatialComponent)
    DIA_REQ_ENTRY(Dia::Sensor::SensorResultsComponent)
};

DIA_COMPONENT_REGISTER(ProximitySensorComponent, "proximity-sensor-component", false, true,
    s_ProximitySensorComponent_fields, DIA_ARRAY_COUNT(s_ProximitySensorComponent_fields),
    s_ProximitySensorComponent_reqs, DIA_ARRAY_COUNT(s_ProximitySensorComponent_reqs),
    nullptr, 0)

void ProximitySensorComponent::Tick(
    Dia::EntitySpatial::EntitySpatialModule& spatialModule,
    Dia::Entity::Domain& domain,
    const Dia::Maths::Vector2D& ownerPosition,
    SensorResultsComponent& results,
    int /*frameNumber*/) const
{
    // Query spatial index for all entities within the radius.
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 16> rawOut;
    const Dia::Geometry2D::Circle circle(radius, ownerPosition);
    spatialModule.QueryCircle(circle, layerMask, rawOut);

    // Overwrite results on every tick (AC-2).
    results.proximityResults.RemoveAll();

    for (unsigned int i = 0; i < rawOut.Size(); ++i)
    {
        if (results.proximityResults.IsFull())
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

        ProximityResult result;
        result.entity   = candidate;
        result.distance = distance;
        results.proximityResults.Add(result);
    }
}

} // namespace Dia::Sensor
