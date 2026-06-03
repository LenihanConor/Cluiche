#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Entity::DebugDataType {

    // Data type CRCs for the entity.inspect WebSocket topic.
    // These are stable identifiers — matched by CRC value, not string, at runtime.
    static const Dia::Core::StringCRC kEntityInspect("entity.inspect");
    static const Dia::Core::StringCRC kEntityInspectRequest("entity.inspect_request");
    static const Dia::Core::StringCRC kEntityFindByName("entity.find_by_name");
    static const Dia::Core::StringCRC kEntityWriteField("entity.write_field");

} // namespace Dia::Entity::DebugDataType
