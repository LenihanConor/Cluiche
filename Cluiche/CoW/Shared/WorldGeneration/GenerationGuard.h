// S-1 Timeout and Failure Handling: the abort checks of a generation run.
//
// GenerationGuard is a GenerationRun observer. At each attempt boundary it ends the run when
//   1. a cancel was requested (outcome Cancelled), else
//   2. the elapsed wall-clock time is strictly greater than the budget (outcome Timeout).
// Cancel is checked first, as in the spec's per-frame state machine. Time comes from an IGenerationClock so tests
// advance it by hand (no sleeping); production uses the steady clock. The check is one per attempt boundary
// (coarse, Feature 5 Open Q 2): an attempt that overruns finishes, then the next boundary reports Timeout.
//
// Not thread-safe: every cow.worldgen.* command is sim-hosted, so cancel and Tick share the SimPU thread.
// The message strings are deterministic (the timeout text names the budget, not the measured time).
//
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/timeout-failure-handling.md
#pragma once

#include "GenerationRun.h"

#include <cstdint>

namespace CoW {
namespace WorldGeneration {

class IGenerationClock
{
public:
    virtual ~IGenerationClock() = default;
    virtual double NowSeconds() const = 0;   // monotonic, seconds
};

// std::chrono::steady_clock.
const IGenerationClock& SteadyGenerationClock();

class GenerationGuard : public IGenerationObserver
{
public:
    // timeoutSeconds is used as given (callers clamp with ClampTimeoutSeconds). 'clock' null = the steady clock.
    // 'next' (optional) receives OnAttemptFinished; it is not consulted for aborts.
    explicit GenerationGuard(float timeoutSeconds, const IGenerationClock* clock = nullptr, IGenerationObserver* next = nullptr);

    void   RequestCancel()               { mCancelRequested = true; }
    bool   CancelRequested() const       { return mCancelRequested; }
    float  TimeoutSeconds() const        { return mTimeoutSeconds; }
    double ElapsedSeconds() const;
    // elapsed > timeout (strict).
    bool   TimedOut() const;

    bool ShouldAbort(GenerationOutcome& reason) override;
    const char* AbortMessage() const override { return mMessage; }
    void OnAttemptFinished(const AttemptRecord& record, uint32_t maxAttempts) override;

private:
    const IGenerationClock* mClock;
    IGenerationObserver*    mNext;
    float                   mTimeoutSeconds;
    double                  mStart;
    bool                    mCancelRequested = false;
    char                    mMessage[kFailureMessageSize] = {};
};

// Blocking run under a caller-owned guard (injected clock, cancel flag). Same result contract as GenerationRun.h's.
GenerationReport GenerateWorld(const WorldStructure& terrain, const SiteRegistry& registry, const TerrainRules& rules,
                               uint64_t outerSeed, const GenParams& params, Dia::Maths::RandomStream& rng,
                               GenerationGuard& guard, WorldStructure* outWorld = nullptr);

} // namespace WorldGeneration
} // namespace CoW
