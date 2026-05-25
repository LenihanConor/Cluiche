#pragma once
#include <cstdint>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaEntity/IComponent.h>

namespace Dia::Entity {

    enum class FieldKind : uint8_t {
        Primitive,
        StringId,
        Math,
        AssetHandle,
        EntityRef,
        Nested,
        Container,
    };

    struct FieldDesc {
        const char* name;
        uint32_t    nameCrc;
        uint16_t    offset;
        FieldKind   kind;
        bool        required;
    };

    // Function pointer types for type-erased component operations.
    using LoadFromJsonFn = void (*)(IComponent* dst, const Json::Value& config);
    using SaveToJsonFn   = void (*)(const IComponent* src, Json::Value& outConfig);

    inline constexpr uint16_t kFlagOverridesDoUpdate = 1u << 0;

    struct ComponentTypeDesc {
        Dia::Core::StringCRC typeId;
        const char*          debugName;
        uint16_t             size;
        uint16_t             alignment;
        uint16_t             schemaVersion;
        uint16_t             flags;

        const FieldDesc*            fields;
        uint16_t                    fieldCount;

        const Dia::Core::StringCRC* requires_;
        uint16_t                    requiresCount;

        LoadFromJsonFn loadFromJson;
        SaveToJsonFn   saveToJson;
    };

} // namespace Dia::Entity
