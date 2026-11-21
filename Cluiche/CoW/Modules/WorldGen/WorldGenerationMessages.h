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

// Why a Regenerate ended without a world (timeout-failure-handling.md). Unsatisfiable covers retry exhaustion and a
// terrain-level cause; Success has no reason.
enum class FailureReason : uint8_t { Timeout, Unsatisfiable, Error, ManualCancel };

// Published once when a Regenerate ends without an accepted world (timeout, unsatisfiable, error or cancel). The previous
// world and its revision are untouched. message carries the distinguishing text ("TIMEOUT: ...", "FAILURE: ...",
// "CANCEL: ...") and is truncated to fit.
struct WorldGenerationFailed
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.WorldGenerationFailed" };
    FailureReason reason   = FailureReason::Unsatisfiable;
    uint64_t seed          = 0;
    uint32_t attempts      = 0;    // attempts that ran
    char     message[128]  = {};
};

} // namespace CoW
