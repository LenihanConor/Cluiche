#include <DiaEntity/ComponentSchemaJson.h>
#include <DiaEntity/ComponentDescribe.h>
#include <DiaCore/Metadata/DescriptionRegistry.h>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <malloc.h>

namespace Dia::Entity {

    const char* FieldKindToString(FieldKind kind) {
        switch (kind) {
        case FieldKind::Primitive:   return "primitive";
        case FieldKind::StringId:    return "string_id";
        case FieldKind::Math:        return "math";
        case FieldKind::AssetHandle: return "asset_handle";
        case FieldKind::EntityRef:   return "entity_ref";
        case FieldKind::Nested:      return "nested";
        case FieldKind::Container:   return "container";
        default:                     return "unknown";
        }
    }

    namespace {

        void FillZeroDefaults(const ComponentTypeDesc& desc, Json::Value& out) {
            for (uint16_t f = 0; f < desc.fieldCount; ++f) {
                const FieldDesc& fd = desc.fields[f];
                const char* fieldName = fd.name ? fd.name : "";
                switch (fd.kind) {
                case FieldKind::StringId:
                case FieldKind::AssetHandle:
                case FieldKind::EntityRef:
                    out[fieldName] = Json::Value("");
                    break;
                case FieldKind::Nested:
                    out[fieldName] = Json::Value(Json::objectValue);
                    break;
                case FieldKind::Container:
                    out[fieldName] = Json::Value(Json::arrayValue);
                    break;
                case FieldKind::Primitive:
                case FieldKind::Math:
                default:
                    out[fieldName] = Json::Value(Json::Int(0));
                    break;
                }
            }
        }

        Json::Value BuildDefaultValues(const ComponentTypeDesc& desc) {
            Json::Value result(Json::objectValue);
            if (BuildComponentDefaultJson(desc, result)) {
                return result;
            }

            if (desc.saveToJson != nullptr) {
                const size_t alignment = (desc.alignment > 0)
                    ? static_cast<size_t>(desc.alignment)
                    : alignof(std::max_align_t);
                void* buf = _aligned_malloc(static_cast<size_t>(desc.size), alignment);
                if (buf) {
                    memset(buf, 0, static_cast<size_t>(desc.size));
                    desc.saveToJson(static_cast<const IComponent*>(buf), result);
                    _aligned_free(buf);
                }
            } else {
                fprintf(stderr,
                    "reflect: WARNING: %s has no saveToJson - default_values will be zero-filled\n",
                    desc.debugName ? desc.debugName : "unknown");
                FillZeroDefaults(desc, result);
            }
            return result;
        }

    } // namespace

    Json::Value BuildComponentSchemaJson(const ComponentTypeDesc& desc) {
        Json::Value comp(Json::objectValue);

        const char* typeIdStr = desc.typeId.AsChar();
        if (typeIdStr && typeIdStr[0] != '\0') {
            comp["type_id"] = typeIdStr;
        } else {
            char hexBuf[20];
            snprintf(hexBuf, sizeof(hexBuf), "0x%08X", desc.typeId.Value());
            comp["type_id"] = hexBuf;
        }

        comp["debug_name"] = desc.debugName ? desc.debugName : "";

        const char* compDesc = Dia::Core::Metadata::DescriptionRegistry::Get(desc.typeId);
        if (compDesc == nullptr) {
            compDesc = desc.description;
        }
        comp["description"] = compDesc ? compDesc : "";

        // Use the shared builder for the extra keys; heap array sized to the field count.
        const unsigned fieldCount = desc.fieldCount;
        ComponentFieldInfo* infos = nullptr;
        unsigned described = 0u;
        if (fieldCount > 0u && desc.fields != nullptr) {
            infos = new ComponentFieldInfo[fieldCount];
            described = DescribeComponentFields(desc, infos, fieldCount);
        }

        Json::Value fields(Json::arrayValue);
        for (uint16_t f = 0; f < desc.fieldCount; ++f) {
            const FieldDesc& fd = desc.fields[f];
            Json::Value field(Json::objectValue);
            field["name"] = fd.name ? fd.name : "";
            field["kind"] = FieldKindToString(fd.kind);

            if (f < described) {
                const ComponentFieldInfo& info = infos[f];
                if (info.described && info.label != nullptr) {
                    field["label"] = info.label;
                }
                if (info.description != nullptr) {
                    field["description"] = info.description;
                }
                if (info.unit != nullptr) {
                    field["unit"] = info.unit;
                }
                if (info.hint != nullptr) {
                    field["hint"] = info.hint;
                }
                if (info.hasRange) {
                    field["min"] = info.min;
                    field["max"] = info.max;
                }
                if (info.hasDefault) {
                    field["default"] = info.defaultValue;
                }
            }
            fields.append(field);
        }
        delete[] infos;
        comp["fields"] = fields;

        comp["default_values"] = BuildDefaultValues(desc);
        return comp;
    }

} // namespace Dia::Entity
