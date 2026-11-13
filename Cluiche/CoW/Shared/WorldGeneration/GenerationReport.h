// S-1 Retry Mechanism: generation parameters, outcome and the per-generation report.
// Every finished generation leaves one GenerationReport (replacing the previous one): one AttemptRecord per attempt
// (sub-seed, pass/fail, failed guarantee ids, a few diagnostics, elapsed time), the outcome, the validation result of the
// accepted world (or of the last attempt), and the world revision it produced (0 if none).
// AttemptRecord is trivially copyable (fixed buffers); GenerationReport holds the full ValidationResult evidence and
// stays on the SimPU (the bus carries only WorldGenerationRetry / WorldGenerationComplete PODs).
// Elapsed times are wall-clock and excluded from SameApartFromTiming; everything else is deterministic.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/retry-mechanism.md (Generation Report)
#pragma once

#include "PlaceResult.h"
#include "ValidationTypes.h"

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

constexpr uint32_t kDefaultMaxRetryAttempts = 10;   // placeholder (Open Q 1); AC3 measures, nobody tunes before
constexpr uint32_t kMaxRetryAttemptsLimit   = 64;   // bounds report size
constexpr unsigned kMaxAttemptDiagnostics   = 4;

constexpr float kDefaultTimeoutSeconds = 30.0f;     // placeholder (Feature 5 Open Q 1): a safety net, far above the 5 s target
constexpr float kMaxTimeoutSeconds     = 3600.0f;
constexpr unsigned kFailureMessageSize = 128;

// Feature 3 (retry bound) and Feature 5 (wall-clock budget for the whole generation, all attempts).
struct GenParams
{
    uint32_t maxRetryAttempts = kDefaultMaxRetryAttempts;
    float    timeoutSeconds   = kDefaultTimeoutSeconds;
};

// NaN -> default, negative -> 0, above kMaxTimeoutSeconds -> kMaxTimeoutSeconds. 0 means "already exceeded at the
// first attempt boundary" (the check is a strict elapsed > timeout).
float ClampTimeoutSeconds(float requested);

// 0 is clamped to 1 (at least one attempt), anything above kMaxRetryAttemptsLimit is clamped down.
uint32_t ClampMaxRetryAttempts(uint32_t requested);

enum class GenerationOutcome : uint8_t { Success, Unsatisfiable, Timeout, Cancelled, Error };
const char* GenerationOutcomeName(GenerationOutcome outcome);

struct AttemptRecord
{
    uint32_t attempt = 0;                 // 0-indexed
    uint64_t subSeed = 0;                 // DeriveSubSeed(outer, attempt)
    bool     passed = false;
    bool     terrainCause = false;        // this attempt ended generation: retrying cannot help
    PlaceResult::Code placeCode = PlaceResult::Code::Ok;   // Ok unless a placer failed
    char     placeReason[96] = {};
    uint32_t failedGuaranteeCount = 0;
    uint32_t failedGuarantees[kMaxValidationGuarantees] = {};   // guarantee id CRCs, injection order
    uint32_t diagnosticCount = 0;
    uint32_t droppedDiagnostics = 0;
    Diagnostic diagnostics[kMaxAttemptDiagnostics] = {};
    float    elapsedMs = 0.0f;            // wall-clock; excluded from determinism checks

    void AddFailedGuarantee(uint32_t idCrc);
    void AddDiagnostic(const char* format, ...);
    void SetPlaceFailure(PlaceResult::Code code, const char* reason);
};

struct GenerationReport
{
    uint64_t outerSeed = 0;
    GenerationOutcome outcome = GenerationOutcome::Success;
    uint32_t maxAttempts = 0;             // the (clamped) bound this generation ran with
    EvidenceList<AttemptRecord> attempts;
    ValidationResult finalValidation;     // accepted world's, else the last attempt's
    uint32_t worldRevision = 0;           // revision produced, 0 if none
    // Feature 5: the distinguishing text of a non-Success outcome ("TIMEOUT: ...", "FAILURE: ...", "CANCEL: ...");
    // empty on Success. Deterministic: a timeout names the budget, not the measured time.
    char     failureMessage[kFailureMessageSize] = {};

    void SetFailureMessage(const char* message);
};

// Equal in every field except AttemptRecord::elapsedMs.
bool SameApartFromTiming(const GenerationReport& a, const GenerationReport& b);

} // namespace WorldGeneration
} // namespace CoW
