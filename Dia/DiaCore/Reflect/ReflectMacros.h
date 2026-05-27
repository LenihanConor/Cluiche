#pragma once
#include "DiaCore/Reflect/Archive.h"
#include "DiaCore/Reflect/PolymorphicRegistry.h"
#include "DiaCore/Reflect/FieldAttributes.h"

// =============================================================================
// DiaReflect Macro DSL
// Each macro expands to part of a serialize() free function body.
// Usage:
//
//   DIA_SERIALIZE(MyType, 1)
//       DIA_FIELD(mPosition)
//       DIA_FIELD_REQUIRED(mName)
//       DIA_FIELD_OWNED_PTR(mShape)
//       DIA_FIELD_REF_ID(mOwnerId)
//   DIA_SERIALIZE_END
//
//   // With inheritance:
//   DIA_SERIALIZE(DerivedType, 2)
//       DIA_BASE(BaseType)
//       DIA_FIELD(mExtra)
//   DIA_SERIALIZE_END
// =============================================================================

// Open the serialize function for Type at given version
#define DIA_SERIALIZE(Type, Version) \
    template<class Archive> \
    void serialize(Archive& _ar_, Type& obj, unsigned _version_) { \
        static_assert(Dia::Reflect::Archive<Archive>, #Type ": Archive type does not satisfy Dia::Reflect::Archive concept"); \
        (void)_version_; \
        static const uint32_t _typeCrc_ = Dia::Core::StringCRC(#Type).Value(); (void)_typeCrc_;

// Close the serialize function
#define DIA_SERIALIZE_END \
    }

// Open the serialize function for a polymorphic concrete type.
// Registers ConcreteType in PolymorphicRegistry AND opens serialize() function body.
// Must be closed with DIA_SERIALIZE_END.
// Strategy: define serialize() first, then the static registrar references it.
// We use a two-part macro approach: DIA_SERIALIZE_POLYMORPHIC opens the function,
// DIA_SERIALIZE_END closes it, then DIA_REGISTER_POLYMORPHIC registers the type.
// BUT to keep the simple one-macro UX, we forward-declare serialize and define
// the registrar in a struct whose constructor is only called at static init time
// (after the TU is fully compiled, so serialize is available).
#define DIA_SERIALIZE_POLYMORPHIC(ConcreteType, BaseType, Version) \
    template<class Archive> \
    void serialize(Archive& _ar_, ConcreteType& obj, unsigned _version_); \
    namespace { \
    struct ConcreteType##_PolyRegistrar { \
        ConcreteType##_PolyRegistrar() { \
            Dia::Reflect::PolymorphicEntry entry{}; \
            entry.typeCrc    = Dia::Core::StringCRC(#ConcreteType).Value(); \
            entry.typeName   = #ConcreteType; \
            entry.factory    = []() -> void* { return new ConcreteType(); }; \
            entry.writeJson  = [](void* obj, Dia::Reflect::JsonWriteArchive& ar) { serialize(ar, *static_cast<ConcreteType*>(obj), (Version)); }; \
            entry.readJson   = [](void* obj, Dia::Reflect::JsonReadArchive& ar)  { serialize(ar, *static_cast<ConcreteType*>(obj), (Version)); }; \
            entry.writeBinary= [](void* obj, Dia::Reflect::BinaryWriteArchive& ar) { serialize(ar, *static_cast<ConcreteType*>(obj), (Version)); }; \
            entry.readBinary = [](void* obj, Dia::Reflect::BinaryReadArchive& ar)  { serialize(ar, *static_cast<ConcreteType*>(obj), (Version)); }; \
            Dia::Reflect::PolymorphicRegistry::Instance().Register(entry); \
        } \
    }; \
    static ConcreteType##_PolyRegistrar s_##ConcreteType##_Reg; \
    } \
    template<class Archive> \
    void serialize(Archive& _ar_, ConcreteType& obj, unsigned _version_) { \
        static_assert(Dia::Reflect::Archive<Archive>, #ConcreteType ": Archive type does not satisfy Dia::Reflect::Archive concept"); \
        (void)_version_;

// Serialize a regular value field (optional -- keeps default if missing)
#define DIA_FIELD(member) \
    _ar_ & Dia::Reflect::named(#member, obj.member);

// Serialize a required value field (archive will error if missing during read)
#define DIA_FIELD_REQUIRED(member) \
    _ar_ & Dia::Reflect::named(#member, obj.member).Required();

// Serialize a field with range enforcement (clamp or error based on RangeAttribute registration)
#define DIA_FIELD_RANGED(member) \
    _ar_ & Dia::Reflect::named(#member, obj.member); \
    if (_ar_.IsReading()) { Dia::Reflect::EnforceRange(obj.member, _typeCrc_, Dia::Core::StringCRC(#member).Value(), _ar_); }

// Serialize an owning pointer field (object embedded inline)
#define DIA_FIELD_OWNED_PTR(member) \
    _ar_ & Dia::Reflect::owned(#member, obj.member);

// Serialize a non-owning reference as an ID only
#define DIA_FIELD_REF_ID(member) \
    _ar_ & Dia::Reflect::refId(#member, obj.member);

// Serialize a field with a custom name string (use when member name differs from desired JSON key)
#define DIA_FIELD_NAMED(customName, member) \
    _ar_ & Dia::Reflect::named(customName, obj.member);

// Serialize an owning polymorphic pointer — writes/reads "_type" tag + concrete fields
// On write: looks up ConcreteType in registry (compile-time known), writes "_type" + fields
// On read: reads "_type" from JSON/4-byte CRC from binary, looks up registry, allocates, reads
#define DIA_FIELD_POLY_OWNED_PTR(member, ConcreteType) \
    _ar_ & Dia::Reflect::poly_owned(#member, obj.member, Dia::Core::StringCRC(#ConcreteType).Value(), #ConcreteType);

// Serialize the base class fields first (calls base's serialize function)
#define DIA_BASE(BaseType) \
    serialize(_ar_, static_cast<BaseType&>(obj), _version_);
