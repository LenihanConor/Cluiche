#pragma once
#include <cstdint>
#include <DiaCore/CRC/CRC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <diaentitytemplate/Entity.h>

namespace Dia::Entity {

    inline constexpr uint32_t kMaxQueryTypes    = 64;
    inline constexpr uint32_t kMaxTypesPerQuery = kMaxComponentTypesPerDomain;

    // Internal cache entry for a single query signature.
    // Stores the sorted-XOR signature, the individual component type CRCs,
    // and the list of matching entity handles.
    // Rebuilt by Domain::EndOfFrame when dirty.
    struct QueryCache {
        // Combined (sorted-XOR) signature of all component type CRC values.
        // Uses CRC (not StringCRC) because the value is computed, not derived from a string.
        Dia::Core::CRC signatureCRC;

        // Individual component type CRCs that make up this query.
        // Used by InvalidateCachesForType to check membership without reversing the XOR.
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxTypesPerQuery> typeCRCs;

        Dia::Core::Containers::DynamicArrayC<Entity, kMaxEntitiesPerDomain> entities;
        bool dirty = true;
    };

} // namespace Dia::Entity
