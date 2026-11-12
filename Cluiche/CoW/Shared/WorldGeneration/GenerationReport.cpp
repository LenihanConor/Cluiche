// S-1 Retry Mechanism: report helpers. See GenerationReport.h.
#include <Shared/WorldGeneration/GenerationReport.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace CoW {
namespace WorldGeneration {

uint32_t ClampMaxRetryAttempts(uint32_t requested)
{
    return std::clamp<uint32_t>(requested, 1u, kMaxRetryAttemptsLimit);
}

const char* GenerationOutcomeName(GenerationOutcome outcome)
{
    switch (outcome)
    {
    case GenerationOutcome::Success:       return "Success";
    case GenerationOutcome::Unsatisfiable: return "Unsatisfiable";
    case GenerationOutcome::Timeout:       return "Timeout";
    case GenerationOutcome::Cancelled:     return "Cancelled";
    case GenerationOutcome::Error:         return "Error";
    }
    return "Unknown";
}

void AttemptRecord::AddFailedGuarantee(uint32_t idCrc)
{
    if (failedGuaranteeCount < kMaxValidationGuarantees) { failedGuarantees[failedGuaranteeCount++] = idCrc; }
}

void AttemptRecord::AddDiagnostic(const char* format, ...)
{
    if (diagnosticCount >= kMaxAttemptDiagnostics) { ++droppedDiagnostics; return; }
    Diagnostic& d = diagnostics[diagnosticCount++];
    va_list args;
    va_start(args, format);
    std::vsnprintf(d.text, sizeof(d.text), format, args);
    va_end(args);
    d.text[sizeof(d.text) - 1] = '\0';
}

void AttemptRecord::SetPlaceFailure(PlaceResult::Code code, const char* reason)
{
    placeCode = code;
    placeReason[0] = '\0';
    if (reason)
    {
        std::strncpy(placeReason, reason, sizeof(placeReason) - 1);
        placeReason[sizeof(placeReason) - 1] = '\0';
    }
}

namespace
{
    bool SameAttempt(const AttemptRecord& a, const AttemptRecord& b)
    {
        if (a.attempt != b.attempt || a.subSeed != b.subSeed || a.passed != b.passed || a.terrainCause != b.terrainCause ||
            a.placeCode != b.placeCode || std::strncmp(a.placeReason, b.placeReason, sizeof(a.placeReason)) != 0 ||
            a.failedGuaranteeCount != b.failedGuaranteeCount || a.diagnosticCount != b.diagnosticCount ||
            a.droppedDiagnostics != b.droppedDiagnostics)
        {
            return false;
        }
        for (uint32_t i = 0; i < a.failedGuaranteeCount; ++i)
        {
            if (a.failedGuarantees[i] != b.failedGuarantees[i]) { return false; }
        }
        for (uint32_t i = 0; i < a.diagnosticCount; ++i)
        {
            if (!(a.diagnostics[i] == b.diagnostics[i])) { return false; }
        }
        return true;
    }
}

bool SameApartFromTiming(const GenerationReport& a, const GenerationReport& b)
{
    if (a.outerSeed != b.outerSeed || a.outcome != b.outcome || a.maxAttempts != b.maxAttempts ||
        a.worldRevision != b.worldRevision || a.attempts.Size() != b.attempts.Size() ||
        !(a.finalValidation == b.finalValidation))
    {
        return false;
    }
    for (unsigned i = 0; i < a.attempts.Size(); ++i)
    {
        if (!SameAttempt(a.attempts[i], b.attempts[i])) { return false; }
    }
    return true;
}

} // namespace WorldGeneration
} // namespace CoW
