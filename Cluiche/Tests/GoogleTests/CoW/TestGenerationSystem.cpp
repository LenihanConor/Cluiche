// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md (Tests 1a, 1b, 2a, 5a) and
// retry-mechanism.md (AC1, AC3). System-level checks of the retry loop through the real session and over many seeds.
// The 100-seed sweeps are SLOW_ (run with --all); thresholds are tuning-gated: a miss is reported with numbers, not tuned.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>
#include <Shared/WorldGeneration/DeriveSubSeed.h>
#include <Shared/WorldGeneration/GenerationRun.h>
#include <Shared/WorldGeneration/LayoutCheck.h>
#include <Shared/WorldGeneration/RegionsAndSites.h>
#include <Shared/WorldGeneration/WorldGenSession.h>
#include <Shared/WorldGeneration/WorldShapePreset.h>
#include <Shared/WorldGeneration/Testing/TerrainSliderDriver.h>
#include <Shared/WorldGeneration/Testing/TestGuarantees.h>
#include <Shared/WorldGeneration/Testing/SessionDrive.h>
#include <Shared/WorldGeneration/Testing/TestPlacers.h>

#include <DiaMaths/Core/RandomStream.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;
using Dia::Maths::RandomStream;

namespace
{
    const GridDimensions kSmall{ 66, 42 };

    std::string FindShippedPresetDir()
    {
        std::error_code       ec;
        std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/Presets";
            if (std::filesystem::exists(candidate, ec)) { return candidate.string(); }
            if (dir == dir.parent_path()) { break; }
            dir = dir.parent_path();
        }
        return {};
    }

    WorldShapeParams LoadParams(const char* id)
    {
        if (std::string(id) == "default") { return WorldShapeParams{}; }
        WorldShapePreset  p;
        RuleWarnings      w;
        const std::string dir = FindShippedPresetDir();
        EXPECT_FALSE(dir.empty());
        const PresetResult r = LoadPreset(dir, id, p, w);
        EXPECT_TRUE(r.Ok()) << r.message;
        return p.params;
    }

    WorldStructure MakeTerrain(uint64_t seed, const WorldShapeParams& shape)
    {
        TerrainRunner runner;
        runner.Run(seed, shape, kSmall);
        return runner.World();
    }

    struct Sweep
    {
        int total = 0;
        int success = 0;
        int firstAttempt = 0;
        int guaranteeViolations = 0;   // Test 1b: a successful world that an independent check rejects
        std::map<size_t, int> attemptsHistogram;   // attempts used -> worlds
        int unsatisfiable = 0;
        int terrainCause = 0;
    };

    Sweep RunSweep(const char* preset, int seeds)
    {
        const WorldShapeParams shape = LoadParams(preset);
        const TerrainRules rules = TestRules();
        const SiteRegistry registry;
        RandomStream rng(1);
        Sweep out;
        for (int seed = 1; seed <= seeds; ++seed)
        {
            const WorldStructure terrain = MakeTerrain(static_cast<uint64_t>(seed), shape);
            GenerationRun run(terrain, registry, rules, static_cast<uint64_t>(seed), GenParams{}, rng);
            run.Run();
            ++out.total;
            ++out.attemptsHistogram[run.Report().attempts.Size()];
            if (run.HasAcceptedWorld())
            {
                ++out.success;
                if (run.Report().attempts.Size() == 1) { ++out.firstAttempt; }
                // Test 1b: re-check independently of the run's own validation.
                const LayoutReport layout = CheckLayout(run.AcceptedWorld(), rules, run.AcceptedLimits());
                const GuaranteeResult* g = run.Report().finalValidation.Find(StringCRC("CoW.Guarantee.WorldLayout"));
                if (!layout.ok || !run.Report().finalValidation.passed || g == nullptr || !g->satisfied) { ++out.guaranteeViolations; }
            }
            else
            {
                ++out.unsatisfiable;
                if (!run.Report().attempts.IsEmpty() && run.Report().attempts[run.Report().attempts.Size() - 1].terrainCause) { ++out.terrainCause; }
            }
        }
        return out;
    }

    void PrintSweep(const char* suite, const char* preset, const Sweep& s)
    {
        std::printf("[%s] %s: %d/%d succeeded (%d on attempt 1), %d unsatisfiable (%d terrain cause), %d guarantee violations; attempts:",
                    suite, preset, s.success, s.total, s.firstAttempt, s.unsatisfiable, s.terrainCause, s.guaranteeViolations);
        for (const auto& [n, count] : s.attemptsHistogram) { std::printf(" %zu x%d", n, count); }
        std::printf("\n");
    }

    bool SameCellBytes(const WorldStructure& a, const WorldStructure& b)
    {
        return a.CellCount() == b.CellCount() && std::memcmp(a.Cells(), b.Cells(), sizeof(Cell) * static_cast<size_t>(a.CellCount())) == 0;
    }

    bool SameSites(const WorldStructure& a, const WorldStructure& b)
    {
        if (a.sites.Count() != b.sites.Count()) { return false; }
        for (int i = 0; i < a.sites.Count(); ++i)
        {
            if (!(a.sites.All()[i] == b.sites.All()[i])) { return false; }
        }
        return true;
    }

    struct Gen
    {
        TestSession s;
        RandomStream rng{ 1 };
        explicit Gen(const SiteRegistry* reg = nullptr) { if (reg) { s.SetSiteRegistry(reg); } }
        bool Regenerate(uint64_t seed, uint32_t maxRetry = kDefaultMaxRetryAttempts)
        {
            Json::Value a(Json::objectValue);
            a["seed"] = static_cast<Json::UInt64>(seed);
            a["maxRetryAttempts"] = maxRetry;
            const Json::Value r = s.Regenerate(a);
            EXPECT_TRUE(r["success"].asBool());
            return TickUntilIdle(s, rng);
        }
    };
}

// ---- Test 1a / AC3 and Test 1b: success rate and guarantee compliance, 100 seeds per preset ------------------------------

TEST(SLOW_GenerationSuccessRate, Default100Seeds)      { const Sweep s = RunSweep("default", 100);     PrintSweep("1a/1b", "default", s);     EXPECT_GE(s.success, 95); EXPECT_EQ(s.guaranteeViolations, 0); }
TEST(SLOW_GenerationSuccessRate, Continental100Seeds)  { const Sweep s = RunSweep("continental", 100); PrintSweep("1a/1b", "continental", s); EXPECT_GE(s.success, 95); EXPECT_EQ(s.guaranteeViolations, 0); }
TEST(SLOW_GenerationSuccessRate, Archipelago100Seeds)  { const Sweep s = RunSweep("archipelago", 100); PrintSweep("1a/1b", "archipelago", s); EXPECT_GE(s.success, 95); EXPECT_EQ(s.guaranteeViolations, 0); }
TEST(SLOW_GenerationSuccessRate, Highlands100Seeds)    { const Sweep s = RunSweep("highlands", 100);   PrintSweep("1a/1b", "highlands", s);   EXPECT_GE(s.success, 95); EXPECT_EQ(s.guaranteeViolations, 0); }

// Default-tier cheap subset of the same checks: every preset, 5 seeds; every success is compliant.
TEST(GenerationSystem, Test1b_SuccessfulWorldsSatisfyEveryRegisteredGuarantee)
{
    for (const char* preset : { "default", "continental", "archipelago", "highlands" })
    {
        const Sweep s = RunSweep(preset, 5);
        EXPECT_EQ(s.guaranteeViolations, 0) << preset;
        EXPECT_GE(s.success, 4) << preset << ": a 5-seed subset should almost always succeed";
    }
}

// ---- Test 2a / AC1: determinism, including seeds that trigger retries ----------------------------------------------------

TEST(GenerationSystem, Test2a_TenSeedsRegenerateByteForByteIncludingRetriedOnes)
{
    // A registered guarantee that depends on the placed world forces retries on some seeds (odd region counts).
    EvenRegionCountGuarantee even;
    SiteRegistry reg;
    ASSERT_TRUE(reg.RegisterGuarantee(&even));

    int retriedSeeds = 0;
    for (uint64_t seed = 1; seed <= 12; ++seed)
    {
        Gen a(&reg), b(&reg);
        ASSERT_TRUE(a.Regenerate(seed));
        ASSERT_TRUE(b.Regenerate(seed));
        const GenerationReport& ra = *a.s.LastReport();
        const GenerationReport& rb = *b.s.LastReport();
        EXPECT_TRUE(SameApartFromTiming(ra, rb)) << "seed " << seed;
        EXPECT_EQ(a.s.State(), b.s.State());
        EXPECT_TRUE(SameCellBytes(a.s.World(), b.s.World())) << "seed " << seed;   // biome, terrain and region of every cell
        EXPECT_TRUE(a.s.World().regions == b.s.World().regions) << "seed " << seed;
        EXPECT_TRUE(SameSites(a.s.World(), b.s.World())) << "seed " << seed;
        if (ra.attempts.Size() > 1) { ++retriedSeeds; }
    }
    EXPECT_GE(retriedSeeds, 1) << "Test 2a needs at least one seed that triggers retries";
}

TEST(GenerationSystem, Test2a_RetrySequenceDoesNotDependOnPriorSessionHistory)
{
    // The same outer seed gives the same report whether or not other generations ran before it on the same stream.
    EvenRegionCountGuarantee even;
    SiteRegistry reg;
    ASSERT_TRUE(reg.RegisterGuarantee(&even));
    Gen fresh(&reg);
    ASSERT_TRUE(fresh.Regenerate(7));

    Gen busy(&reg);
    for (uint64_t other : { 3ull, 5ull, 11ull }) { ASSERT_TRUE(busy.Regenerate(other)); }
    ASSERT_TRUE(busy.Regenerate(7));
    // worldRevision is session history (1 vs 5), not part of the generation's content: compare with it neutralised.
    GenerationReport rf = *fresh.s.LastReport(), rb = *busy.s.LastReport();
    EXPECT_EQ(rf.worldRevision, fresh.s.Revision());
    EXPECT_EQ(rb.worldRevision, busy.s.Revision());
    rf.worldRevision = rb.worldRevision = 0;
    EXPECT_TRUE(SameApartFromTiming(rf, rb));
    EXPECT_TRUE(SameCellBytes(fresh.s.World(), busy.s.World()));
}

// ---- Test 5a: forced guarantee failure -> retry with sub-seed -> success, or fallthrough -----------------------------------

TEST(GenerationSystem, Test5a_FirstAttemptViolatesAGuaranteeThenTheSubSeedAttemptSucceeds)
{
    FailFirstNGuarantee failOnce("Test.FailOnAttemptZero", 1);
    SiteRegistry reg;
    ASSERT_TRUE(reg.RegisterGuarantee(&failOnce));
    Gen g(&reg);
    ASSERT_TRUE(g.Regenerate(21));

    const GenerationReport& rep = *g.s.LastReport();
    ASSERT_EQ(rep.attempts.Size(), 2u);
    EXPECT_FALSE(rep.attempts[0].passed);
    EXPECT_EQ(rep.attempts[0].failedGuarantees[0], StringCRC("Test.FailOnAttemptZero").Value());
    EXPECT_TRUE(rep.attempts[1].passed);
    EXPECT_EQ(rep.attempts[1].subSeed, DeriveSubSeed(21ull, 1ull));
    EXPECT_EQ(g.s.State(), WorldGenState::Complete);

    // The kept world is exactly what Place produces on the same terrain with the attempt-1 sub-seed.
    WorldStructure expected = MakeTerrain(21, g.s.Shape());
    const TerrainRules rules = TestRules();
    const SiteRegistry empty;
    RandomStream rng(99);
    RegionsAndSites::Place(expected, empty, rules, DeriveSubSeed(21ull, 1ull), 1, rng, nullptr);
    EXPECT_TRUE(SameCellBytes(g.s.World(), expected));
    EXPECT_TRUE(g.s.World().regions == expected.regions);
    EXPECT_TRUE(SameSites(g.s.World(), expected));
}

TEST(GenerationSystem, Test5a_EngineeredNeverSatisfiedSeedFallsThroughWithoutReturningAViolatingWorld)
{
    FixedGuarantee never("Test.Never", false);
    SiteRegistry reg;
    ASSERT_TRUE(reg.RegisterGuarantee(&never));
    Gen g(&reg);
    ASSERT_TRUE(g.Regenerate(21, 5));

    const GenerationReport& rep = *g.s.LastReport();
    EXPECT_EQ(rep.outcome, GenerationOutcome::Unsatisfiable);
    EXPECT_EQ(rep.attempts.Size(), 5u);
    EXPECT_EQ(g.s.State(), WorldGenState::Failed);
    // No world that violates the guarantee is delivered: regions and sites are absent, never half-placed.
    EXPECT_FALSE(g.s.RegionsFresh());
    EXPECT_EQ(g.s.World().regions.regions.Size(), 0u);
    EXPECT_EQ(g.s.World().sites.Count(), 0);

    // Same outer seed: same sequence and the same outcome every time.
    Gen again(&reg);
    ASSERT_TRUE(again.Regenerate(21, 5));
    EXPECT_TRUE(SameApartFromTiming(rep, *again.s.LastReport()));
}

TEST(GenerationSystem, AC1_AtLeastThreeRetrySeedsGiveTheSameCountAndWorldEveryRun)
{
    EvenRegionCountGuarantee even;
    SiteRegistry reg;
    ASSERT_TRUE(reg.RegisterGuarantee(&even));
    int retried = 0;
    for (uint64_t seed = 1; seed <= 40 && retried < 3; ++seed)
    {
        Gen a(&reg);
        ASSERT_TRUE(a.Regenerate(seed));
        if (a.s.LastReport()->attempts.Size() < 2u) { continue; }
        ++retried;
        for (int run = 0; run < 2; ++run)
        {
            Gen b(&reg);
            ASSERT_TRUE(b.Regenerate(seed));
            EXPECT_EQ(b.s.LastReport()->attempts.Size(), a.s.LastReport()->attempts.Size()) << "seed " << seed;
            EXPECT_TRUE(SameCellBytes(a.s.World(), b.s.World())) << "seed " << seed;
        }
    }
    EXPECT_GE(retried, 3);
}

// ---- Test 4 (timeout path), Test 5 (failure path) and Test 5a re-checked with the previous world kept ------------------------

#include <DiaCellLayers/Testing/LogCapture.h>
#include <Shared/WorldGeneration/GenerationGuard.h>
#include <cstring>
#include <stdexcept>

namespace {

struct SysClock : IGenerationClock
{
    double now = 5.0;
    double NowSeconds() const override { return now; }
};

// Passes or fails on demand; can drag the clock forward or throw.
class SysSwitchGuarantee : public IWorldGuarantee
{
public:
    Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC("Test.SysSwitch"); }
    GuaranteeResult Check(const ValidationContext&) const override
    {
        if (clock != nullptr) { clock->now += step; }
        if (throws) { throw std::runtime_error("injected generation error"); }
        GuaranteeResult r;
        r.idCrc = GetId().Value();
        r.satisfied = pass;
        if (!pass) { r.AddDiagnostic("engineered failure"); }
        return r;
    }
    bool pass = true;
    bool throws = false;
    SysClock* clock = nullptr;
    double step = 0.0;
};

bool SameShownWorld(const WorldGenSession& a, const WorldStructure& b)
{
    const WorldStructure& w = a.World();
    if (w.CellCount() != b.CellCount() || !(w.regions == b.regions) || w.sites.Count() != b.sites.Count()) { return false; }
    return std::memcmp(w.Cells(), b.Cells(), sizeof(Cell) * static_cast<size_t>(w.CellCount())) == 0;
}

struct SysGen
{
    TestSession s;
    RandomStream rng{ 1 };
    SiteRegistry registry;
    SysSwitchGuarantee sw;
    SysClock clock;
    SysGen()
    {
        registry.RegisterGuarantee(&sw);
        s.SetSiteRegistry(&registry);
        s.SetGenerationClock(&clock);
        Json::Value a(Json::objectValue);
        a["seed"] = 21;
        EXPECT_TRUE(s.Regenerate(a)["success"].asBool());
        EXPECT_TRUE(TickUntilIdle(s, rng));
        EXPECT_EQ(s.State(), WorldGenState::Complete);
    }
    void Regenerate(uint64_t seed, uint32_t maxRetry, float timeout)
    {
        Json::Value a(Json::objectValue);
        a["seed"] = static_cast<Json::UInt64>(seed);
        a["maxRetryAttempts"] = maxRetry;
        a["timeoutSeconds"] = timeout;
        EXPECT_TRUE(s.Regenerate(a)["success"].asBool());
        TickUntilIdle(s, rng);
    }
};

}

TEST(GenerationSystem, Test4_ForcedPastTheHardTimeoutAutoCancelsWithAMessageIdentifyingTimeout)
{
    Dia::CellLayers::Testing::LogCapture log("CoW");
    SysGen g;
    const WorldStructure before = g.s.World();
    const uint32_t revision = g.s.Revision();
    g.sw.pass = false;
    g.sw.clock = &g.clock;
    g.sw.step = 3.0;                                   // each attempt "takes" 3 s against a 5 s budget
    log.Clear();
    g.Regenerate(21, 64, 5.0f);

    const GenerationReport& rep = *g.s.LastReport();
    EXPECT_EQ(rep.outcome, GenerationOutcome::Timeout);
    EXPECT_STREQ(rep.failureMessage, "TIMEOUT: World generation exceeded 5.0 seconds");
    EXPECT_LT(rep.attempts.Size(), 64u);               // the timeout interrupted the retry loop
    // Discard-and-return: the attempt is gone, the previous world and revision stand, the stage stays open.
    EXPECT_TRUE(SameShownWorld(g.s, before));
    EXPECT_EQ(g.s.Revision(), revision);
    EXPECT_EQ(g.s.State(), WorldGenState::Failed);
    EXPECT_EQ(g.s.Status()["generation"]["outcome"].asString(), "Timeout");

    // Distinct from a cancel and from a failure, in the log and in the enum.
    EXPECT_EQ(log.Errors("TIMEOUT: World generation exceeded 5.0 seconds"), 1u);
    EXPECT_EQ(log.Errors("CANCEL"), 0u);
    EXPECT_EQ(log.Errors("FAILURE"), 0u);
    EXPECT_EQ(log.Count(Dia::CellLayers::Testing::LogCapture::Level::kInfo, "CANCEL"), 0u);
}

TEST(GenerationSystem, Test5_AGenerationErrorRoutesThroughDiscardAndReturnWithAMessageIdentifyingFailure)
{
    Dia::CellLayers::Testing::LogCapture log("CoW");
    SysGen g;
    const WorldStructure before = g.s.World();
    const uint32_t revision = g.s.Revision();
    g.sw.throws = true;                                // an error, not merely slowness
    log.Clear();
    g.Regenerate(21, 10, 30.0f);

    const GenerationReport& rep = *g.s.LastReport();
    EXPECT_EQ(rep.outcome, GenerationOutcome::Error);
    EXPECT_STREQ(rep.failureMessage, "FAILURE: World generation error: injected generation error");
    EXPECT_TRUE(SameShownWorld(g.s, before));
    EXPECT_EQ(g.s.Revision(), revision);
    EXPECT_EQ(g.s.State(), WorldGenState::Failed);

    EXPECT_EQ(log.Errors("FAILURE: World generation error: injected generation error"), 1u);
    EXPECT_EQ(log.Errors("TIMEOUT"), 0u);
    EXPECT_EQ(log.Errors("CANCEL"), 0u);

    // The session is still usable on the previous world: the next Regenerate (the error gone) succeeds.
    g.sw.throws = false;
    g.Regenerate(22, 10, 30.0f);
    EXPECT_EQ(g.s.LastReport()->outcome, GenerationOutcome::Success);
    EXPECT_EQ(g.s.Revision(), revision + 1);
}

TEST(GenerationSystem, Test5_AllFourFailureModesAreDistinguishable)
{
    std::string messages[4];
    GenerationOutcome outcomes[4];
    for (int mode = 0; mode < 4; ++mode)
    {
        SysGen g;
        if (mode == 0) { g.sw.pass = false; g.sw.clock = &g.clock; g.sw.step = 9.0; g.Regenerate(21, 10, 5.0f); }
        if (mode == 1) { g.sw.pass = false; g.Regenerate(21, 3, 30.0f); }
        if (mode == 2) { g.sw.throws = true; g.Regenerate(21, 3, 30.0f); }
        if (mode == 3)
        {
            g.sw.pass = false;
            Json::Value a(Json::objectValue); a["seed"] = 21; a["maxRetryAttempts"] = 10;
            ASSERT_TRUE(g.s.Regenerate(a)["success"].asBool());
            g.s.Tick(g.rng);
            ASSERT_TRUE(g.s.Cancel(Json::Value(Json::objectValue))["cancelled"].asBool());
        }
        messages[mode] = g.s.LastReport()->failureMessage;
        outcomes[mode] = g.s.LastReport()->outcome;
    }
    EXPECT_EQ(outcomes[0], GenerationOutcome::Timeout);
    EXPECT_EQ(outcomes[1], GenerationOutcome::Unsatisfiable);
    EXPECT_EQ(outcomes[2], GenerationOutcome::Error);
    EXPECT_EQ(outcomes[3], GenerationOutcome::Cancelled);
    EXPECT_EQ(messages[0].rfind("TIMEOUT", 0), 0u);
    EXPECT_EQ(messages[1].rfind("FAILURE", 0), 0u);
    EXPECT_EQ(messages[2].rfind("FAILURE", 0), 0u);
    EXPECT_EQ(messages[3].rfind("CANCEL", 0), 0u);
    EXPECT_NE(messages[1], messages[2]);               // the two FAILURE modes are told apart by their text
}

TEST(GenerationSystem, Test5a_ANeverSatisfiedSeedFallsThroughAndKeepsThePreviousWorldInsteadOfAViolatingOne)
{
    SysGen g;
    const WorldStructure before = g.s.World();
    const uint32_t revision = g.s.Revision();
    g.sw.pass = false;                                 // engineered to never satisfy the guarantee
    g.Regenerate(21, 5, 30.0f);

    const GenerationReport& rep = *g.s.LastReport();
    EXPECT_EQ(rep.outcome, GenerationOutcome::Unsatisfiable);
    EXPECT_EQ(rep.attempts.Size(), 5u);
    EXPECT_TRUE(SameShownWorld(g.s, before));          // never a world that violates the guarantee
    EXPECT_EQ(g.s.Revision(), revision);

    // Same outer seed: the same sequence and outcome every time.
    SysGen h;
    h.sw.pass = false;
    h.Regenerate(21, 5, 30.0f);
    EXPECT_TRUE(SameApartFromTiming(*g.s.LastReport(), *h.s.LastReport()));
}
