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

// Published once per failed generation attempt, for live progress ("Attempt 2/10: 1 guarantee failed"). A small POD:
// the reasons are in the module's GenerationReport (GetLastReport()). Not published for live preview (it never retries).
struct WorldGenerationRetry
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.WorldGenerationRetry" };
    uint32_t attemptNumber        = 0;   // 1-indexed
    uint32_t maxAttempts          = 0;
    uint32_t failedGuaranteeCount = 0;   // guarantees that failed in the attempt that just failed
};

} // namespace CoW
