// S-1 Retry Mechanism: the attempt loop as a pure, steppable object.
//
// A run is given the FIXED terrain world (terrain + biomes written, regions and sites empty, SD-S1-008), the site
// registry, the rules, the outer seed and GenParams. Step() runs exactly one attempt:
//   copy the terrain -> RegionsAndSites::Place(copy, ..., DeriveSubSeed(outer, attempt), attempt, rng, names) -> validate.
// Terrain and biomes are never touched, so every attempt re-rolls only Regions and Sites.
//
// Validation: SiteRegistry::Guarantees() in registration order, then (last) a WorldLayoutGuarantee built from this
// attempt's PlaceReport::limits (skipped if the registry already holds a guarantee with the same id), with the
// placeholder TerrainTableNavigationProfile injected. The validator runs once per attempt, never after success.
//
// Stop rules: success; attempts == maxRetryAttempts (Unsatisfiable); a placer returning TerrainCause, or a validation
// failure whose every diagnostic is marked "(terrain cause" (Unsatisfiable at that attempt, terrainCause = true, no
// retry); an observer abort (Feature 5 seam: Timeout / Cancelled / Error, no world accepted).
//
// Run() / GenerateWorld() loop Step() for tests and headless use. The module runs it to completion in one frame today;
// stepping one attempt per frame is Feature 5's state machine and needs no change here.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/retry-mechanism.md
#pragma once

#include "GenerationReport.h"
#include "LayoutCheck.h"
#include "RegionsAndSites.h"
#include "SiteRegistry.h"
#include "TerrainRules.h"
#include "TerrainTypes.h"

#include <DiaMaths/Core/RandomStream.h>

#include <chrono>
#include <cstdint>

namespace Draoi { namespace HexWorld { class BiomeRegistry; } }

namespace Draoi {
namespace HexWorld {

class IGenerationObserver
{
public:
    virtual ~IGenerationObserver() = default;
    // Polled at each attempt boundary before the attempt runs. Return true and set 'reason' (Timeout, Cancelled or
    // Error) to end the run with no accepted world. GenerationGuard (Feature 5) implements Timeout and Cancelled.
    virtual bool ShouldAbort(GenerationOutcome& reason) { (void)reason; return false; }
    // The distinguishing text for the abort just reported by ShouldAbort ("TIMEOUT: ...", "CANCEL: ..."), recorded in
    // GenerationReport::failureMessage. nullptr lets the run use its generic text for the outcome.
    virtual const char* AbortMessage() const { return nullptr; }
    // After every attempt (passed or not) has been recorded.
    virtual void OnAttemptFinished(const AttemptRecord& record, uint32_t maxAttempts) { (void)record; (void)maxAttempts; }
};

class GenerationRun
{
public:
    // 'terrain', 'registry', 'rules' and 'biomes' must outlive the run. params.maxRetryAttempts is clamped (ClampMaxRetryAttempts).
    GenerationRun(const WorldStructure& terrain, const SiteRegistry& registry, const TerrainRules& rules, const Draoi::HexWorld::BiomeRegistry& biomes, uint64_t outerSeed,
                  const GenParams& params, Dia::Maths::RandomStream& rng, const StepSeedNames& names, IGenerationObserver* observer = nullptr);

    // Runs one attempt (or ends the run on an observer abort). Returns true while the run is not finished.
    bool Step();
    // Steps until finished.
    void Run();

    bool IsFinished() const { return mFinished; }
    GenerationOutcome Outcome() const { return mReport.outcome; }
    uint32_t AttemptsRun() const { return mNextAttempt; }
    const GenerationReport& Report() const { return mReport; }
    // The caller stamps the revision the accepted world was given (Success only).
    void SetWorldRevision(uint32_t revision) { mReport.worldRevision = revision; }

    // Valid when finished with Outcome() == Success.
    bool HasAcceptedWorld() const { return mFinished && mReport.outcome == GenerationOutcome::Success; }
    const WorldStructure& AcceptedWorld() const { return mWorld; }
    WorldStructure& AcceptedWorld() { return mWorld; }
    const LayoutLimits& AcceptedLimits() const { return mPlaceReport.limits; }
    const RegionsAndSites::PlaceReport& LastPlaceReport() const { return mPlaceReport; }

private:
    void Finish(GenerationOutcome outcome, const char* message = nullptr);
    void FinishWithError(AttemptRecord& rec, std::chrono::steady_clock::time_point started, const char* what);

    const WorldStructure&     mTerrain;
    const SiteRegistry&       mRegistry;
    const TerrainRules&       mRules;
    const Draoi::HexWorld::BiomeRegistry& mBiomes;
    uint64_t                  mOuterSeed;
    uint32_t                  mMaxAttempts;
    Dia::Maths::RandomStream& mRng;
    StepSeedNames             mNames;
    IGenerationObserver*      mObserver;

    WorldStructure                mWorld;   // the last attempt's world; the accepted one after Success
    RegionsAndSites::PlaceReport  mPlaceReport;
    GenerationReport              mReport;
    uint32_t                      mNextAttempt = 0;
    bool                          mFinished = false;
};

// Blocking convenience: terrain in, report out. On Success 'outWorld' (if non-null) receives the accepted world.
// Enforces params.timeoutSeconds on the steady clock (a GenerationGuard); the overload in GenerationGuard.h takes the
// guard so tests can inject a clock and a cancel.
GenerationReport GenerateWorld(const WorldStructure& terrain, const SiteRegistry& registry, const TerrainRules& rules, const Draoi::HexWorld::BiomeRegistry& biomes,
                               uint64_t outerSeed, const GenParams& params, Dia::Maths::RandomStream& rng, const StepSeedNames& names,
                               WorldStructure* outWorld = nullptr);

} // namespace HexWorld
} // namespace Draoi
