// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/timeout-failure-handling.md (AC1-AC8, Goal 5)
// GenerationGuard (timeout and cancel checks) and the failure modes of a GenerationRun: Timeout, Unsatisfiable, Error,
// Cancelled. Time is a fake clock advanced by the test doubles, so nothing sleeps.
#include <gtest/gtest.h>
#include <Shared/CoWStepSeedNames.h>
#include <Shared/CoWNodeKinds.h>
#include <DraoiHexWorldTerrain/GenerationGuard.h>
#include <DraoiHexWorldTerrain/GenerationRun.h>
#include <Shared/WorldGeneration/Testing/TerrainSliderDriver.h>
#include <DraoiHexWorldTerrain/Testing/TestGuarantees.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

#include <DiaCellLayers/Testing/LogCapture.h>
#include <DiaMaths/Core/RandomStream.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;
using namespace CoW::WorldGeneration::Testing;
using namespace Draoi::HexWorld::Testing;
using Dia::Maths::RandomStream;
using Level = Dia::CellLayers::Testing::LogCapture::Level;

namespace
{
    struct FakeClock : IGenerationClock
    {
        double now = 100.0;   // arbitrary origin: elapsed is relative to the guard's construction
        double NowSeconds() const override { return now; }
    };

    // Fails every validation and advances the clock: "this attempt took `step` seconds".
    class SlowFailGuarantee : public IWorldGuarantee
    {
    public:
        SlowFailGuarantee(FakeClock& clock, double step) : mClock(clock), mStep(step) {}
        Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC("Test.SlowFail"); }
        GuaranteeResult Check(const ValidationContext&) const override
        {
            mClock.now += mStep;
            GuaranteeResult r;
            r.idCrc = GetId().Value();
            r.satisfied = false;
            r.AddDiagnostic("slow attempt failed");
            return r;
        }
    private:
        FakeClock& mClock;
        double mStep;
    };

    class ThrowingGuarantee : public IWorldGuarantee
    {
    public:
        explicit ThrowingGuarantee(bool stdException) : mStd(stdException) {}
        Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC("Test.Throws"); }
        GuaranteeResult Check(const ValidationContext&) const override
        {
            if (mStd) { throw std::runtime_error("Test error"); }
            throw 42;
        }
    private:
        bool mStd;
    };

    // Requests a cancel on the guard once `afterAttempts` attempts have finished.
    struct CancelAfter : IGenerationObserver
    {
        CancelAfter(GenerationGuard*& guard, uint32_t afterAttempts) : g(guard), after(afterAttempts) {}
        void OnAttemptFinished(const AttemptRecord& r, uint32_t) override
        {
            if (r.attempt + 1 >= after && g != nullptr) { g->RequestCancel(); }
        }
        GenerationGuard*& g;
        uint32_t after;
    };

    struct Fixture
    {
        TerrainRules rules = TestRules();
        RandomStream rng{ 1 };
        CoWSiteRegistry registry;
        WorldStructure terrain;
        FakeClock clock;
        Fixture()
        {
            TerrainRunner runner;
            runner.Run(1, WorldShapeParams{}, GridDimensions{ 66, 42 });
            terrain = runner.World();
        }
        GenParams Params(uint32_t maxAttempts, float timeout) const
        {
            GenParams p;
            p.maxRetryAttempts = maxAttempts;
            p.timeoutSeconds = timeout;
            return p;
        }
    };

    bool StartsWith(const char* text, const char* prefix) { return std::strncmp(text, prefix, std::strlen(prefix)) == 0; }
}

// ---- GenerationGuard -------------------------------------------------------------------------------------------

TEST(GenerationGuard, ElapsedIsMeasuredFromConstructionOnTheInjectedClock)
{
    FakeClock clock;
    GenerationGuard guard(5.0f, &clock);
    EXPECT_DOUBLE_EQ(guard.ElapsedSeconds(), 0.0);
    clock.now += 2.5;
    EXPECT_DOUBLE_EQ(guard.ElapsedSeconds(), 2.5);
    EXPECT_FLOAT_EQ(guard.TimeoutSeconds(), 5.0f);
}

TEST(GenerationGuard, TimeoutIsStrictlyGreaterThanTheBudget)
{
    FakeClock clock;
    GenerationGuard guard(5.0f, &clock);
    GenerationOutcome reason = GenerationOutcome::Success;
    clock.now += 5.0;                                // exactly at the budget: not exceeded
    EXPECT_FALSE(guard.TimedOut());
    EXPECT_FALSE(guard.ShouldAbort(reason));
    clock.now += 0.001;                              // just past it
    EXPECT_TRUE(guard.TimedOut());
    EXPECT_TRUE(guard.ShouldAbort(reason));
    EXPECT_EQ(reason, GenerationOutcome::Timeout);
}

TEST(GenerationGuard, NoAbortWhileWithinBudgetAndNotCancelled)
{
    FakeClock clock;
    GenerationGuard guard(5.0f, &clock);
    GenerationOutcome reason = GenerationOutcome::Success;
    clock.now += 4.999;
    EXPECT_FALSE(guard.ShouldAbort(reason));
    EXPECT_EQ(reason, GenerationOutcome::Success);   // untouched when it does not abort
}

TEST(GenerationGuard, CancelIsCheckedBeforeTimeout)
{
    FakeClock clock;
    GenerationGuard guard(1.0f, &clock);
    clock.now += 10.0;                                // far past the budget
    guard.RequestCancel();
    GenerationOutcome reason = GenerationOutcome::Success;
    ASSERT_TRUE(guard.ShouldAbort(reason));
    EXPECT_EQ(reason, GenerationOutcome::Cancelled);
    EXPECT_TRUE(StartsWith(guard.AbortMessage(), "CANCEL"));
}

TEST(GenerationGuard, ZeroBudgetTimesOutAsSoonAsAnyTimePasses)
{
    FakeClock clock;
    GenerationGuard guard(0.0f, &clock);
    EXPECT_FALSE(guard.TimedOut());                   // elapsed 0 is not > 0
    clock.now += 1e-6;
    EXPECT_TRUE(guard.TimedOut());
}

TEST(GenerationGuard, HugeBudgetNeverTimesOutInPractice)
{
    FakeClock clock;
    GenerationGuard guard(kMaxTimeoutSeconds, &clock);
    clock.now += 3599.0;
    EXPECT_FALSE(guard.TimedOut());
}

TEST(GenerationGuard, MessagesAreDistinctAndNameTheBudgetNotTheMeasuredTime)
{
    FakeClock clock;
    GenerationGuard a(2.5f, &clock);
    clock.now += 7.123;
    GenerationOutcome reason;
    ASSERT_TRUE(a.ShouldAbort(reason));
    EXPECT_STREQ(a.AbortMessage(), "TIMEOUT: World generation exceeded 2.5 seconds");

    FakeClock clock2;
    GenerationGuard b(2.5f, &clock2);
    clock2.now += 99.0;
    ASSERT_TRUE(b.ShouldAbort(reason));
    EXPECT_STREQ(b.AbortMessage(), a.AbortMessage());   // deterministic: same budget, different elapsed

    GenerationGuard c(2.5f, &clock2);
    c.RequestCancel();
    ASSERT_TRUE(c.ShouldAbort(reason));
    EXPECT_STREQ(c.AbortMessage(), "CANCEL: World generation cancelled by user");
}

TEST(GenerationGuard, ForwardsAttemptNotificationsToTheChainedObserver)
{
    struct Counter : IGenerationObserver
    {
        void OnAttemptFinished(const AttemptRecord&, uint32_t) override { ++n; }
        int n = 0;
    } counter;
    GenerationGuard guard(5.0f, nullptr, &counter);
    AttemptRecord rec;
    guard.OnAttemptFinished(rec, 3);
    guard.OnAttemptFinished(rec, 3);
    EXPECT_EQ(counter.n, 2);
}

TEST(GenerationGuard, SteadyClockIsMonotonic)
{
    const double a = SteadyGenerationClock().NowSeconds();
    const double b = SteadyGenerationClock().NowSeconds();
    EXPECT_GE(b, a);
}

TEST(GenerationGuard, ClampTimeoutSeconds)
{
    EXPECT_FLOAT_EQ(ClampTimeoutSeconds(std::numeric_limits<float>::quiet_NaN()), kDefaultTimeoutSeconds);
    EXPECT_FLOAT_EQ(ClampTimeoutSeconds(-3.0f), 0.0f);
    EXPECT_FLOAT_EQ(ClampTimeoutSeconds(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ClampTimeoutSeconds(5.0f), 5.0f);
    EXPECT_FLOAT_EQ(ClampTimeoutSeconds(1.0e9f), kMaxTimeoutSeconds);
    EXPECT_FLOAT_EQ(ClampTimeoutSeconds(std::numeric_limits<float>::infinity()), kMaxTimeoutSeconds);
    EXPECT_FLOAT_EQ(GenParams{}.timeoutSeconds, kDefaultTimeoutSeconds);
}

// ---- GenerationRun failure modes (Feature 5 ACs) -----------------------------------------------------------------

TEST(GenerationTimeout, AC1_AnAttemptThatOverrunsTheBudgetEndsTheRunAtTheNextBoundary)
{
    Fixture f;
    SlowFailGuarantee slow(f.clock, 6.0);              // one attempt "takes" 6 s against a 5 s budget
    ASSERT_TRUE(f.registry.RegisterGuarantee(&slow));
    GenerationGuard guard(5.0f, &f.clock);
    GenerationRun run(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(10, 5.0f), f.rng, CoWStepSeedNames(), &guard);
    run.Run();
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Timeout);
    EXPECT_TRUE(StartsWith(run.Report().failureMessage, "TIMEOUT"));
    EXPECT_EQ(run.Report().attempts.Size(), 1u);       // the overrunning attempt finished; the next boundary stopped
    EXPECT_FALSE(run.HasAcceptedWorld());
}

TEST(GenerationTimeout, AC2_TimeoutWinsOverTheRetryBound)
{
    Fixture f;
    SlowFailGuarantee slow(f.clock, 1.0);              // 1 s per failing attempt, budget 5 s, bound 64
    ASSERT_TRUE(f.registry.RegisterGuarantee(&slow));
    GenerationGuard guard(5.0f, &f.clock);
    GenerationRun run(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(64, 5.0f), f.rng, CoWStepSeedNames(), &guard);
    run.Run();
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Timeout);
    EXPECT_EQ(run.Report().attempts.Size(), 6u);       // boundaries at elapsed 0..5 pass, 6 > 5 stops: not all 64
    EXPECT_LT(run.Report().attempts.Size(), 64u);
}

TEST(GenerationTimeout, AC3_ExhaustionIsUnsatisfiableAfterExactlyTheBoundWithAFailurePrefix)
{
    Fixture f;
    FixedGuarantee never("Test.Never", false);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&never));
    GenerationGuard guard(5.0f, &f.clock);
    GenerationRun run(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(5, 5.0f), f.rng, CoWStepSeedNames(), &guard);
    run.Run();
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Unsatisfiable);
    EXPECT_EQ(run.Report().attempts.Size(), 5u);
    EXPECT_STREQ(run.Report().failureMessage,
                 "FAILURE: World generation failed after 5 attempts (unsatisfiable seed or parameters)");
}

TEST(GenerationTimeout, AC4_AnExceptionEndsTheRunAsErrorWithItsMessage)
{
    Fixture f;
    ThrowingGuarantee boom(true);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&boom));
    GenerationGuard guard(5.0f, &f.clock);
    GenerationRun run(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(10, 5.0f), f.rng, CoWStepSeedNames(), &guard);
    run.Run();
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Error);
    EXPECT_STREQ(run.Report().failureMessage, "FAILURE: World generation error: Test error");
    ASSERT_EQ(run.Report().attempts.Size(), 1u);       // the attempt that threw is recorded
    EXPECT_FALSE(run.Report().attempts[0].passed);
    EXPECT_GE(run.Report().attempts[0].diagnosticCount, 1u);
    EXPECT_FALSE(run.HasAcceptedWorld());
}

TEST(GenerationTimeout, AC4_ANonStandardExceptionIsCaughtToo)
{
    Fixture f;
    ThrowingGuarantee boom(false);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&boom));
    GenerationRun run(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(10, 5.0f), f.rng, CoWStepSeedNames());
    run.Run();
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Error);
    EXPECT_STREQ(run.Report().failureMessage, "FAILURE: World generation error: unknown exception");
}

TEST(GenerationTimeout, AC5_CancelMidRunEndsItAtTheNextBoundary)
{
    Fixture f;
    FixedGuarantee never("Test.Never", false);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&never));
    GenerationGuard* guardPtr = nullptr;
    CancelAfter canceller(guardPtr, 1);                 // cancel once the first attempt has finished
    GenerationGuard guard(5.0f, &f.clock, &canceller);
    guardPtr = &guard;
    GenerationRun run(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(10, 5.0f), f.rng, CoWStepSeedNames(), &guard);
    run.Run();
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Cancelled);
    EXPECT_STREQ(run.Report().failureMessage, "CANCEL: World generation cancelled by user");
    EXPECT_EQ(run.Report().attempts.Size(), 1u);
    EXPECT_FALSE(run.HasAcceptedWorld());
}

TEST(GenerationTimeout, AC5_CancelBeforeTheFirstAttemptRunsNothing)
{
    Fixture f;
    GenerationGuard guard(5.0f, &f.clock);
    guard.RequestCancel();
    GenerationRun run(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(10, 5.0f), f.rng, CoWStepSeedNames(), &guard);
    run.Run();
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Cancelled);
    EXPECT_EQ(run.Report().attempts.Size(), 0u);
}

TEST(GenerationTimeout, CancelAfterTheRunSucceededChangesNothing)
{
    Fixture f;
    GenerationGuard guard(5.0f, &f.clock);
    GenerationRun run(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(10, 5.0f), f.rng, CoWStepSeedNames(), &guard);
    run.Run();
    ASSERT_EQ(run.Outcome(), GenerationOutcome::Success);
    guard.RequestCancel();
    EXPECT_FALSE(run.Step());                           // a finished run stays finished
    EXPECT_EQ(run.Outcome(), GenerationOutcome::Success);
    EXPECT_TRUE(run.HasAcceptedWorld());
    EXPECT_EQ(run.Report().failureMessage[0], '\0');
}

TEST(GenerationTimeout, AC6_EachFailureModeHasItsOwnPrefixLogLevelAndOutcome)
{
    std::set<std::string> messages;
    std::set<int> outcomes;

    {   // Timeout
        Dia::CellLayers::Testing::LogCapture log("HexWorld");
        Fixture f;
        SlowFailGuarantee slow(f.clock, 6.0);
        ASSERT_TRUE(f.registry.RegisterGuarantee(&slow));
        GenerationGuard guard(5.0f, &f.clock);
        GenerationRun run(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(10, 5.0f), f.rng, CoWStepSeedNames(), &guard);
        run.Run();
        EXPECT_EQ(log.Errors("TIMEOUT: World generation exceeded 5.0 seconds"), 1u);
        EXPECT_EQ(log.Errors("FAILURE"), 0u);
        messages.insert(run.Report().failureMessage); outcomes.insert(static_cast<int>(run.Outcome()));
    }
    {   // Unsatisfiable
        Dia::CellLayers::Testing::LogCapture log("HexWorld");
        Fixture f;
        FixedGuarantee never("Test.Never", false);
        ASSERT_TRUE(f.registry.RegisterGuarantee(&never));
        GenerationRun run(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(3, 5.0f), f.rng, CoWStepSeedNames());
        run.Run();
        EXPECT_EQ(log.Errors("FAILURE: World generation failed after 3 attempts"), 1u);
        EXPECT_EQ(log.Errors("TIMEOUT"), 0u);
        messages.insert(run.Report().failureMessage); outcomes.insert(static_cast<int>(run.Outcome()));
    }
    {   // Error
        Dia::CellLayers::Testing::LogCapture log("HexWorld");
        Fixture f;
        ThrowingGuarantee boom(true);
        ASSERT_TRUE(f.registry.RegisterGuarantee(&boom));
        GenerationRun run(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(3, 5.0f), f.rng, CoWStepSeedNames());
        run.Run();
        EXPECT_EQ(log.Errors("FAILURE: World generation error: Test error"), 1u);
        messages.insert(run.Report().failureMessage); outcomes.insert(static_cast<int>(run.Outcome()));
    }
    {   // Cancel: info, not an error
        Dia::CellLayers::Testing::LogCapture log("HexWorld");
        Fixture f;
        GenerationGuard guard(5.0f, &f.clock);
        guard.RequestCancel();
        GenerationRun run(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(3, 5.0f), f.rng, CoWStepSeedNames(), &guard);
        run.Run();
        EXPECT_EQ(log.Count(Level::kInfo, "CANCEL: World generation cancelled by user"), 1u);
        EXPECT_EQ(log.Errors(), 0u);
        messages.insert(run.Report().failureMessage); outcomes.insert(static_cast<int>(run.Outcome()));
    }
    EXPECT_EQ(messages.size(), 4u);
    EXPECT_EQ(outcomes.size(), 4u);
}

TEST(GenerationTimeout, AC7_NoFailureModeReturnsAWorld)
{
    for (int mode = 0; mode < 4; ++mode)
    {
        Fixture f;
        SlowFailGuarantee slow(f.clock, 6.0);
        FixedGuarantee never("Test.Never", false);
        ThrowingGuarantee boom(true);
        GenerationGuard guard(5.0f, &f.clock);
        if (mode == 0) { ASSERT_TRUE(f.registry.RegisterGuarantee(&slow)); }
        if (mode == 1) { ASSERT_TRUE(f.registry.RegisterGuarantee(&never)); }
        if (mode == 2) { ASSERT_TRUE(f.registry.RegisterGuarantee(&boom)); }
        if (mode == 3) { guard.RequestCancel(); }
        WorldStructure out;
        const GenerationReport rep = GenerateWorld(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(2, 5.0f), f.rng, CoWStepSeedNames(), guard, &out);
        EXPECT_NE(rep.outcome, GenerationOutcome::Success) << "mode " << mode;
        EXPECT_EQ(out.CellCount(), 0) << "mode " << mode;     // the caller's world was never written
        EXPECT_EQ(rep.worldRevision, 0u) << "mode " << mode;
        EXPECT_NE(rep.failureMessage[0], '\0') << "mode " << mode;
    }
}

TEST(GenerationTimeout, AC8_TheSameSeedSucceedsFastAndTimesOutSlow)
{
    // Informational: timeout is wall-clock based, so the same seed can pass or fail depending on speed.
    Fixture fast;
    GenerationGuard fastGuard(5.0f, &fast.clock);
    EXPECT_EQ(GenerateWorld(fast.terrain, fast.registry, fast.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, fast.Params(10, 5.0f), fast.rng, CoWStepSeedNames(), fastGuard).outcome,
              GenerationOutcome::Success);

    Fixture slow;
    SlowFailGuarantee drag(slow.clock, 6.0);
    ASSERT_TRUE(slow.registry.RegisterGuarantee(&drag));
    GenerationGuard slowGuard(5.0f, &slow.clock);
    EXPECT_EQ(GenerateWorld(slow.terrain, slow.registry, slow.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, slow.Params(10, 5.0f), slow.rng, CoWStepSeedNames(), slowGuard).outcome,
              GenerationOutcome::Timeout);
}

TEST(GenerationTimeout, AGuardedRunWithinBudgetIsIdenticalToAnUnguardedOne)
{
    Fixture a;
    Fixture b;
    FailFirstNGuarantee flakyA("Test.Flaky", 2);
    FailFirstNGuarantee flakyB("Test.Flaky", 2);
    ASSERT_TRUE(a.registry.RegisterGuarantee(&flakyA));
    ASSERT_TRUE(b.registry.RegisterGuarantee(&flakyB));
    GenerationGuard guard(5.0f, &b.clock);
    GenerationRun unguarded(a.terrain, a.registry, a.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 7, a.Params(10, 5.0f), a.rng, CoWStepSeedNames());
    GenerationRun guarded(b.terrain, b.registry, b.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 7, b.Params(10, 5.0f), b.rng, CoWStepSeedNames(), &guard);
    unguarded.Run();
    guarded.Run();
    ASSERT_EQ(unguarded.Outcome(), GenerationOutcome::Success);
    EXPECT_TRUE(SameApartFromTiming(unguarded.Report(), guarded.Report()));
}

TEST(GenerationTimeout, FailureMessagesAreDeterministicAndPartOfTheReportComparison)
{
    auto timedOut = [](double step)
    {
        Fixture f;
        SlowFailGuarantee slow(f.clock, step);
        EXPECT_TRUE(f.registry.RegisterGuarantee(&slow));
        GenerationGuard guard(5.0f, &f.clock);
        GenerationRun run(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(10, 5.0f), f.rng, CoWStepSeedNames(), &guard);
        run.Run();
        return run.Report();
    };
    const GenerationReport a = timedOut(6.0);
    const GenerationReport b = timedOut(9.0);           // different measured time, same budget
    EXPECT_TRUE(SameApartFromTiming(a, b));
    GenerationReport c = a;
    c.SetFailureMessage("CANCEL: other");
    EXPECT_FALSE(SameApartFromTiming(a, c));
}

TEST(GenerationTimeout, TheBlockingGenerateWorldEnforcesParamsTimeoutOnTheSteadyClock)
{
    Fixture f;
    FixedGuarantee never("Test.Never", false);
    ASSERT_TRUE(f.registry.RegisterGuarantee(&never));
    // A zero budget is exceeded as soon as any real time has passed since the guard was built, so by the second
    // boundary at the latest (the first attempt's Place and validation take far longer than the clock resolution).
    const GenerationReport rep = GenerateWorld(f.terrain, f.registry, f.rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), 1, f.Params(64, 0.0f), f.rng, CoWStepSeedNames());
    EXPECT_EQ(rep.outcome, GenerationOutcome::Timeout);
    EXPECT_LE(rep.attempts.Size(), 1u);
    EXPECT_TRUE(StartsWith(rep.failureMessage, "TIMEOUT"));
}
