#include <DiaEntity/ComponentDescribe.h>
#include <DiaCore/Reflect/FieldAttributes.h>
#include <malloc.h>
#include <new>

namespace Dia::Entity {

    namespace {

        // Constructs a default instance, saves it to JSON, destroys it. Returns false if unavailable.
        bool BuildDefaultJson(const ComponentTypeDesc& desc, Json::Value& outJson) {
            if (desc.constructDefault == nullptr || desc.saveToJson == nullptr) {
                return false;
            }

            alignas(16) char stackBuf[256];
            void* mem = stackBuf;
            bool heap = false;
            if (desc.size > sizeof(stackBuf) || desc.alignment > 16u) {
                mem = _aligned_malloc(desc.size, desc.alignment > 0u ? desc.alignment : 16u);
                if (mem == nullptr) {
                    return false;
                }
                heap = true;
            }

            desc.constructDefault(mem);
            IComponent* comp = static_cast<IComponent*>(mem);
            desc.saveToJson(comp, outJson);
            comp->~IComponent();

            if (heap) {
                _aligned_free(mem);
            }
            return true;
        }

    } // namespace

    unsigned DescribeComponentFields(const ComponentTypeDesc& desc, ComponentFieldInfo* out, unsigned maxOut) {
        if (out == nullptr || desc.fields == nullptr) {
            return 0u;
        }

        Json::Value defaults;
        const bool haveDefaults = BuildDefaultJson(desc, defaults) && defaults.isObject();

        const uint32_t typeCrc = Dia::Core::StringCRC(desc.debugName).Value();
        const Dia::Reflect::FieldAttributeRegistry& registry = Dia::Reflect::FieldAttributeRegistry::Instance();

        unsigned count = desc.fieldCount < maxOut ? desc.fieldCount : maxOut;
        for (unsigned i = 0u; i < count; ++i) {
            const FieldDesc& field = desc.fields[i];
            ComponentFieldInfo& info = out[i];

            info = ComponentFieldInfo();
            info.typeId = desc.typeId.AsChar();
            info.name   = field.name;
            info.label  = field.name;
            info.kind   = field.kind;

            const Dia::Reflect::DescribeAttribute* describe =
                registry.FindAttribute<Dia::Reflect::DescribeAttribute>(typeCrc, field.nameCrc);
            if (describe != nullptr) {
                info.described = true;
                if (describe->label != nullptr && describe->label[0] != '\0') {
                    info.label = describe->label;
                }
                info.description = describe->description;
                info.unit        = (describe->unit != nullptr && describe->unit[0] != '\0') ? describe->unit : nullptr;
                info.hint        = describe->hint;
            }

            // Nested fields are described as one item: label and description only, no range.
            if (field.kind != FieldKind::Nested) {
                const Dia::Reflect::FieldAttribute* attrs[Dia::Reflect::FieldAttributeList::kMaxAttrsPerField];
                const unsigned found = registry.GetAttributes(typeCrc, field.nameCrc, attrs,
                    Dia::Reflect::FieldAttributeList::kMaxAttrsPerField);
                const unsigned n = found < Dia::Reflect::FieldAttributeList::kMaxAttrsPerField
                    ? found : Dia::Reflect::FieldAttributeList::kMaxAttrsPerField;
                for (unsigned j = 0u; j < n; ++j) {
                    double lo = 0.0, hi = 0.0;
                    if (attrs[j]->GetRange(lo, hi)) {
                        info.hasRange = true;
                        info.min = lo;
                        info.max = hi;
                        break;
                    }
                }
            }

            if (haveDefaults && defaults.isMember(field.name)) {
                info.hasDefault   = true;
                info.defaultValue = defaults[field.name];
            }
        }
        return count;
    }

} // namespace Dia::Entity
