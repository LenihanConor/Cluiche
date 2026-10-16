#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace CoW {

// Published by S-1 WorldGenerationModule when a new world has been accepted (generated or imported).
// A notification only: the world is read via WorldGenerationModule::GetCurrentWorld() and
// re-read when worldRevision changes. Bus messages must stay trivially copyable.
struct WorldGenerationComplete
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.WorldGenerationComplete" };
    uint64_t seed          = 0;
    uint32_t retryAttempts = 0;   // 0 if the first attempt succeeded
    uint32_t worldRevision = 0;   // matches GetWorldRevision() at publish time
};

} // namespace CoW
