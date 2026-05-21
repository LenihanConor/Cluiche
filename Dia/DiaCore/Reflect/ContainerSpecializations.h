#pragma once
// DiaReflect — container type traits and archive specialization helpers
// Included by JsonArchive.h and BinaryArchive.h to enable:
//   - C-style static arrays  T[N]
//   - DynamicArrayC<T, N>
//
// HashTableC specialization is deferred (see note at bottom of this file).
//
// Design: type traits live here so both archive headers share the same
// detection logic without duplicating it.

#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include <type_traits>
#include <cstddef>

namespace Dia::Reflect {

// ---------------------------------------------------------
// IsDynamicArrayC<T> — true_type when T is DynamicArrayC<E,N>
// ---------------------------------------------------------
template<typename T>
struct IsDynamicArrayC : std::false_type {};

template<typename T, unsigned int N>
struct IsDynamicArrayC<Dia::Core::Containers::DynamicArrayC<T, N>> : std::true_type {};

// ---------------------------------------------------------
// DynamicArrayCElem<T> — extract the element type
// ---------------------------------------------------------
template<typename T>
struct DynamicArrayCElem;

template<typename T, unsigned int N>
struct DynamicArrayCElem<Dia::Core::Containers::DynamicArrayC<T, N>> {
    using type = T;
    static constexpr unsigned int capacity = N;
};

// ---------------------------------------------------------
// NOTE: HashTableC specialization not implemented.
// HashTableC<K,V,N> requires iteration over key-value pairs
// and a scheme for JSON key representation (keys may not be
// strings).  Defer until the serialization scheme for
// associative containers is decided at spec level.
// ---------------------------------------------------------

} // namespace Dia::Reflect
