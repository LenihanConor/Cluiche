// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/retry-mechanism.md (AC1, AC2, AC4, AC5, AC6, AC7, SD-S1-008)
// GenerationRun: the retry loop as a steppable object, driven with test placers and test guarantees.
#include <gtest/gtest.h>
#include <Shared/CoWStepSeedNames.h>
#include <Shared/WorldGeneration/DeriveSubSeed.h>
#include <Shared/WorldGeneration/GenerationRun.h>
#include <Shared/WorldGeneration/WorldLayoutGuarantee.h>
#include <Shared/WorldGeneration/Testing/TerrainSliderDriver.h>
#include <Shared/WorldGeneration/Testing/TestGuarantees.h>
#include <Shared/WorldGeneration/Testing/TestPlacers.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>

#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaMaths/Core/RandomStream.h>

#include <cstdint>
#include <set>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;
using Dia::Maths::RandomStream;

namespace
{
    const GridDimensions kSmall{ 66, 42 };

    WorldStructure MakeTerrain(uint64_t seed)
    {
        TerrainRunner runner;
        runner.Run(seed, WorldShapeParams{}, kSmall);
        return runner.World();
    }

    bool SameTerrainAndBiomes(const WorldStructure& a, const WorldStructure& b)
    {
        if (a.CellCount() != b.CellCount()) { return false; }
        for (int i = 0; i < a.CellCount(); ++i)
        {
            const Cell& x = a.Cells()[i];
            const Cell& y = b.Cells()[i];
            if (x.elevation != y.elevation || x.latitude != y.latitude || x.waterSource != y.waterSource || x.terrain != y.terrain ||
                x.stratum != y.stratum || x.landmass != y.landmass || x.terrainFlags != y.terrainFlags ||
                x.downstream != y.downstream || x.biome != y.biome)
            {
                return false;
            }
        }
        return true;
    }

    bool SameRegionsAndSites(const WorldStructure& a, const WorldStructure& b)
    {
        if (a.CellCount() != b.CellCount() || !(a.regions == b.regions) || a.sites.Count() != b.sites.Count()) { return false; }
        for (int i = 0; i < a.CellCount(); ++i)
        {
            if (a.Cells()[i].region != b.Cells()[i].region) { return false; }
        }
        for (int i = 0; i < a.sites.Count(); ++i)
        {
            if (!(a.sites.All()[i] == b.sites.All()[i])) { return false; }
        }
        return true;
    }

    struct Fixture
    {
        TerrainRules rules = TestRules();
        RandomStream rng{ 1 };
        SiteRegistry registry;
        WorldStructure terrain;
        explicit Fixture(uint64_t terrainSeed = 1) : terrain(MakeTerrain(terrainSeed)) {}
        GenerationRun Make(uint64_t seed, uint32_t maxAttempts, IGenerationObserver* obs = nullptr)
        {
            GenParams p; p.maxRetryAttempts = maxAttempts;
            return GenerationRun(terrain, registry, rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), seed, p, rng, CoWStepSeedNames(), obs);
        }
    };

    struct Recorder : IGenerationObserver
    {
        std::vector<AttemptRecord> records;
        uint32_t lastMax = 0;
        int abortAtCall = -1;      // ShouldAbort returns true on this (0-based) call
        GenerationOutcome abortReason = GenerationOutcome::Cancelled;
        int abortCalls = 0;
        bool ShouldAbort(GenerationOutcome& reason) override
        {
            const bool abort = abortCalls++ == abortAtCall;
            if (abort) { reason = abortReason; }
            return abort;
        }
        void OnAttemptFinished(const AttemptRecord& r, uint32_t maxAttempts) override { records.push_back(r); lastMax = maxAttempts; }
    };
}

TEST(GenerationRun, AC6_FirstAttemptSuccessValidatesExactlyOnce)
{
    Fixture f;
    CountingGuarantee counter("Test.Counter");
    ASSERT_TRUE(f.registry.RegisterGuarantee(&counter));
    GenerationRun run = f.Make(1, 10);
    run.Run();
    EXPECT_EQ(counter.calls, 1);
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Success);
    EXPECT_EQ(run.Report().attempts.Size(), 1u);
    EXPECT_TRUE(run.Report().attempts[0].passed);
    EXPECT_TRUE(run.HasAcceptedWorld());
    EXPECT_GT(run.AcceptedWorld().regions.regions.Size(), 0u);
}

TEST(GenerationRun, AC2_AlwaysFailingGuaranteeStopsAfterExactlyMaxAttempts)
{
    Fixture f;
    FixedGuarantee never("Test.Never", false);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&never));
    GenerationRun run = f.Make(1, 5);
    run.Run();
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Unsatisfiable);
    EXPECT_EQ(run.Report().attempts.Size(), 5u);
    EXPECT_EQ(run.Report().maxAttempts, 5u);
    EXPECT_EQ(run.Report().worldRevision, 0u);
    EXPECT_FALSE(run.HasAcceptedWorld());
    for (const AttemptRecord& r : run.Report().attempts)
    {
        EXPECT_FALSE(r.passed);
        EXPECT_FALSE(r.terrainCause);
        EXPECT_EQ(r.failedGuaranteeCount, 1u);
        EXPECT_EQ(r.failedGuarantees[0], StringCRC("Test.Never").Value());
    }
}

TEST(GenerationRun, AC7_RetryablePlacerFailuresRetryAndReportEveryAttempt)
{
    Fixture f;
    AttemptGatedPlacer placer("Test.Gated", 2);   // attempts 0 and 1 fail, attempt 2 places
    ASSERT_TRUE(f.registry.Register(&placer));
    GenerationRun run = f.Make(42, 10);
    run.Run();
    const GenerationReport& rep = run.Report();
    ASSERT_EQ(rep.attempts.Size(), 3u);
    EXPECT_EQ(rep.outcome, GenerationOutcome::Success);
    EXPECT_EQ(rep.outerSeed, 42u);
    for (unsigned i = 0; i < 3; ++i)
    {
        EXPECT_EQ(rep.attempts[i].attempt, i);
        EXPECT_EQ(rep.attempts[i].subSeed, DeriveSubSeed(42ull, static_cast<uint64_t>(i)));
    }
    EXPECT_FALSE(rep.attempts[0].passed);
    EXPECT_FALSE(rep.attempts[1].passed);
    EXPECT_TRUE(rep.attempts[2].passed);
    EXPECT_EQ(rep.attempts[0].placeCode, PlaceResult::Code::Retryable);
    EXPECT_NE(std::string(rep.attempts[0].placeReason).find("Test.Gated"), std::string::npos);
    EXPECT_EQ(rep.attempts[2].placeCode, PlaceResult::Code::Ok);
    EXPECT_EQ(placer.placeCalls, 3);
    // The accepted attempt's evidence is the final validation.
    EXPECT_TRUE(rep.finalValidation.passed);
    const GuaranteeResult* layout = rep.finalValidation.Find(StringCRC("CoW.Guarantee.WorldLayout"));
    ASSERT_NE(layout, nullptr);
    EXPECT_TRUE(layout->satisfied);
}

TEST(GenerationRun, AC7_SameSeedGivesTheIdenticalReportApartFromTiming)
{
    Fixture f;
    AttemptGatedPlacer p1("Test.Gated", 2);
    ASSERT_TRUE(f.registry.Register(&p1));
    GenerationRun a = f.Make(7, 10); a.Run();
    GenerationRun b = f.Make(7, 10); b.Run();
    EXPECT_TRUE(SameApartFromTiming(a.Report(), b.Report()));
    EXPECT_TRUE(SameRegionsAndSites(a.AcceptedWorld(), b.AcceptedWorld()));
    GenerationRun c = f.Make(8, 10); c.Run();
    EXPECT_FALSE(SameApartFromTiming(a.Report(), c.Report()));
}

TEST(GenerationRun, ValidationFailuresRetryAndNameTheFailedGuarantee)
{
    Fixture f;
    FailFirstNGuarantee flaky("Test.Flaky", 2);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&flaky));
    GenerationRun run = f.Make(3, 10);
    run.Run();
    const GenerationReport& rep = run.Report();
    ASSERT_EQ(rep.attempts.Size(), 3u);
    EXPECT_EQ(rep.outcome, GenerationOutcome::Success);
    for (unsigned i = 0; i < 2; ++i)
    {
        EXPECT_FALSE(rep.attempts[i].passed);
        ASSERT_EQ(rep.attempts[i].failedGuaranteeCount, 1u);
        EXPECT_EQ(rep.attempts[i].failedGuarantees[0], StringCRC("Test.Flaky").Value());
        ASSERT_GE(rep.attempts[i].diagnosticCount, 1u);
        EXPECT_NE(std::string(rep.attempts[i].diagnostics[0].text).find("Test.Flaky fails"), std::string::npos);
        EXPECT_EQ(rep.attempts[i].placeCode, PlaceResult::Code::Ok);
    }
    EXPECT_TRUE(rep.attempts[2].passed);
    EXPECT_EQ(rep.attempts[2].failedGuaranteeCount, 0u);
}

TEST(GenerationRun, InjectionOrderOfFailedGuaranteesIsRegistrationOrderThenLayout)
{
    Fixture f;
    FixedGuarantee a("Test.A", false), b("Test.B", false);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&a));
    ASSERT_TRUE(f.registry.RegisterGuarantee(&b));
    GenerationRun run = f.Make(1, 1);
    run.Run();
    const AttemptRecord& r = run.Report().attempts[0];
    ASSERT_EQ(r.failedGuaranteeCount, 2u);
    EXPECT_EQ(r.failedGuarantees[0], StringCRC("Test.A").Value());
    EXPECT_EQ(r.failedGuarantees[1], StringCRC("Test.B").Value());
    ASSERT_EQ(run.Report().finalValidation.guarantees.Size(), 3u);   // A, B, then the layout guarantee last
    EXPECT_EQ(run.Report().finalValidation.guarantees[2].idCrc, StringCRC("CoW.Guarantee.WorldLayout").Value());
}

TEST(GenerationRun, TerrainCausePlacerEndsGenerationAtAttemptZeroWithoutRetry)
{
    Fixture f;
    AttemptGatedPlacer placer("Test.Terrain", 99, PlaceResult::Code::TerrainCause);
    ASSERT_TRUE(f.registry.Register(&placer));
    GenerationRun run = f.Make(1, 10);
    run.Run();
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Unsatisfiable);
    ASSERT_EQ(run.Report().attempts.Size(), 1u);
    EXPECT_TRUE(run.Report().attempts[0].terrainCause);
    EXPECT_EQ(run.Report().attempts[0].placeCode, PlaceResult::Code::TerrainCause);
    EXPECT_EQ(placer.placeCalls, 1);
    EXPECT_EQ(run.Report().worldRevision, 0u);
}

TEST(GenerationRun, TerrainCauseDiagnosticFromAGuaranteeEndsGenerationWithoutRetry)
{
    Fixture f;
    FailFirstNGuarantee terrain("Test.TerrainCause", 99, true);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&terrain));
    GenerationRun run = f.Make(1, 10);
    run.Run();
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Unsatisfiable);
    ASSERT_EQ(run.Report().attempts.Size(), 1u);
    EXPECT_TRUE(run.Report().attempts[0].terrainCause);
    EXPECT_FALSE(run.Report().attempts[0].passed);
}

TEST(GenerationRun, MixedTerrainCauseAndOrdinaryFailureStillRetries)
{
    Fixture f;
    FailFirstNGuarantee terrain("Test.TerrainCause", 99, true);
    FixedGuarantee ordinary("Test.Ordinary", false);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&terrain));
    ASSERT_TRUE(f.registry.RegisterGuarantee(&ordinary));
    GenerationRun run = f.Make(1, 4);
    run.Run();
    EXPECT_EQ(run.Report().attempts.Size(), 4u);   // retrying could still fix the ordinary failure
    for (const AttemptRecord& r : run.Report().attempts) { EXPECT_FALSE(r.terrainCause); }
}

TEST(GenerationRun, StepRunsExactlyOneAttemptAndReportsWhetherMoreMayFollow)
{
    Fixture f;
    AttemptGatedPlacer placer("Test.Gated", 2);
    ASSERT_TRUE(f.registry.Register(&placer));
    GenerationRun run = f.Make(1, 10);
    EXPECT_FALSE(run.IsFinished());
    EXPECT_TRUE(run.Step());
    EXPECT_EQ(run.AttemptsRun(), 1u);
    EXPECT_EQ(run.Report().attempts.Size(), 1u);
    EXPECT_TRUE(run.Step());
    EXPECT_EQ(run.AttemptsRun(), 2u);
    EXPECT_FALSE(run.Step());   // attempt 2 succeeds
    EXPECT_TRUE(run.IsFinished());
    EXPECT_EQ(run.AttemptsRun(), 3u);
    EXPECT_FALSE(run.Step());   // finished: no more attempts
    EXPECT_EQ(run.AttemptsRun(), 3u);
    EXPECT_EQ(run.Report().attempts.Size(), 3u);
}

TEST(GenerationRun, AC5_AttemptSubSeedsAreDistinct)
{
    for (uint64_t outer : { 0ull, 1ull, 2ull, 12345ull, 0xFFFFFFFFFFFFFFFEull, 0xFFFFFFFFFFFFFFFFull })
    {
        std::set<uint64_t> seeds;
        for (uint64_t a = 0; a < 10; ++a) { seeds.insert(DeriveSubSeed(outer, a)); }
        EXPECT_EQ(seeds.size(), 10u) << "outer " << outer;
    }
}

TEST(GenerationRun, MaxRetryAttemptsZeroRunsOneAttempt)
{
    Fixture f;
    FixedGuarantee never("Test.Never", false);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&never));
    GenerationRun run = f.Make(1, 0);
    run.Run();
    EXPECT_EQ(run.Report().maxAttempts, 1u);
    EXPECT_EQ(run.Report().attempts.Size(), 1u);
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Unsatisfiable);
}

TEST(GenerationRun, HugeMaxRetryAttemptsIsClampedToTheLimit)
{
    Fixture f;
    FixedGuarantee never("Test.Never", false);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&never));
    GenerationRun run = f.Make(1, 100000);
    run.Run();
    EXPECT_EQ(run.Report().maxAttempts, kMaxRetryAttemptsLimit);
    EXPECT_EQ(run.Report().attempts.Size(), kMaxRetryAttemptsLimit);
}

TEST(GenerationRun, TerrainAndBiomesAreIdenticalOnEveryAttemptAndTheInputIsUntouched)
{
    Fixture f;
    const WorldStructure before = f.terrain;
    FailFirstNGuarantee flaky("Test.Flaky", 2);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&flaky));
    GenerationRun run = f.Make(5, 10);
    run.Run();
    ASSERT_TRUE(run.HasAcceptedWorld());
    EXPECT_TRUE(SameTerrainAndBiomes(run.AcceptedWorld(), before));
    EXPECT_TRUE(SameTerrainAndBiomes(f.terrain, before));
    EXPECT_EQ(f.terrain.regions.regions.Size(), 0u);   // the fixed terrain never receives regions
    EXPECT_EQ(f.terrain.sites.Count(), 0);
}

TEST(GenerationRun, AC1_RetryingSeedsGiveTheSameSequenceAndWorldEveryRun)
{
    Fixture f;
    EvenRegionCountGuarantee even;
    ASSERT_TRUE(f.registry.RegisterGuarantee(&even));
    int retried = 0;
    for (uint64_t seed = 1; seed <= 40 && retried < 4; ++seed)
    {
        GenerationRun a = f.Make(seed, 10); a.Run();
        if (a.Report().attempts.Size() < 2u) { continue; }
        ++retried;
        GenerationRun b = f.Make(seed, 10); b.Run();
        EXPECT_TRUE(SameApartFromTiming(a.Report(), b.Report())) << "seed " << seed;
        EXPECT_EQ(a.Outcome(), b.Outcome());
        if (a.HasAcceptedWorld())
        {
            ASSERT_TRUE(b.HasAcceptedWorld());
            EXPECT_TRUE(SameRegionsAndSites(a.AcceptedWorld(), b.AcceptedWorld())) << "seed " << seed;
            EXPECT_EQ(a.AcceptedWorld().regions.regions.Size() % 2, 0u);
        }
    }
    EXPECT_GE(retried, 3) << "need at least three seeds that trigger retries";
}

TEST(GenerationRun, AcceptingGuaranteeInRegistryWithTheLayoutIdSuppressesTheBuiltInOne)
{
    Fixture f;
    // An app that registered its own layout guarantee (same id) must not get a second one.
    const LayoutLimits limits;
    WorldLayoutGuarantee own(f.rules, limits);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&own));
    GenerationRun run = f.Make(1, 1);
    run.Run();
    EXPECT_EQ(run.Report().finalValidation.guarantees.Size(), 1u);
}

TEST(GenerationRun, ObserverSeesEveryAttemptWithTheClampedBound)
{
    Fixture f;
    FailFirstNGuarantee flaky("Test.Flaky", 2);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&flaky));
    Recorder rec;
    GenerationRun run = f.Make(1, 10, &rec);
    run.Run();
    ASSERT_EQ(rec.records.size(), 3u);
    EXPECT_EQ(rec.lastMax, 10u);
    EXPECT_FALSE(rec.records[0].passed);
    EXPECT_TRUE(rec.records[2].passed);
}

TEST(GenerationRun, ObserverAbortBeforeAnAttemptEndsTheRunWithNoWorld)
{
    Fixture f;
    FixedGuarantee never("Test.Never", false);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&never));
    Recorder rec;
    rec.abortAtCall = 1;                              // let attempt 0 run, abort before attempt 1
    rec.abortReason = GenerationOutcome::Timeout;
    GenerationRun run = f.Make(1, 10, &rec);
    run.Run();
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Timeout);
    EXPECT_EQ(run.Report().attempts.Size(), 1u);
    EXPECT_FALSE(run.HasAcceptedWorld());
    EXPECT_EQ(run.Report().worldRevision, 0u);

    Recorder first;
    first.abortAtCall = 0;
    GenerationRun none = f.Make(1, 10, &first);
    none.Run();
    EXPECT_EQ(none.Outcome(), GenerationOutcome::Cancelled);
    EXPECT_EQ(none.Report().attempts.Size(), 0u);
}

TEST(GenerationRun, GenerateWorldReturnsTheReportAndTheAcceptedWorld)
{
    Fixture f;
    WorldStructure out;
    GenParams p;
    const GenerationReport rep = GenerateWorld(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, p, f.rng, CoWStepSeedNames(), &out);
    EXPECT_EQ(rep.outcome, GenerationOutcome::Success);
    EXPECT_GT(out.regions.regions.Size(), 0u);
    EXPECT_TRUE(SameTerrainAndBiomes(out, f.terrain));

    FixedGuarantee never("Test.Never", false);
    SiteRegistry failing;
    ASSERT_TRUE(failing.RegisterGuarantee(&never));
    WorldStructure untouched;
    GenParams q; q.maxRetryAttempts = 2;
    const GenerationReport bad = GenerateWorld(f.terrain, failing, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, q, f.rng, CoWStepSeedNames(), &untouched);
    EXPECT_EQ(bad.outcome, GenerationOutcome::Unsatisfiable);
    EXPECT_EQ(untouched.CellCount(), 0);   // no accepted world was written
}

TEST(GenerationRun, AC4_LogsAttemptCountAndDiagnosticsPerAttempt)
{
    Dia::CellLayers::Testing::LogCapture log("HexWorld");
    Fixture f;
    FailFirstNGuarantee flaky("Test.Flaky", 2);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&flaky));
    GenerationRun run = f.Make(1, 10);
    run.Run();
    EXPECT_EQ(log.Warnings("Attempt 1 of 10 failed"), 1u);
    EXPECT_EQ(log.Warnings("Attempt 2 of 10 failed"), 1u);
    EXPECT_EQ(log.Warnings("Test.Flaky fails"), 2u);
    EXPECT_EQ(log.Count(Dia::CellLayers::Testing::LogCapture::Level::kInfo, "succeeded on attempt 3 of 10"), 1u);
    EXPECT_EQ(log.Errors(), 0u);
}

TEST(GenerationRun, AC4_ExhaustionLogsAnErrorWithTheAttemptCount)
{
    Dia::CellLayers::Testing::LogCapture log("HexWorld");
    Fixture f;
    FixedGuarantee never("Test.Never", false);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&never));
    GenerationRun run = f.Make(1, 3);
    run.Run();
    EXPECT_EQ(log.Warnings("failed (seed 1"), 3u);
    EXPECT_EQ(log.Errors("failed after 3 attempt"), 1u);
}

TEST(GenerationRun, AFailureWithoutAnyDiagnosticIsNeverATerrainCause)
{
    Fixture f;
    SilentFailGuarantee silent("Test.Silent");
    ASSERT_TRUE(f.registry.RegisterGuarantee(&silent));
    GenerationRun run = f.Make(1, 3);
    run.Run();
    EXPECT_EQ(run.Report().attempts.Size(), 3u);   // retried to the bound
    for (const AttemptRecord& r : run.Report().attempts)
    {
        EXPECT_FALSE(r.terrainCause);
        EXPECT_EQ(r.failedGuaranteeCount, 1u);
        EXPECT_EQ(r.diagnosticCount, 0u);
    }
}

TEST(GenerationRun, AnIndependentExtraPlacerNeverChangesTheAttemptSequenceOrTheRegions)
{
    // Registered kinds are placed after regions and built-in nodes, on their own sub-seeds, so adding one that always
    // succeeds leaves every attempt's sub-seed, pass/fail and the regions/built-in nodes unchanged.
    Fixture a;
    EvenRegionCountGuarantee evenA;
    ASSERT_TRUE(a.registry.RegisterGuarantee(&evenA));
    Fixture b;
    EvenRegionCountGuarantee evenB;
    TestPlacer extra("Test.Extra", {}, 3);
    ASSERT_TRUE(b.registry.RegisterGuarantee(&evenB));
    ASSERT_TRUE(b.registry.Register(&extra));

    int compared = 0;
    for (uint64_t seed = 1; seed <= 12; ++seed)
    {
        GenerationRun ra = a.Make(seed, 10); ra.Run();
        GenerationRun rb = b.Make(seed, 10); rb.Run();
        ASSERT_EQ(ra.Report().attempts.Size(), rb.Report().attempts.Size()) << "seed " << seed;
        for (unsigned i = 0; i < ra.Report().attempts.Size(); ++i)
        {
            EXPECT_EQ(ra.Report().attempts[i].subSeed, rb.Report().attempts[i].subSeed);
            EXPECT_EQ(ra.Report().attempts[i].passed, rb.Report().attempts[i].passed);
        }
        if (ra.HasAcceptedWorld())
        {
            ASSERT_TRUE(rb.HasAcceptedWorld());
            EXPECT_TRUE(ra.AcceptedWorld().regions == rb.AcceptedWorld().regions) << "seed " << seed;
            EXPECT_EQ(rb.AcceptedWorld().sites.Count(), ra.AcceptedWorld().sites.Count() + 1);   // only the extra site
            ++compared;
        }
    }
    EXPECT_GE(compared, 10);
}

TEST(GenerationRun, TinyAndDegenerateGridsFinishWithAConsistentReport)
{
    const TerrainRules rules = TestRules();
    SiteRegistry registry;
    RandomStream rng(1);
    for (GridDimensions dims : { GridDimensions{ 1, 1 }, GridDimensions{ 3, 3 }, GridDimensions{ 8, 6 }, GridDimensions{ 20, 14 } })
    {
        TerrainRunner runner;
        runner.Run(5, WorldShapeParams{}, dims);
        const WorldStructure terrain = runner.World();
        GenParams p; p.maxRetryAttempts = 4;
        GenerationRun run(terrain, registry, rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 5, p, rng, CoWStepSeedNames());
        run.Run();
        SCOPED_TRACE(dims.width * 1000 + dims.height);
        EXPECT_TRUE(run.IsFinished());
        EXPECT_GE(run.Report().attempts.Size(), 1u);
        EXPECT_LE(run.Report().attempts.Size(), 4u);
        const GenerationOutcome o = run.Outcome();
        EXPECT_TRUE(o == GenerationOutcome::Success || o == GenerationOutcome::Unsatisfiable);
        EXPECT_EQ(o == GenerationOutcome::Success, run.Report().attempts[run.Report().attempts.Size() - 1].passed);
        EXPECT_EQ(run.HasAcceptedWorld(), o == GenerationOutcome::Success);
    }
}

TEST(GenerationRun, ReportKeepsTheLastAttemptsValidationWhenNothingWasAccepted)
{
    Fixture f;
    FixedGuarantee never("Test.Never", false);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&never));
    GenerationRun run = f.Make(1, 2);
    run.Run();
    EXPECT_FALSE(run.Report().finalValidation.passed);
    const GuaranteeResult* g = run.Report().finalValidation.Find(StringCRC("Test.Never"));
    ASSERT_NE(g, nullptr);
    EXPECT_FALSE(g->satisfied);
}
