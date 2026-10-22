#pragma once
#include <DiaCore/Json/external/json/json.h>
#include <DiaEntity/ComponentTypeDesc.h>

namespace Dia::Entity {

    // Describes one component field for tooling: UI metadata (from DescribeAttribute),
    // range (from RangeAttribute) and the default value (from a constructed instance).
    // String pointers are literals / registry-owned and outlive the call.
    struct ComponentFieldInfo {
        const char* typeId        = nullptr;   // "foo-component"
        const char* name          = nullptr;   // member name (FieldDesc.name)
        const char* label         = nullptr;   // describe label, else the member name
        const char* description   = nullptr;   // or null
        const char* unit          = nullptr;   // or null
        const char* hint          = nullptr;   // or null
        FieldKind   kind          = FieldKind::Primitive;
        bool        hasRange      = false;
        double      min           = 0.0;
        double      max           = 0.0;
        bool        hasDefault    = false;
        Json::Value defaultValue;
        bool        described     = false;     // a DescribeAttribute exists
    };

    // Fills out[0..n) with one item per FieldDesc (nested fields are one item); returns n <= maxOut.
    // Attributes are joined by (CRC(desc.debugName), field.nameCrc).
    // Defaults come from serialising a default-constructed instance (needs desc.constructDefault).
    unsigned DescribeComponentFields(const ComponentTypeDesc& desc, ComponentFieldInfo* out, unsigned maxOut);

} // namespace Dia::Entity
