#pragma once

#include <cstdint>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Attribute
{
    // A resolved list of map keys, in the order the map's columns appear in the schema (PD-004: pointer + count).
    // The arrays are owned by the source and must outlive the FromValueSchema call.
    struct MapKeys
    {
        const Dia::Core::StringCRC* ids   = nullptr;
        const char* const*          names = nullptr;
        uint32_t                    count = 0;
    };

    // Supplies map keys for sidecar `map` entries, resolved by the entry's keyRef string.
    class IMapKeySource
    {
    public:
        virtual ~IMapKeySource() = default;

        // Returns false when keyRef is unknown to this source.
        virtual bool GetKeys(const char* keyRef, MapKeys& out) const = 0;
    };
}
