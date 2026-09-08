#pragma once
// DiaReflect — field attribute metadata
// Runtime-queryable metadata attached to individual fields.
// Attributes are declared separately from DIA_SERIALIZE via DIA_ATTR_* macros.
// See docs/specs/systems/dia/diareflect.md
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Reflect/SerializeResult.h"
#include <cstring>
#include <type_traits>

namespace Dia::Reflect {

// =============================================================================
// FieldAttribute base — all attribute types derive from this
// =============================================================================
struct FieldAttribute {
    virtual ~FieldAttribute() = default;
    virtual const char* GetKind() const = 0;
};

// =============================================================================
// RequiredAttribute — field must be present in source data
// =============================================================================
struct RequiredAttribute : FieldAttribute {
    static constexpr const char* kKind = "Required";
    const char* GetKind() const override { return kKind; }
};

// =============================================================================
// RangeAttribute<T> — valid range for numeric fields
// clamp=false (default): record error when out-of-range
// clamp=true:            silently clamp to [minValue, maxValue]
// =============================================================================
template<typename T>
struct RangeAttribute : FieldAttribute {
    static constexpr const char* kKind = "Range";
    T    minValue{};
    T    maxValue{};
    bool clamp = false;

    RangeAttribute() = default;
    RangeAttribute(T mn, T mx, bool cl = false) : minValue(mn), maxValue(mx), clamp(cl) {}

    const char* GetKind() const override { return kKind; }
};

// =============================================================================
// AssetRefAttribute — marks a field as a reference to an asset of a given type
// =============================================================================
struct AssetRefAttribute : FieldAttribute {
    static constexpr const char* kKind = "AssetRef";
    Dia::Core::StringCRC targetTypeId;

    AssetRefAttribute() = default;
    explicit AssetRefAttribute(const char* typeName) : targetTypeId(typeName) {}

    const char* GetKind() const override { return kKind; }
};

// =============================================================================
// FieldAttributeList — all attributes registered for one (type, field) pair
// =============================================================================
struct FieldKey {
    uint32_t typeCrc;
    uint32_t fieldCrc;

    bool operator==(const FieldKey& other) const {
        return typeCrc == other.typeCrc && fieldCrc == other.fieldCrc;
    }
};

struct FieldAttributeList {
    static constexpr unsigned int kMaxAttrsPerField = 8u;

    FieldKey             key{};
    const FieldAttribute* attrs[kMaxAttrsPerField]{};
    unsigned int         count = 0u;

    bool IsFull() const { return count >= kMaxAttrsPerField; }

    void Add(const FieldAttribute* attr) {
        if (!IsFull()) {
            attrs[count++] = attr;
        }
    }
};

// =============================================================================
// FieldAttributeRegistry — singleton mapping (typeCrc, fieldCrc) → attributes
// =============================================================================
class FieldAttributeRegistry {
public:
    static constexpr unsigned int kMaxEntries = 256u;

    static FieldAttributeRegistry& Instance();

    // Register an attribute for a (type, field) pair.
    // Safe to call at static-init time.
    void Register(uint32_t typeCrc, uint32_t fieldCrc, const FieldAttribute* attr) {
        // Find existing entry for this key
        for (unsigned int i = 0u; i < mEntries.Size(); ++i) {
            if (mEntries.At(i).key.typeCrc == typeCrc &&
                mEntries.At(i).key.fieldCrc == fieldCrc) {
                mEntries.At(i).Add(attr);
                return;
            }
        }
        // New entry
        if (!mEntries.IsFull()) {
            FieldAttributeList list;
            list.key.typeCrc  = typeCrc;
            list.key.fieldCrc = fieldCrc;
            list.Add(attr);
            mEntries.Add(list);
        }
    }

    // Returns all attributes for a (type, field) pair.
    // outAttrs is filled with up to maxOut pointers; returns actual count found.
    unsigned int GetAttributes(uint32_t typeCrc, uint32_t fieldCrc,
                               const FieldAttribute** outAttrs,
                               unsigned int maxOut) const {
        for (unsigned int i = 0u; i < mEntries.Size(); ++i) {
            const FieldAttributeList& list = mEntries.At(i);
            if (list.key.typeCrc == typeCrc && list.key.fieldCrc == fieldCrc) {
                unsigned int n = list.count < maxOut ? list.count : maxOut;
                for (unsigned int j = 0u; j < n; ++j) {
                    outAttrs[j] = list.attrs[j];
                }
                return list.count;
            }
        }
        return 0u;
    }

    // Find the first attribute of type AttrT for a (type, field) pair.
    // Matches via AttrT::kKind string comparison (no RTTI required).
    template<typename AttrT>
    const AttrT* FindAttribute(uint32_t typeCrc, uint32_t fieldCrc) const {
        for (unsigned int i = 0u; i < mEntries.Size(); ++i) {
            const FieldAttributeList& list = mEntries.At(i);
            if (list.key.typeCrc == typeCrc && list.key.fieldCrc == fieldCrc) {
                for (unsigned int j = 0u; j < list.count; ++j) {
                    if (strcmp(list.attrs[j]->GetKind(), AttrT::kKind) == 0) {
                        return static_cast<const AttrT*>(list.attrs[j]);
                    }
                }
                return nullptr;
            }
        }
        return nullptr;
    }

    // For testing: clear all entries
    void Clear() { mEntries.RemoveAll(); }

private:
    FieldAttributeRegistry() = default;
    Dia::Core::Containers::DynamicArrayC<FieldAttributeList, kMaxEntries> mEntries;
};

// =============================================================================
// EnforceRange — called by DIA_FIELD_RANGED after reading a field value.
// Looks up RangeAttribute for (typeCrc, fieldCrc). If found:
//   clamp=true  → clamp value to [min, max]
//   clamp=false → report RangeViolation error via archive
// =============================================================================
template<typename T, typename Archive>
void EnforceRange(T& value, uint32_t typeCrc, uint32_t fieldCrc, Archive& ar) {
    if constexpr (std::is_arithmetic_v<T>) {
        auto* rangeAttr = FieldAttributeRegistry::Instance()
            .FindAttribute<RangeAttribute<T>>(typeCrc, fieldCrc);
        if (rangeAttr) {
            if (value < rangeAttr->minValue) {
                if (rangeAttr->clamp) {
                    value = rangeAttr->minValue;
                } else if constexpr (requires { ar.AddError(SerializeErrorKind{}, Dia::Core::StringCRC{}, ""); }) {
                    ar.AddError(SerializeErrorKind::RangeViolation,
                        Dia::Core::StringCRC(""),
                        "Value below minimum range");
                }
            } else if (value > rangeAttr->maxValue) {
                if (rangeAttr->clamp) {
                    value = rangeAttr->maxValue;
                } else if constexpr (requires { ar.AddError(SerializeErrorKind{}, Dia::Core::StringCRC{}, ""); }) {
                    ar.AddError(SerializeErrorKind::RangeViolation,
                        Dia::Core::StringCRC(""),
                        "Value above maximum range");
                }
            }
        }
    }
}

} // namespace Dia::Reflect

// =============================================================================
// DIA_ATTR_* macros — static-init registration of field attributes
//
// Usage (at file scope, outside any function):
//
//   struct RigidBody { float mMass = 1.0f; int mTextureId = 0; };
//   DIA_SERIALIZE(RigidBody, 1)
//       DIA_FIELD(mMass)
//       DIA_FIELD(mTextureId)
//   DIA_SERIALIZE_END
//
//   DIA_ATTR_REQUIRED(RigidBody, mMass)
//   DIA_ATTR_RANGE(RigidBody, mMass, 0.0f, 1000.0f)
//   DIA_ATTR_ASSET_REF(RigidBody, mTextureId, DiaTexture)
//
// Each macro expands to an anonymous-namespace struct whose constructor
// runs at static init time, registering a static attribute instance.
// =============================================================================

// DIA_ATTR_REQUIRED — register a RequiredAttribute for (TypeName, fieldName)
#define DIA_ATTR_REQUIRED(TypeName, fieldName) \
    namespace { \
    struct TypeName##_##fieldName##_RequiredReg { \
        TypeName##_##fieldName##_RequiredReg() { \
            static Dia::Reflect::RequiredAttribute _attr_; \
            Dia::Reflect::FieldAttributeRegistry::Instance().Register( \
                Dia::Core::StringCRC(#TypeName).Value(), \
                Dia::Core::StringCRC(#fieldName).Value(), \
                &_attr_); \
        } \
    }; \
    static TypeName##_##fieldName##_RequiredReg s_##TypeName##_##fieldName##_RequiredReg; \
    }

// DIA_ATTR_RANGE — register a RangeAttribute<T> for (TypeName, fieldName)
// T is deduced from the field type.
#define DIA_ATTR_RANGE(TypeName, fieldName, MinVal, MaxVal) \
    namespace { \
    struct TypeName##_##fieldName##_RangeReg { \
        TypeName##_##fieldName##_RangeReg() { \
            using _ElemT_ = std::remove_reference_t<decltype(std::declval<TypeName>().fieldName)>; \
            static Dia::Reflect::RangeAttribute<_ElemT_> _attr_{(MinVal), (MaxVal)}; \
            Dia::Reflect::FieldAttributeRegistry::Instance().Register( \
                Dia::Core::StringCRC(#TypeName).Value(), \
                Dia::Core::StringCRC(#fieldName).Value(), \
                &_attr_); \
        } \
    }; \
    static TypeName##_##fieldName##_RangeReg s_##TypeName##_##fieldName##_RangeReg; \
    }

// DIA_ATTR_RANGE_CLAMPED — register a RangeAttribute<T> with clamp=true
#define DIA_ATTR_RANGE_CLAMPED(TypeName, fieldName, MinVal, MaxVal) \
    namespace { \
    struct TypeName##_##fieldName##_RangeClampReg { \
        TypeName##_##fieldName##_RangeClampReg() { \
            using _ElemT_ = std::remove_reference_t<decltype(std::declval<TypeName>().fieldName)>; \
            static Dia::Reflect::RangeAttribute<_ElemT_> _attr_{(MinVal), (MaxVal), true}; \
            Dia::Reflect::FieldAttributeRegistry::Instance().Register( \
                Dia::Core::StringCRC(#TypeName).Value(), \
                Dia::Core::StringCRC(#fieldName).Value(), \
                &_attr_); \
        } \
    }; \
    static TypeName##_##fieldName##_RangeClampReg s_##TypeName##_##fieldName##_RangeClampReg; \
    }

// DIA_ATTR_ASSET_REF — register an AssetRefAttribute for (TypeName, fieldName)
// TargetTypeName is the name of the asset type this field references.
#define DIA_ATTR_ASSET_REF(TypeName, fieldName, TargetTypeName) \
    namespace { \
    struct TypeName##_##fieldName##_AssetRefReg { \
        TypeName##_##fieldName##_AssetRefReg() { \
            static Dia::Reflect::AssetRefAttribute _attr_{#TargetTypeName}; \
            Dia::Reflect::FieldAttributeRegistry::Instance().Register( \
                Dia::Core::StringCRC(#TypeName).Value(), \
                Dia::Core::StringCRC(#fieldName).Value(), \
                &_attr_); \
        } \
    }; \
    static TypeName##_##fieldName##_AssetRefReg s_##TypeName##_##fieldName##_AssetRefReg; \
    }
