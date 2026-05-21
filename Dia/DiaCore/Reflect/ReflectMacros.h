#pragma once
#include "DiaCore/Reflect/Archive.h"

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
    void serialize(Archive& _ar_, Type& obj, unsigned _version_ = (Version)) { \
        static_assert(Dia::Reflect::Archive<Archive>, #Type ": Archive type does not satisfy Dia::Reflect::Archive concept"); \
        (void)_version_;

// Close the serialize function
#define DIA_SERIALIZE_END \
    }

// Serialize a regular value field (optional -- keeps default if missing)
#define DIA_FIELD(member) \
    _ar_ & Dia::Reflect::named(#member, obj.member);

// Serialize a required value field (archive will error if missing during read)
#define DIA_FIELD_REQUIRED(member) \
    _ar_ & Dia::Reflect::named(#member, obj.member).Required();

// Serialize an owning pointer field (object embedded inline)
#define DIA_FIELD_OWNED_PTR(member) \
    _ar_ & Dia::Reflect::owned(#member, obj.member);

// Serialize a non-owning reference as an ID only
#define DIA_FIELD_REF_ID(member) \
    _ar_ & Dia::Reflect::refId(#member, obj.member);

// Serialize a field with a custom name string (use when member name differs from desired JSON key)
#define DIA_FIELD_NAMED(customName, member) \
    _ar_ & Dia::Reflect::named(customName, obj.member);

// Serialize the base class fields first (calls base's serialize function)
#define DIA_BASE(BaseType) \
    serialize(_ar_, static_cast<BaseType&>(obj), _version_);
