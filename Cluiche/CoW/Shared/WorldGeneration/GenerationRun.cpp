// S-1 Retry Mechanism: the attempt loop. See GenerationRun.h.
#include <Shared/WorldGeneration/GenerationRun.h>

#include <Shared/WorldGeneration/DeriveSubSeed.h>
#include <Shared/WorldGeneration/ReachabilityValidator.h>
#include <Shared/WorldGeneration/TerrainTableNavigationProfile.h>
#include <Shared/WorldGeneration/WorldLayoutGuarantee.h>

#include <DiaObservation/Log/DiaLog.h>

#include <chrono>
#include <cstring>
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

GenerationRun::GenerationRun(const WorldStructure& terrain, const SiteRegistry& registry, const TerrainRules& rules,
                             uint64_t outerSeed, const GenParams& params, Dia::Maths::RandomStream& rng,
                             IGenerationObserver* observer)
    : mTerrain(terrain), mRegistry(registry), mRules(rules), mOuterSeed(outerSeed)
    , mMaxAttempts(ClampMaxRetryAttempts(params.maxRetryAttempts)), mRng(rng), mObserver(observer)
{
    mReport.outerSeed = outerSeed;
    mReport.maxAttempts = mMaxAttempts;
}

void GenerationRun::Finish(GenerationOutcome outcome)
{
    mFinished = true;
    mReport.outcome = outcome;
    mReport.worldRevision = 0;
}

bool GenerationRun::Step()
{
    if (mFinished) { return false; }

    GenerationOutcome abortReason = GenerationOutcome::Cancelled;
    if (mObserver != nullptr && mObserver->ShouldAbort(abortReason))
    {
        DIA_LOG_WARNING("CoW", "World generation aborted before attempt %u (%s)", mNextAttempt + 1, GenerationOutcomeName(abortReason));
        Finish(abortReason);
        return false;
    }

    const auto started = std::chrono::steady_clock::now();
    const uint32_t attempt = mNextAttempt++;

    AttemptRecord rec;
    rec.attempt = attempt;
    rec.subSeed = DeriveSubSeed(mOuterSeed, static_cast<uint64_t>(attempt));

    mWorld = mTerrain;   // terrain and biomes are identical on every attempt
    const PlaceResult placed = RegionsAndSites::Place(mWorld, mRegistry, mRules, rec.subSeed, static_cast<int>(attempt), mRng, &mPlaceReport);

    bool passed = false;
    bool terrainCause = false;
    if (!placed.IsOk())
    {
        rec.SetPlaceFailure(placed.code, placed.reason);
        rec.AddDiagnostic("placement failed: %s", placed.reason);
        terrainCause = placed.code == PlaceResult::Code::TerrainCause;
    }
    else
    {
        const TerrainTableNavigationProfile profile;
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
        DIA_LOG_ERROR("CoW", "World generation unsatisfiable: terrain cause at attempt %u (seed %llu): %s", attempt + 1,
                      static_cast<unsigned long long>(mOuterSeed), Headline(rec));
        Finish(GenerationOutcome::Unsatisfiable);
        return false;
    }
    if (mNextAttempt >= mMaxAttempts)
    {
        DIA_LOG_ERROR("CoW", "World generation failed after %u attempt(s) (seed %llu): retry exhausted", mNextAttempt,
                      static_cast<unsigned long long>(mOuterSeed));
        Finish(GenerationOutcome::Unsatisfiable);
        return false;
    }
    return true;
}

void GenerationRun::Run()
{
    while (Step()) {}
}

GenerationReport GenerateWorld(const WorldStructure& terrain, const SiteRegistry& registry, const TerrainRules& rules,
                               uint64_t outerSeed, const GenParams& params, Dia::Maths::RandomStream& rng,
                               WorldStructure* outWorld)
{
    GenerationRun run(terrain, registry, rules, outerSeed, params, rng);
    run.Run();
    if (outWorld != nullptr && run.HasAcceptedWorld()) { *outWorld = run.AcceptedWorld(); }
    return run.Report();
}

} // namespace WorldGeneration
} // namespace CoW
