#include <DiaSensor/SensorResultsComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Reflect/ReflectMacros.h>

// Serialize free function — no serializable FIELDs on this component.
DIA_SERIALIZE(Dia::Sensor::SensorResultsComponent, Dia::Sensor::SensorResultsComponent::kVersion)
DIA_SERIALIZE_END

namespace Dia::Sensor {

DIA_COMPONENT_REGISTER(SensorResultsComponent, "sensor-results-component", false, true,
    nullptr, 0,
    nullptr, 0,
    nullptr, 0)

} // namespace Dia::Sensor
