#include <DiaSensor/SensorBlackboardAdapter.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Reflect/ReflectMacros.h>

// Serialize free function — no fields on the abstract base.
DIA_SERIALIZE(Dia::Sensor::SensorBlackboardAdapter, Dia::Sensor::SensorBlackboardAdapter::kVersion)
DIA_SERIALIZE_END

namespace Dia::Sensor {

// ---------------------------------------------------------------------------
// kTypeId definition — required because DIA_COMPONENT declares it static const.
// DIA_COMPONENT_REGISTER is NOT called for this abstract base (it cannot be
// instantiated). Only kTypeId needs a translation unit to live in.
// ---------------------------------------------------------------------------
const Dia::Core::StringCRC SensorBlackboardAdapter::kTypeId("sensor-blackboard-adapter");

// GetDesc stub — returns a minimal descriptor used for pool-table lookup.
// Abstract components are never constructed directly, so thunks are nullptr.
const Dia::Entity::ComponentTypeDesc& SensorBlackboardAdapter::GetDesc()
{
    static const Dia::Entity::ComponentTypeDesc sDesc {
        /* typeId         */ SensorBlackboardAdapter::kTypeId,
        /* debugName      */ "SensorBlackboardAdapter",
        /* size           */ 0u,
        /* alignment      */ 0u,
        /* schemaVersion  */ SensorBlackboardAdapter::kVersion,
        /* flags          */ 0u,
        /* fields         */ nullptr,
        /* fieldCount     */ 0u,
        /* requires_      */ nullptr,
        /* requiresCount  */ 0u,
        /* writesTo       */ nullptr,
        /* writesToCount  */ 0u,
        /* loadFromJson   */ nullptr,
        /* saveToJson     */ nullptr
    };
    return sDesc;
}

} // namespace Dia::Sensor
