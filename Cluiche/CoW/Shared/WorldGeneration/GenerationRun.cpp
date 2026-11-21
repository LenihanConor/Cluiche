// S-1 Retry Mechanism: the attempt loop. See GenerationRun.h.
#include <Shared/WorldGeneration/GenerationRun.h>
#include <DraoiHexWorld/BiomeRegistry.h>

#include <Shared/WorldGeneration/DeriveSubSeed.h>
#include <Shared/WorldGeneration/GenerationGuard.h>
#include <Shared/WorldGeneration/ReachabilityValidator.h>
#include <Shared/WorldGeneration/TerrainTableNavigationProfile.h>
#include <Shared/WorldGeneration/WorldLayoutGuarantee.h>

#include <DiaObservation/Log/DiaLog.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <utility>

namespace CoW {
namespace WorldGeneration {

namespace
{
    constexpr const char* kTerrainCauseMarker = "(terrain cause";

    bool IsTerrainCauseDiagnostic(const Diagnostic& d) { return std::strstr(d.text, kTerrainCauseMarker) != nullptr; }

    // At least one guarantee failed, and every diagnostic of every failed guarantee names a terrain cause.
    bool FailureIsTerrainCause(const ValidationResult& v)
    {
        bool anyFailed = false;
        for (unsigned i = 0; i < v.guarantees.Size(); ++i)
        {
            const GuaranteeResult& g = v.guarantees[i];
            if (g.satisfied) { continue; }
            anyFailed = true;
            if (g.diagnostics.IsEmpty() || g.droppedDiagnostics > 0) { return false; }
            for (const Diagnostic& d : g.diagnostics)
            {
                if (!IsTerrainCauseDiagnostic(d)) { return false; }
            }
        }
        return anyFailed && v.diagnostics.IsEmpty();
    }

    void RecordValidation(AttemptRecord& rec, const ValidationResult& v)
    {
        for (unsigned i = 0; i < v.diagnostics.Size(); ++i) { rec.AddDiagnostic("%s", v.diagnostics[i].text); }
        for (unsigned i = 0; i < v.guarantees.Size(); ++i)
        {
            const GuaranteeResult& g = v.guarantees[i];
            if (g.satisfied) { continue; }
            rec.AddFailedGuarantee(g.idCrc);
            for (const Diagnostic& d : g.diagnostics) { rec.AddDiagnostic("%s", d.text); }
            rec.droppedDiagnostics += g.droppedDiagnostics;
        }
    }

    // The placer reason or the first diagnostic, for log lines.
    const char* Headline(const AttemptRecord& rec)
    {
        if (rec.placeReason[0] != '\0') { return rec.placeReason; }
        if (rec.diagnosticCount > 0) { return rec.diagnostics[0].text; }
        return "(no diagnostic)";
    }
}

GenerationRun::GenerationRun(const WorldStructure& terrain, const SiteRegistry& registry, const TerrainRules& rules, const Draoi::HexWorld::BiomeRegistry& biomes,
                             uint64_t outerSeed, const GenParams& params, Dia::Maths::RandomStream& rng,
                             IGenerationObserver* observer)
    : mTerrain(terrain), mRegistry(registry), mRules(rules), mBiomes(biomes), mOuterSeed(outerSeed)
    , mMaxAttempts(ClampMaxRetryAttempts(params.maxRetryAttempts)), mRng(rng), mObserver(observer)
{
    mReport.outerSeed = outerSeed;
    mReport.maxAttempts = mMaxAttempts;
}

void GenerationRun::Finish(GenerationOutcome outcome, const char* message)
{
    mFinished = true;
    mReport.outcome = outcome;
    mReport.worldRevision = 0;
    mReport.SetFailureMessage(message);
}

bool GenerationRun::Step()
{
    if (mFinished) { return false; }

    GenerationOutcome abortReason = GenerationOutcome::Cancelled;
    if (mObserver != nullptr && mObserver->ShouldAbort(abortReason))
    {
        // Timeout and Cancelled are the observer's (Feature 5); anything else it reports is an Error.
        if (abortReason != GenerationOutcome::Timeout && abortReason != GenerationOutcome::Cancelled) { abortReason = GenerationOutcome::Error; }
        const char* given = mObserver->AbortMessage();
        const char* fallback = abortReason == GenerationOutcome::Timeout ? "TIMEOUT: World generation timed out"
                             : abortReason == GenerationOutcome::Cancelled ? "CANCEL: World generation cancelled by user"
                                                                           : "FAILURE: World generation aborted";
        const char* message = (given != nullptr && given[0] != '\0') ? given : fallback;
        if (abortReason == GenerationOutcome::Cancelled)
        {
            DIA_LOG_INFO("CoW", "%s (before attempt %u, seed %llu)", message, mNextAttempt + 1, static_cast<unsigned long long>(mOuterSeed));
        }
        else
        {
            DIA_LOG_ERROR("CoW", "%s (before attempt %u, seed %llu)", message, mNextAttempt + 1, static_cast<unsigned long long>(mOuterSeed));
        }
        Finish(abortReason, message);
        return false;
    }

    const auto started = std::chrono::steady_clock::now();
    const uint32_t attempt = mNextAttempt++;

    AttemptRecord rec;
    rec.attempt = attempt;
    rec.subSeed = DeriveSubSeed(mOuterSeed, static_cast<uint64_t>(attempt));

    bool passed = false;
    bool terrainCause = false;
    try
    {
        mWorld = mTerrain;   // terrain and biomes are identical on every attempt
        const PlaceResult placed = RegionsAndSites::Place(mWorld, mRegistry, mRules, mBiomes, rec.subSeed, static_cast<int>(attempt), mRng, &mPlaceReport);

        if (!placed.IsOk())
        {
            rec.SetPlaceFailure(placed.code, placed.reason);
            rec.AddDiagnostic("placement failed: %s", placed.reason);
            terrainCause = placed.code == PlaceResult::Code::TerrainCause;
        }
        else
        {
            const TerrainTableNavigationProfile profile(mRules);
            const WorldLayoutGuarantee layout(mRules, mPlaceReport.limits);
            ValidationParams vp;
            vp.AddProfile(&profile);
            bool registryHasLayout = false;
            for (const IWorldGuarantee* g : mRegistry.Guarantees())
            {
                vp.AddGuarantee(g);
                registryHasLayout = registryHasLayout || g->GetId() == layout.GetId();
            }
            if (!registryHasLayout) { vp.AddGuarantee(&layout); }

            ValidationResult validation = ReachabilityValidator::Validate(mWorld, vp);
            passed = validation.passed;
            if (!passed)
            {
                RecordValidation(rec, validation);
                terrainCause = FailureIsTerrainCause(validation);
            }
            mReport.finalValidation = std::move(validation);
        }
    }
    catch (const std::exception& e)
    {
        FinishWithError(rec, started, e.what());
        return false;
    }
    catch (...)
    {
        FinishWithError(rec, started, "unknown exception");
        return false;
    }

    rec.passed = passed;
    rec.terrainCause = terrainCause;
    rec.elapsedMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - started).count();
    mReport.attempts.Add(rec);

    if (passed)
    {
        DIA_LOG_INFO("CoW", "World generation succeeded on attempt %u of %u (seed %llu)", attempt + 1, mMaxAttempts,
                     static_cast<unsigned long long>(mOuterSeed));
    }
    else
    {
        DIA_LOG_WARNING("CoW", "Attempt %u of %u failed (seed %llu, %u guarantee(s)): %s", attempt + 1, mMaxAttempts,
                        static_cast<unsigned long long>(mOuterSeed), rec.failedGuaranteeCount, Headline(rec));
    }
    if (mObserver != nullptr) { mObserver->OnAttemptFinished(rec, mMaxAttempts); }

    if (passed)
    {
        mFinished = true;
        mReport.outcome = GenerationOutcome::Success;
        return false;
    }
    if (terrainCause)
    {
        char message[kFailureMessageSize];
        std::snprintf(message, sizeof(message), "FAILURE: World generation unsatisfiable: terrain cause at attempt %u", attempt + 1);
        DIA_LOG_ERROR("CoW", "%s (seed %llu): %s", message, static_cast<unsigned long long>(mOuterSeed), Headline(rec));
        Finish(GenerationOutcome::Unsatisfiable, message);
        return false;
    }
    if (mNextAttempt >= mMaxAttempts)
    {
        char message[kFailureMessageSize];
        std::snprintf(message, sizeof(message), "FAILURE: World generation failed after %u attempts (unsatisfiable seed or parameters)", mNextAttempt);
        DIA_LOG_ERROR("CoW", "%s (seed %llu)", message, static_cast<unsigned long long>(mOuterSeed));
        Finish(GenerationOutcome::Unsatisfiable, message);
        return false;
    }
    return true;
}

void GenerationRun::FinishWithError(AttemptRecord& rec, std::chrono::steady_clock::time_point started, const char* what)
{
    rec.passed = false;
    rec.terrainCause = false;
    rec.AddDiagnostic("error: %s", what);
    rec.elapsedMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - started).count();
    mReport.attempts.Add(rec);
    char message[kFailureMessageSize];
    std::snprintf(message, sizeof(message), "FAILURE: World generation error: %s", what);
    DIA_LOG_ERROR("CoW", "%s (attempt %u, seed %llu)", message, rec.attempt + 1, static_cast<unsigned long long>(mOuterSeed));
    if (mObserver != nullptr) { mObserver->OnAttemptFinished(rec, mMaxAttempts); }
    Finish(GenerationOutcome::Error, message);
}

void GenerationRun::Run()
{
    while (Step()) {}
}

GenerationReport GenerateWorld(const WorldStructure& terrain, const SiteRegistry& registry, const TerrainRules& rules, const Draoi::HexWorld::BiomeRegistry& biomes,
                               uint64_t outerSeed, const GenParams& params, Dia::Maths::RandomStream& rng,
                               WorldStructure* outWorld)
{
    GenerationGuard guard(ClampTimeoutSeconds(params.timeoutSeconds));
    return GenerateWorld(terrain, registry, rules, biomes, outerSeed, params, rng, guard, outWorld);
}

GenerationReport GenerateWorld(const WorldStructure& terrain, const SiteRegistry& registry, const TerrainRules& rules, const Draoi::HexWorld::BiomeRegistry& biomes,
                               uint64_t outerSeed, const GenParams& params, Dia::Maths::RandomStream& rng,
                               GenerationGuard& guard, WorldStructure* outWorld)
{
    GenerationRun run(terrain, registry, rules, biomes, outerSeed, params, rng, &guard);
    run.Run();
    if (outWorld != nullptr && run.HasAcceptedWorld()) { *outWorld = run.AcceptedWorld(); }
    return run.Report();
}

} // namespace WorldGeneration
} // namespace CoW
