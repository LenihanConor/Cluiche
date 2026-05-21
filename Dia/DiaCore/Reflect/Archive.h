#pragma once
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia::Reflect {

// Forward declarations
template<typename T> struct NamedField;
template<typename T> struct OwnedPtrField;
template<typename T> struct RefIdField;

// ---------------------------------------------------------
// Archive concept
// An Archive is a type that can process NamedField/OwnedPtrField/RefIdField
// via operator&. Direction (read vs write) is part of the archive type.
// ---------------------------------------------------------
template<typename A>
concept Archive = requires(A& ar) {
    { ar.IsReading() } -> std::same_as<bool>;
    { ar.IsWriting() } -> std::same_as<bool>;
};

// ---------------------------------------------------------
// NamedField<T> — a regular value field (read/write by value)
// ---------------------------------------------------------
template<typename T>
struct NamedField {
    const char* nameStr;           // raw string — used by JSON archives as key
    Dia::Core::StringCRC name;     // CRC — used by binary archives
    T& value;
    bool required = false;

    NamedField<T>& Required() { required = true; return *this; }
};

// Helper — deduces T
template<typename T>
NamedField<T> named(const char* fieldName, T& value) {
    return NamedField<T>{ fieldName, Dia::Core::StringCRC(fieldName), value, false };
}

// ---------------------------------------------------------
// OwnedPtrField<T> — owning pointer field (serialized inline)
// ---------------------------------------------------------
template<typename T>
struct OwnedPtrField {
    const char* nameStr;           // raw string — used by JSON archives as key
    Dia::Core::StringCRC name;     // CRC — used by binary archives
    T*& ptr;
};

template<typename T>
OwnedPtrField<T> owned(const char* fieldName, T*& ptr) {
    return OwnedPtrField<T>{ fieldName, Dia::Core::StringCRC(fieldName), ptr };
}

// ---------------------------------------------------------
// RefIdField<T> — non-owning reference (serialized as ID only)
// T must be a handle/ID type (arithmetic or StringCRC)
// ---------------------------------------------------------
template<typename T>
struct RefIdField {
    const char* nameStr;           // raw string — used by JSON archives as key
    Dia::Core::StringCRC name;     // CRC — used by binary archives
    T& id;
};

template<typename T>
RefIdField<T> refId(const char* fieldName, T& id) {
    return RefIdField<T>{ fieldName, Dia::Core::StringCRC(fieldName), id };
}

} // namespace Dia::Reflect
