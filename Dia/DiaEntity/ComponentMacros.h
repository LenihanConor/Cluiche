#pragma once
#include <cstdint>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Reflect/ReflectMacros.h>
#include <DiaCore/Reflect/JsonArchive.h>
#include <DiaEntity/ComponentTypeDesc.h>
#include <DiaEntity/ComponentRegistry.h>

// =============================================================================
// Field-kind deduction helpers — add specialisations for new primitive types.
// =============================================================================
namespace Dia::Entity::detail {
    template<class T> struct FieldKindOf {
        static constexpr FieldKind value = FieldKind::Nested;
    };
    template<> struct FieldKindOf<bool>                 { static constexpr FieldKind value = FieldKind::Primitive; };
    template<> struct FieldKindOf<int32_t>              { static constexpr FieldKind value = FieldKind::Primitive; };
    template<> struct FieldKindOf<uint32_t>             { static constexpr FieldKind value = FieldKind::Primitive; };
    template<> struct FieldKindOf<float>                { static constexpr FieldKind value = FieldKind::Primitive; };
    template<> struct FieldKindOf<double>               { static constexpr FieldKind value = FieldKind::Primitive; };
    template<> struct FieldKindOf<int16_t>              { static constexpr FieldKind value = FieldKind::Primitive; };
    template<> struct FieldKindOf<uint16_t>             { static constexpr FieldKind value = FieldKind::Primitive; };
    template<> struct FieldKindOf<int8_t>               { static constexpr FieldKind value = FieldKind::Primitive; };
    template<> struct FieldKindOf<uint8_t>              { static constexpr FieldKind value = FieldKind::Primitive; };
    template<> struct FieldKindOf<Dia::Core::StringCRC> { static constexpr FieldKind value = FieldKind::StringId; };
} // namespace Dia::Entity::detail

// =============================================================================
// DIA_COMPONENT(ClassName, "string-name", SchemaVersion)
//
// Place inside the class body of any IComponent-derived class.
// Emits:
//   - kTypeId     : static const StringCRC (defined in DIA_COMPONENT_REGISTER)
//   - kVersion    : static constexpr uint16_t
//   - GetTypeId() : virtual override
//   - GetStaticTypeId() : static method (used by Domain templates)
//   - GetDesc()   : static method (defined in DIA_COMPONENT_REGISTER)
//
// NOTE: StringCRC is not a literal/constexpr type, so kTypeId cannot be
// constexpr. It is declared `static const` here and defined out-of-class
// inside DIA_COMPONENT_REGISTER (one .cpp per component).
// =============================================================================
#define DIA_COMPONENT(ClassName, StringName, SchemaVersion)                     \
public:                                                                          \
    static const Dia::Core::StringCRC     kTypeId;                               \
    static constexpr uint16_t             kVersion = (SchemaVersion);            \
    Dia::Core::StringCRC GetTypeId() const override { return kTypeId; }          \
    static Dia::Core::StringCRC GetStaticTypeId()   { return kTypeId; }          \
    static const Dia::Entity::ComponentTypeDesc& GetDesc();                       \
private:

// =============================================================================
// FIELD(type, name, defaultVal)
//
// Declares a member field with a default value. In the class body this only
// declares the member — reflection metadata is provided in DIA_COMPONENT_REGISTER.
// The member is placed in public scope so offsetof() works from the .cpp.
// =============================================================================
#define FIELD(type, name, defaultVal) \
public: type name = (defaultVal); private:

// =============================================================================
// REQUIRES(OtherComponent)
//
// Documentation-only marker in the class body. Register actual dependencies
// using DIA_REQ_ENTRY inside the DIA_COMPONENT_REGISTER block in the .cpp.
// =============================================================================
#define REQUIRES(OtherComponent)  /* declare dependency via DIA_REQ_ENTRY in DIA_COMPONENT_REGISTER */

// =============================================================================
// DIA_FIELD_ENTRY(type, member, ClassName)
//
// Generates a FieldDesc initialiser for a single component field.
// Use inside a static FieldDesc array initialisation block.
//
// Example:
//   static Dia::Entity::FieldDesc s_MyComp_fields[] = {
//       DIA_FIELD_ENTRY(float,   speed,     MyComponent)
//       DIA_FIELD_ENTRY(int32_t, hitPoints, MyComponent)
//   };
// =============================================================================
#define DIA_FIELD_ENTRY(type, member, ClassName)                     \
    {                                                                 \
        #member,                                                      \
        Dia::Core::StringCRC(#member).Value(),                        \
        static_cast<uint16_t>(offsetof(ClassName, member)),           \
        Dia::Entity::detail::FieldKindOf<type>::value,                \
        false                                                         \
    },

// =============================================================================
// DIA_REQ_ENTRY(OtherComponent)
//
// Generates a StringCRC entry in the requirements array.
// Use inside a static StringCRC array initialisation block.
//
// Example:
//   static Dia::Core::StringCRC s_MyComp_reqs[] = {
//       DIA_REQ_ENTRY(TransformComponent)
//   };
// =============================================================================
#define DIA_REQ_ENTRY(OtherComponent) \
    OtherComponent::kTypeId,

// =============================================================================
// DIA_ARRAY_COUNT(arr)
//
// Safe compile-time array count. Use to pass fieldCount/requiresCount.
// =============================================================================
#define DIA_ARRAY_COUNT(arr) \
    (static_cast<uint16_t>(sizeof(arr) / sizeof((arr)[0])))

// =============================================================================
// DIA_COMPONENT_REGISTER(ClassName, StringName, IsUpdatable,
//                         FieldsPtr, FieldsCount, ReqsPtr, ReqsCount)
//
// Place in exactly one .cpp per component. Implements:
//   - Out-of-class definition of kTypeId (using the same StringName passed to
//     DIA_COMPONENT in the class body, so the CRC values match)
//   - GetDesc() static method (returns the ComponentTypeDesc singleton)
//   - Automatic registration at static-init time via a local registrar struct
//
// The serialize() free function (needed by loadFromJson/saveToJson thunks)
// MUST be defined in the same .cpp BEFORE this macro, using DIA_SERIALIZE.
//
// Full usage pattern in ComponentFoo.cpp:
//
//   // 1. Write the serialize free function.
//   DIA_SERIALIZE(ComponentFoo, ComponentFoo::kVersion)
//       DIA_FIELD(speed)
//       DIA_FIELD(tag)
//   DIA_SERIALIZE_END
//
//   // 2. Define field metadata (one DIA_FIELD_ENTRY per FIELD in the class).
//   //    For a component with no fields, skip this and pass nullptr, 0.
//   static Dia::Entity::FieldDesc s_ComponentFoo_fields[] = {
//       DIA_FIELD_ENTRY(float,               speed, ComponentFoo)
//       DIA_FIELD_ENTRY(Dia::Core::StringCRC, tag,  ComponentFoo)
//   };
//
//   // 3. Define required-component list (one per REQUIRES in the class).
//   //    For a component with no requirements, skip this and pass nullptr, 0.
//   static Dia::Core::StringCRC s_ComponentFoo_reqs[] = {
//       DIA_REQ_ENTRY(TransformComponent)
//   };
//
//   // 4. Register — defines kTypeId, GetDesc(), and auto-registers.
//   //    StringName MUST match the string passed to DIA_COMPONENT in the header.
//   DIA_COMPONENT_REGISTER(ComponentFoo, "foo-component", false,
//       s_ComponentFoo_fields, DIA_ARRAY_COUNT(s_ComponentFoo_fields),
//       nullptr, 0)
//
// Parameters:
//   ClassName   : the component class name (no quotes)
//   StringName  : the type-id string (quoted) — must match DIA_COMPONENT's StringName
//   IsUpdatable : bool literal — true if the component overrides DoUpdate
//   FieldsPtr   : pointer to FieldDesc array, or nullptr if none
//   FieldsCount : number of entries (use DIA_ARRAY_COUNT), or 0 if none
//   ReqsPtr     : pointer to StringCRC array of required type IDs, or nullptr
//   ReqsCount   : number of entries, or 0 if none
// =============================================================================
#define DIA_COMPONENT_REGISTER(ClassName, StringName, IsUpdatable, FieldsPtr, FieldsCount, ReqsPtr, ReqsCount) \
    \
    /* Out-of-class definition of the static kTypeId member.               \
       Must use the same string literal as DIA_COMPONENT(ClassName, StringName, ...) \
       so that CRC values are identical at all call sites. */              \
    const Dia::Core::StringCRC ClassName::kTypeId(StringName);              \
    \
    /* GetDesc() — lazy singleton, self-registers on first call. */        \
    const Dia::Entity::ComponentTypeDesc& ClassName::GetDesc() {           \
        static const Dia::Entity::ComponentTypeDesc sDesc {                \
            ClassName::kTypeId,                                            \
            #ClassName,                                                    \
            static_cast<uint16_t>(sizeof(ClassName)),                      \
            static_cast<uint16_t>(alignof(ClassName)),                     \
            ClassName::kVersion,                                           \
            (IsUpdatable)                                                  \
                ? Dia::Entity::kFlagOverridesDoUpdate                      \
                : static_cast<uint16_t>(0u),                               \
            (FieldsPtr),                                                   \
            (FieldsCount),                                                 \
            (ReqsPtr),                                                     \
            (ReqsCount),                                                   \
            /* loadFromJson thunk: reads config into a default-constructed component. */ \
            [](Dia::Entity::IComponent* dst, const Json::Value& cfg) {     \
                Dia::Reflect::JsonReadArchive ar(cfg);                     \
                serialize(ar, *static_cast<ClassName*>(dst), ClassName::kVersion); \
            },                                                             \
            /* saveToJson thunk: const_cast safe — archive only reads the object. */ \
            [](const Dia::Entity::IComponent* src, Json::Value& out) {    \
                Dia::Reflect::JsonWriteArchive ar;                         \
                serialize(ar,                                              \
                    *const_cast<ClassName*>(static_cast<const ClassName*>(src)), \
                    ClassName::kVersion);                                   \
                out = ar.GetRoot();                                        \
            }                                                              \
        };                                                                 \
        return sDesc;                                                      \
    }                                                                      \
    \
    /* Auto-registrar: ensures GetDesc() (and thus Register()) runs at    \
       static-init time even if GetDesc() is never called directly. */    \
    namespace {                                                            \
        struct ClassName##_AutoReg {                                       \
            ClassName##_AutoReg() {                                        \
                Dia::Entity::ComponentRegistry::Get().Register(            \
                    ClassName::GetDesc());                                  \
            }                                                              \
        };                                                                 \
        static ClassName##_AutoReg s_##ClassName##_reg;                    \
    }
