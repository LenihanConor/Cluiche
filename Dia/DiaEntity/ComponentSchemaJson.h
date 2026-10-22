#pragma once
#include <DiaCore/Json/external/json/json.h>
#include <DiaEntity/ComponentTypeDesc.h>

namespace Dia::Entity {

    // Lowercase JSON name of a field kind: "primitive", "string_id", "math", "asset_handle",
    // "entity_ref", "nested", "container", else "unknown".
    const char* FieldKindToString(FieldKind kind);

    // Builds one component entry of registeredtypes.diaschema:
    // type_id, debug_name, description, fields[] and default_values.
    // Field entries always carry name and kind; label, description, unit, hint, min, max and
    // default are added only when present. default_values come from a constructed default
    // instance when available, else from a zero-filled buffer via saveToJson, else per-kind zeros.
    Json::Value BuildComponentSchemaJson(const ComponentTypeDesc& desc);

} // namespace Dia::Entity
