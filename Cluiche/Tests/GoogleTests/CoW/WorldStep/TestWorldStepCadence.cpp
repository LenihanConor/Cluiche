// World Step Feature 2: cadence, retry of a missed week, apply timing, stall, catch-up, synchronous mode and the
// deferred-equals-synchronous oracle (module-and-cadence.md AC 6-15, 24a; plan tasks 7-12, 18).
#include "WorldStepTestRig.h"

#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/JobStepLauncher.h>
#include <DiaThreading/JobSystem.h>
#include <DiaObservation/Health/IHealthReporter.h>

using namespace WsTest;
using Dia::Observation::Health::HealthStatus;
using Dia::DeferredStep::StepState;
using Dia::DeferredStep::Testing::LogCapture;
using Level = Dia::Observation::Log::LogLevel;

namespace
{
    const StringCRC kWorld("world");

    int ExactWeekDaysIn(uint64_t first, uint64_t last)
    {
        int n = 0;
        for (uint64_t d = first; d <= last; ++d)
        {
            if (CoW::ResolveBand(d) == CoW::TimeBand::Week) ++n;
        }
        return n;
    }

    // Two real-shaped stages (the second reads the first's committed state) registered for the oracle runs.
    struct OracleRig : Harness
    {
        OracleStage weather{ 64, 3, 11u };
        OracleStage ecology{ 64, 2, 29u, &weather };

        explicit OracleRig(bool deferred, unsigned k = 7) : Harness(Config("weather,ecology", k, deferred))
        {
            EXPECT_TRUE(module.RegisterStage(StringCRC("weather"), weather));
            EXPECT_TRUE(module.RegisterStage(StringCRC("ecology"), ecology));
        }
        ~OracleRig()
        {
            module.UnregisterStage(StringCRC("weather"));
            module.UnregisterStage(StringCRC("ecology"));
        }

        void RunUntilApplies(uint64_t applies, uint64_t maxDay = 2000)
        {
            for (uint64_t d = 1; d <= maxDay && module.ApplyCount() < applies; ++d)
                StepDay(d);
        }
    };
}

// ---- AC 6: exactly 52 kicks over 52 weeks, including the four boundary weeks -------------------------------------

TEST(WorldStepCadence, FiftyTwoWeeksIssueExactlyFiftyTwoKicks)
{
    Rig rig;
    rig.Steps(1, 364);
    EXPECT_EQ(rig.module.KickCount(), 52u);
    EXPECT_EQ(rig.scheduler.GetStats().kicks, 52u);
    EXPECT_EQ(rig.launcher.GetLaunchCount(), 52u);
    ASSERT_EQ(rig.kickDays.size(), 52u);
    for (size_t i = 0; i < rig.kickDays.size(); ++i)
        EXPECT_EQ(rig.kickDays[i], 7u * (i + 1));
    EXPECT_EQ(rig.module.LateKicks(), 0u);   // with k = 7 a week always finds the channel free
    EXPECT_EQ(rig.module.LostWeeks(), 0u);
}

TEST(WorldStepCadence, TheFourBoundaryWeeksAreNotSkipped)
{
    // A `band == Week` gate would issue 48. The test proves the data distinguishes 48 from 52, then the module issues 52.
    EXPECT_EQ(ExactWeekDaysIn(1, 364), 48);
    Rig rig;
    rig.Steps(1, 364);
    EXPECT_NE(rig.module.KickCount(), 48u);
    EXPECT_EQ(rig.module.KickCount(), 52u);
    // Season (91, 182, 273) and Year (364) weeks kicked.
    for (uint64_t boundary : { 91u, 182u, 273u, 364u })
    {
        EXPECT_NE(CoW::ResolveBand(boundary), CoW::TimeBand::Week);
        bool kicked = false;
        for (uint64_t d : rig.kickDays) kicked = kicked || d == boundary;
        EXPECT_TRUE(kicked) << "boundary day " << boundary;
    }
}

TEST(WorldStepCadence, DecadeAndCenturyWeeksAlsoKick)
{
    CoW::SimulationTick decade = MakeTick(3640);
    CoW::SimulationTick century = MakeTick(36400);
    ASSERT_EQ(decade.band, CoW::TimeBand::Decade);
    ASSERT_EQ(century.band, CoW::TimeBand::Century);
    {
        Rig rig;
        rig.StepTick(decade);
        EXPECT_EQ(rig.module.KickCount(), 1u);
    }
    {
        Rig rig;
        rig.StepTick(century);
        EXPECT_EQ(rig.module.KickCount(), 1u);
    }
}

TEST(WorldStepCadence, NonWeekDaysNeverKick)
{
    Rig rig;
    rig.Steps(1, 6);
    EXPECT_EQ(rig.module.KickCount(), 0u);
    EXPECT_EQ(rig.scheduler.GetStats().kicks, 0u);
}

// ---- AC 7: no kick before AllReady, logged once ------------------------------------------------------------------

TEST(WorldStepCadence, NoKickBeforeReadyAndNotReadyIsLoggedOnce)
{
    LogCapture logs("WST");
    Rig rig;
    rig.ecology.SetReady(false);
    rig.Steps(1, 30);   // weeks 7, 14, 21, 28 pass while not ready
    EXPECT_EQ(rig.module.KickCount(), 0u);
    EXPECT_EQ(rig.scheduler.GetStats().kicks, 0u);
    EXPECT_EQ(logs.Infos("not ready"), 1u);   // once, not once per tick
    EXPECT_EQ(logs.Infos("now ready"), 0u);

    rig.ecology.SetReady(true);
    rig.Steps(31, 35);
    EXPECT_EQ(rig.module.KickCount(), 1u);   // weeks that passed unready are skipped, not queued
    EXPECT_EQ(rig.kickDays.front(), 35u);
    EXPECT_EQ(logs.Infos("now ready"), 1u);
    EXPECT_EQ(logs.Infos("not ready"), 1u);
}

TEST(WorldStepCadence, FlippingToNotReadyAgainLogsAgainOnlyOnTheTransition)
{
    LogCapture logs("WST");
    Rig rig;
    rig.Steps(1, 7);
    rig.weather.SetReady(false);
    rig.Steps(8, 40);
    EXPECT_EQ(logs.Infos("not ready"), 1u);
    rig.weather.SetReady(true);
    rig.Steps(41, 42);
    EXPECT_EQ(logs.Infos("now ready"), 2u);   // first ready, then ready again
}

// ---- AC 8 / 9: retry of a missed week -----------------------------------------------------------------------------

TEST(WorldStepCadence, DueWeekWhileChannelInFlightIsRetriedWithMissedDayAndNewApplyKey)
{
    LogCapture logs("WST");
    Rig rig(Config("weather,ecology", 10, true));
    rig.Steps(1, 14);   // week 7 kicks (applies 17); week 14 finds the channel in flight
    EXPECT_EQ(rig.module.KickCount(), 1u);
    EXPECT_TRUE(rig.module.HasPendingWeek());
    EXPECT_EQ(rig.module.PendingWeekDay(), 14);
    EXPECT_EQ(rig.module.LateKicks(), 1u);
    EXPECT_EQ(logs.Warnings("week 14"), 1u);
    EXPECT_EQ(rig.module.GetHealthReporter().Report().status, HealthStatus::kDegraded);
    EXPECT_EQ(rig.module.LostWeeks(), 0u);

    rig.Steps(15, 16);   // still in flight: retried each tick, no second warning
    EXPECT_TRUE(rig.module.HasPendingWeek());
    EXPECT_EQ(rig.module.LateKicks(), 1u);
    EXPECT_EQ(logs.Warnings("week 14"), 1u);

    rig.Step(17);   // week 7's step applies, then the pending week kicks with the current day as its kick key
    EXPECT_FALSE(rig.module.HasPendingWeek());
    EXPECT_EQ(rig.module.KickCount(), 2u);
    EXPECT_EQ(rig.module.LastKickDay(), 17u);
    Dia::DeferredStep::StepKey applyKey = 0;
    ASSERT_TRUE(rig.scheduler.GetApplyKey(kWorld, applyKey));
    EXPECT_EQ(applyKey, 27u);   // apply_after_steps is counted from the actual kick
    EXPECT_EQ(logs.Infos("3 day(s) late"), 1u);

    const std::vector<StepKey> expected = { 7, 14 };   // every stage receives the missed day, not the retry day
    EXPECT_EQ(rig.CaptureKeys("weather"), expected);
    EXPECT_EQ(rig.CaptureKeys("ecology"), expected);
}

TEST(WorldStepCadence, SecondDueWeekWhilePendingIsDroppedWithAnErrorAndTheOlderWeekKicksFirst)
{
    LogCapture logs("WST");
    Rig rig(Config("weather,ecology", 20, true));
    rig.Steps(1, 21);   // 7 kicks (applies 27), 14 pending, 21 dropped
    EXPECT_EQ(rig.module.LateKicks(), 1u);
    EXPECT_EQ(rig.module.LostWeeks(), 1u);
    EXPECT_EQ(rig.module.PendingWeekDay(), 14);   // the older week keeps its place
    EXPECT_EQ(logs.Count(Level::kError, "dropped"), 1u);
    EXPECT_EQ(rig.module.KickCount(), 1u);

    rig.Steps(22, 27);   // channel frees at 27: the pending week kicks first
    EXPECT_EQ(rig.module.KickCount(), 2u);
    const std::vector<StepKey> expected = { 7, 14 };
    EXPECT_EQ(rig.CaptureKeys("weather"), expected);
    EXPECT_FALSE(rig.module.HasPendingWeek());
}

TEST(WorldStepCadence, RetryLaunchingAndANewDueWeekInTheSameTickMakesTheNewWeekPending)
{
    // k = 14: week 7 applies at 21. Week 14 goes pending; at 21 the channel frees, the retry launches, and the
    // new week 21 finds the channel busy again, so it becomes the pending week. Order is preserved.
    Rig rig(Config("weather,ecology", 14, true));
    rig.Steps(1, 21);
    EXPECT_EQ(rig.module.KickCount(), 2u);          // 7, and the retry of 14 at day 21
    EXPECT_EQ(rig.module.LastKickDay(), 21u);
    EXPECT_TRUE(rig.module.HasPendingWeek());
    EXPECT_EQ(rig.module.PendingWeekDay(), 21);
    EXPECT_EQ(rig.module.LateKicks(), 2u);
    EXPECT_EQ(rig.module.LostWeeks(), 0u);

    rig.Steps(22, 35);   // the retried step applies at 35, then 21 retries (and week 35 goes pending)
    const std::vector<StepKey> expected = { 7, 14, 21 };
    EXPECT_EQ(rig.CaptureKeys("weather"), expected);
}

TEST(WorldStepCadence, KickRetryNeverLowersTheKickKey)
{
    Rig rig(Config("weather,ecology", 10, true));
    // k = 10 against a 7-day cadence backlogs by design: the first genuinely dropped week is day 35 (week 28 is still
    // pending then), so stop before it. Every retry up to here must be accepted.
    rig.Steps(1, 34);
    EXPECT_GT(rig.module.LateKicks(), 0u);
    // Every retried kick used the current day; a lower key would have been rejected and counted as a lost week.
    EXPECT_EQ(rig.module.LostWeeks(), 0u);
    ASSERT_GE(rig.kickDays.size(), 3u);
    for (size_t i = 1; i < rig.kickDays.size(); ++i)
        EXPECT_GT(rig.kickDays[i], rig.kickDays[i - 1]);
}

// ---- amendment 3: one test per non-launched KickResult -------------------------------------------------------------

TEST(WorldStepCadence, UnknownChannelReRegistersAndLosesTheWeek)
{
    LogCapture logs("WST");
    Rig rig;
    rig.Steps(1, 6);
    rig.scheduler.Unregister(kWorld);   // behind the module's back
    ASSERT_EQ(rig.scheduler.GetChannelCount(), 0u);
    rig.Step(7);
    EXPECT_EQ(rig.module.KickCount(), 0u);
    EXPECT_EQ(rig.module.LostWeeks(), 1u);
    EXPECT_EQ(logs.Count(Level::kError, "not registered"), 1u);
    EXPECT_EQ(rig.scheduler.GetChannelCount(), 1u);   // re-registered
    EXPECT_TRUE(rig.module.IsChannelRegistered());
    rig.Steps(8, 14);
    EXPECT_EQ(rig.module.KickCount(), 1u);   // the channel is usable again
}

TEST(WorldStepCadence, NonMonotonicKickKeyLosesTheWeekWithAnErrorAndADebugAssert)
{
    AssertCapture asserts;
    LogCapture logs("WST");
    Rig rig;
    rig.Steps(1, 14);                 // kicks at 7 and 14 (floor 14)
    rig.weather.SetReady(false);
    rig.Step(21);                     // the step applies, no kick (not ready)
    rig.weather.SetReady(true);
    ASSERT_EQ(rig.scheduler.GetState(kWorld), StepState::kIdle);
    rig.StepTick(MakeTick(7));        // a replayed week behind the floor
    EXPECT_EQ(rig.module.LostWeeks(), 1u);
    EXPECT_EQ(logs.Count(Level::kError, "lower than the previous kick"), 1u);
    EXPECT_FALSE(rig.module.HasPendingWeek());
    EXPECT_EQ(rig.module.GetCurrentKey(), 21u);   // the key itself never moved backwards
    (void)asserts;
#ifdef DEBUG
    EXPECT_GE(asserts.Count(), 1);
#endif
}

TEST(WorldStepCadence, ApplyKeyBeforeKickKeyLosesTheWeekWithAnErrorAndADebugAssert)
{
    AssertCapture asserts;
    LogCapture logs("WST");
    Rig rig;
    CoW::SimulationTick t;
    t.day = 0xFFFFFFFFFFFFFFFCull;   // day + 7 wraps, so the apply key lands before the kick key
    t.band = CoW::TimeBand::Week;
    rig.StepTick(t);
    EXPECT_EQ(rig.module.KickCount(), 0u);
    EXPECT_EQ(rig.module.LostWeeks(), 1u);
    EXPECT_EQ(logs.Count(Level::kError, "before kick key"), 1u);
    (void)asserts;
#ifdef DEBUG
    EXPECT_GE(asserts.Count(), 1);
#endif
}

// ---- AC 10: applies at exactly kick day + k, even when finished early ----------------------------------------------

TEST(WorldStepCadence, StepAppliesAtExactlyKickDayPlusKEvenWhenItFinishedEarly)
{
    Rig rig;   // autoComplete: the step is finished one frame after the kick
    rig.Steps(1, 13);
    EXPECT_EQ(rig.Count(CallPhase::Commit), 0u);   // not before 14
    EXPECT_TRUE(rig.commitDays.empty());
    rig.Step(14);
    ASSERT_EQ(rig.commitDays.size(), 1u);
    EXPECT_EQ(rig.commitDays[0], 14u);
    EXPECT_EQ(rig.module.LastApplyKey(), 14u);
    EXPECT_EQ(rig.scheduler.GetStats().stalls, 0u);
}

TEST(WorldStepCadence, EveryAppliedStepLandsOnItsKickDayPlusK)
{
    Rig rig(Config("weather,ecology", 9, true));
    rig.Steps(1, 120);
    ASSERT_FALSE(rig.commitDays.empty());
    // Each commit day minus 9 is a day on which a step was launched.
    for (uint64_t commitDay : rig.commitDays)
    {
        bool matched = false;
        for (uint64_t kickDay : rig.kickDays) matched = matched || (kickDay + 9 == commitDay);
        EXPECT_TRUE(matched) << "commit on day " << commitDay;
    }
}

// ---- AC 11: a late step stalls SimPU, then applies ---------------------------------------------------------------

TEST(WorldStepCadence, StepStillRunningAtItsApplyKeyStallsThenAppliesAndIsReported)
{
    LogCapture logs("WST");
    Rig rig;
    rig.autoComplete = false;   // the step only runs when the scheduler has to wait for it
    const int64_t stallsBefore = CounterValue("cow.worldstep.stalls");
    rig.Steps(1, 13);
    EXPECT_EQ(rig.Count(CallPhase::Run), 0u);
    rig.Step(14);
    EXPECT_EQ(rig.scheduler.GetStats().stalls, 1u);
    EXPECT_EQ(rig.Count(CallPhase::Run, "weather"), 1u);
    EXPECT_EQ(rig.Count(CallPhase::Commit, "weather"), 1u);   // applied after the wait
    EXPECT_EQ(rig.commitDays.front(), 14u);
    EXPECT_EQ(logs.Warnings("stalled"), 1u);
    EXPECT_EQ(CounterValue("cow.worldstep.stalls"), stallsBefore + 1);
    EXPECT_EQ(rig.module.GetHealthReporter().Report().status, HealthStatus::kDegraded);
    EXPECT_GE(rig.module.LastStallMs(), 0.0);
}

TEST(WorldStepCadence, HealthRecoversAfterSixteenCleanAppliedStepsFollowingAStall)
{
    Rig rig;
    rig.autoComplete = false;
    rig.Steps(1, 14);   // the first step stalls at its apply key 14 (applied step 1)
    ASSERT_EQ(rig.scheduler.GetStats().stalls, 1u);
    EXPECT_EQ(rig.module.GetHealthReporter().Report().status, HealthStatus::kDegraded);
    rig.autoComplete = true;   // the step kicked at 14 still needs completing; every later one finishes early
    rig.Steps(15, 119);        // applied step 16 lands at 119
    ASSERT_EQ(rig.module.ApplyCount(), 16u);
    EXPECT_EQ(rig.module.GetHealthReporter().Report().status, HealthStatus::kDegraded);   // the stall is still inside the window
    rig.Steps(120, 126);       // applied step 17 lands at 126: the stall has left the window of 16
    ASSERT_EQ(rig.module.ApplyCount(), 17u);
    EXPECT_EQ(rig.module.GetHealthReporter().Report().status, HealthStatus::kOK);
    EXPECT_EQ(rig.scheduler.GetStats().stalls, 1u);   // never stalled again
}

// ---- AC 12: catch-up (several sim steps in one real frame) ----------------------------------------------------------

TEST(WorldStepCadence, CatchUpFramesNeverSlipTheApplyKey)
{
    // Frames of three days each: the key jumps by three, so a step due at 14 applies in the frame whose key is 15,
    // yet its apply key stays 14.
    Rig rig;
    uint64_t day = 0;
    for (int frame = 0; frame < 15; ++frame)
    {
        for (int i = 0; i < 3; ++i)
            rig.module.OnSimulationTick(MakeTick(++day));
        rig.scheduler.Tick(rig.module.GetCurrentKey());
        rig.module.CallUpdate();
        rig.launcher.CompleteAll();
        rig.Harvest();
    }
    EXPECT_EQ(day, 45u);
    EXPECT_EQ(rig.module.KickCount(), 6u);       // weeks 7..42
    EXPECT_EQ(rig.module.LateKicks(), 0u);
    EXPECT_EQ(rig.module.LostWeeks(), 0u);
    EXPECT_EQ(rig.module.ApplyCount(), 5u);      // 7..35 applied (at 14..42); 42 still in flight
    EXPECT_EQ(rig.module.LastApplyKey(), 42u);   // the apply key is kick + 7, never the (later) frame key
    // Applies happen more often per frame than days, never in larger batches: at most one commit per frame.
    EXPECT_EQ(rig.Count(CallPhase::Commit, "weather"), 5u);
}

TEST(WorldStepCadence, TwoDueWeeksInOneFrameSecondBecomesPendingAndKicksNextFrame)
{
    Rig rig;
    for (uint64_t d = 1; d <= 6; ++d) rig.Step(d);
    rig.module.OnSimulationTick(MakeTick(7));
    rig.module.OnSimulationTick(MakeTick(14));
    rig.scheduler.Tick(rig.module.GetCurrentKey());
    rig.module.CallUpdate();
    rig.launcher.CompleteAll();
    rig.Harvest();
    EXPECT_EQ(rig.module.KickCount(), 1u);
    EXPECT_EQ(rig.module.PendingWeekDay(), 14);
    EXPECT_EQ(rig.module.LateKicks(), 1u);
    EXPECT_EQ(rig.module.LostWeeks(), 0u);

    rig.Step(15);   // week 7's step applies (apply key 14), then 14 retries
    const std::vector<StepKey> expected = { 7, 14 };
    EXPECT_EQ(rig.CaptureKeys("weather"), expected);
}

// ---- AC 13: every stage receives the same key -----------------------------------------------------------------------

TEST(WorldStepCadence, EveryStageReceivesTheSameCaptureKeyEachStep)
{
    Rig rig;
    rig.Steps(1, 70);
    const std::vector<StepKey> w = rig.CaptureKeys("weather");
    const std::vector<StepKey> e = rig.CaptureKeys("ecology");
    ASSERT_EQ(w.size(), 10u);
    EXPECT_EQ(w, e);
    EXPECT_EQ(w, rig.kickDays);   // the kick day, when no week was late
}

// ---- AC 14: synchronous mode ----------------------------------------------------------------------------------------

TEST(WorldStepCadence, SynchronousModeRunsCaptureRunCommitInlineInDeclaredOrderWithNoScheduler)
{
    Rig rig(Config("weather,ecology", 7, false));
    rig.Steps(1, 7);
    ASSERT_EQ(rig.history.size(), 6u);
    const CallPhase phases[6] = { CallPhase::Capture, CallPhase::Capture, CallPhase::Run, CallPhase::Run, CallPhase::Commit, CallPhase::Commit };
    const char* stages[6] = { "weather", "ecology", "weather", "ecology", "weather", "ecology" };
    for (int i = 0; i < 6; ++i)
    {
        EXPECT_EQ(rig.history[i].phase, phases[i]) << i;
        EXPECT_EQ(rig.history[i].stageId, StringCRC(stages[i])) << i;
    }
    EXPECT_EQ(rig.history[0].key, 7u);
    EXPECT_EQ(rig.module.SyncStepCount(), 1u);
    EXPECT_EQ(rig.module.KickCount(), 0u);
    EXPECT_EQ(rig.scheduler.GetChannelCount(), 0u);   // no channel
    EXPECT_EQ(rig.scheduler.GetStats().kicks, 0u);    // no kick
    EXPECT_EQ(rig.launcher.GetLaunchCount(), 0u);
    EXPECT_FALSE(rig.module.IsChannelRegistered());
    EXPECT_EQ(rig.module.LateKicks(), 0u);
}

TEST(WorldStepCadence, SynchronousModeStepsEveryWeekIncludingTheBoundaryWeeks)
{
    Rig rig(Config("weather,ecology", 7, false));
    rig.Steps(1, 364);
    EXPECT_EQ(rig.module.SyncStepCount(), 52u);
    EXPECT_EQ(rig.module.ApplyCount(), 52u);
    EXPECT_EQ(rig.module.LateKicks(), 0u);
    EXPECT_EQ(rig.module.LostWeeks(), 0u);
}

TEST(WorldStepCadence, SynchronousModeStillWaitsForReadiness)
{
    Rig rig(Config("weather,ecology", 7, false));
    rig.ecology.SetReady(false);
    rig.Steps(1, 20);
    EXPECT_EQ(rig.module.SyncStepCount(), 0u);
    rig.ecology.SetReady(true);
    rig.Steps(21, 21);
    EXPECT_EQ(rig.module.SyncStepCount(), 1u);
}

// ---- AC 15: the determinism oracle ----------------------------------------------------------------------------------

TEST(WorldStepCadence, DeferredAndSynchronousRunsAreBitIdenticalAfterTheSameNumberOfAppliedSteps)
{
    OracleRig deferred(true);
    OracleRig sync(false);
    deferred.RunUntilApplies(20);
    sync.RunUntilApplies(20);
    ASSERT_EQ(deferred.module.ApplyCount(), 20u);
    ASSERT_EQ(sync.module.ApplyCount(), 20u);
    EXPECT_EQ(deferred.weather.Commits(), 20u);
    EXPECT_EQ(sync.weather.Commits(), 20u);
    EXPECT_EQ(deferred.weather.Committed(), sync.weather.Committed());
    EXPECT_EQ(deferred.ecology.Committed(), sync.ecology.Committed());
    // The two modes are not at the same game day: a deferred step applies at day + k, a synchronous one at day.
    EXPECT_GT(deferred.module.GetCurrentKey(), sync.module.GetCurrentKey());
    // The deferred run has a step in flight at the comparison point; it does not disturb committed state.
    EXPECT_GE(deferred.module.KickCount(), 20u);
    // And the state moved (the comparison is not of two untouched initial states).
    EXPECT_NE(sync.weather.Committed(), std::vector<uint32_t>(64, 1u));
}

TEST(WorldStepCadence, OracleHoldsAcrossStallsAndEarlyFinishes)
{
    OracleRig eager(true);
    OracleRig lazy(true);
    lazy.autoComplete = false;   // every step runs on a stall at its apply key
    OracleRig sync(false);
    eager.RunUntilApplies(15);
    lazy.RunUntilApplies(15);
    sync.RunUntilApplies(15);
    EXPECT_EQ(eager.weather.Committed(), sync.weather.Committed());
    EXPECT_EQ(lazy.weather.Committed(), sync.weather.Committed());
    EXPECT_EQ(lazy.ecology.Committed(), sync.ecology.Committed());
    EXPECT_GT(lazy.scheduler.GetStats().stalls, 0u);
}

// ---- AC 24a (oracle half): the oracle holds across a live mode switch ------------------------------------------------

TEST(WorldStepCadence, OracleHoldsAcrossALiveModeSwitch)
{
    // k = 3 leaves the channel idle between a step's apply (day 7n + 3) and the next week (7n + 7), so the
    // switch can happen while idle without skipping a week. Both runs then step the same weeks (7, 14, ...).
    OracleRig mixed(true, 3);
    OracleRig reference(false, 3);
    uint64_t day = 0;
    while (mixed.module.ApplyCount() < 6 && day < 200) mixed.StepDay(++day);
    ASSERT_EQ(mixed.module.ApplyCount(), 6u);
    ASSERT_EQ(mixed.scheduler.GetState(kWorld), StepState::kIdle);

    const char* reason = nullptr;
    ASSERT_TRUE(mixed.module.SetDeferred(false, &reason)) << (reason != nullptr ? reason : "");
    EXPECT_FALSE(mixed.module.IsDeferred());
    EXPECT_EQ(mixed.scheduler.GetChannelCount(), 0u);   // channel unregistered with no leak
    EXPECT_FALSE(mixed.module.IsChannelRegistered());
    const std::vector<uint32_t> committedAtSwitch = mixed.weather.Committed();

    while (mixed.module.ApplyCount() < 12 && day < 400) mixed.StepDay(++day);
    ASSERT_EQ(mixed.module.ApplyCount(), 12u);
    EXPECT_NE(mixed.weather.Committed(), committedAtSwitch);

    reference.RunUntilApplies(12);
    ASSERT_EQ(reference.module.ApplyCount(), 12u);
    EXPECT_EQ(mixed.weather.Committed(), reference.weather.Committed());
    EXPECT_EQ(mixed.ecology.Committed(), reference.ecology.Committed());
}

// ---- Edge cases (plan task 19) ---------------------------------------------------------------------------------------

TEST(WorldStepCadence, TickDayZeroResolvesToACenturyBandAndKicksOnce)
{
    Rig rig;
    rig.Step(0);   // GameSpeedController never publishes day 0, but a Century band is still a due week
    EXPECT_EQ(rig.module.KickCount(), 1u);
    EXPECT_EQ(rig.CaptureKeys("weather").front(), 0u);
    EXPECT_EQ(rig.module.GetCurrentKey(), 0u);
}

TEST(WorldStepCadence, ApplyAfterOneDayAppliesOnTheNextDay)
{
    Rig rig(Config("weather,ecology", 1, true));
    rig.Steps(1, 8);
    ASSERT_EQ(rig.commitDays.size(), 1u);
    EXPECT_EQ(rig.commitDays[0], 8u);
    EXPECT_EQ(rig.module.LateKicks(), 0u);
    EXPECT_EQ(rig.scheduler.GetStats().stalls, 0u);
}

TEST(WorldStepCadence, ApplyAfter365DaysKeepsOneWeekPendingAndDropsTheRest)
{
    Rig rig(Config("weather,ecology", 365, true));
    rig.Steps(1, 60);
    EXPECT_EQ(rig.module.KickCount(), 1u);
    EXPECT_EQ(rig.module.LateKicks(), 1u);
    EXPECT_EQ(rig.module.PendingWeekDay(), 14);
    EXPECT_EQ(rig.module.LostWeeks(), 6u);   // weeks 21, 28, 35, 42, 49, 56
}

TEST(WorldStepCadence, AChainOfOneStageKicksAndAnExtraRegisteredStageIsSkippedWithOneWarning)
{
    LogCapture logs("WST");
    Rig rig(Config("weather", 7, true), false);
    EXPECT_TRUE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
    EXPECT_TRUE(rig.module.RegisterStage(StringCRC("ecology"), rig.ecology));   // registered but not configured
    rig.Steps(1, 21);
    EXPECT_EQ(rig.module.KickCount(), 3u);
    EXPECT_EQ(rig.Count(CallPhase::Capture, "weather"), 3u);
    EXPECT_EQ(rig.Count(CallPhase::Capture, "ecology"), 0u);   // never called
    EXPECT_EQ(logs.Warnings("not in the configured order"), 1u);
}

TEST(WorldStepCadence, StageRegisteredAfterTheFirstTickStartsKickingOnceItArrives)
{
    Rig rig(kDefaultConfig, false);
    EXPECT_TRUE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
    rig.Steps(1, 20);
    EXPECT_EQ(rig.module.KickCount(), 0u);
    EXPECT_TRUE(rig.module.RegisterStage(StringCRC("ecology"), rig.ecology));
    rig.Steps(21, 21);
    EXPECT_EQ(rig.module.KickCount(), 1u);
}

// ---- Task 18: one integration test with a real JobSystem and the capped executor ---------------------------------------

TEST(WorldStepCadence, RealJobSystemAndCappedExecutorMatchTheSerialRunOver52Weeks)
{
    constexpr int kIterations = 3;   // the orchestrator records a longer soak (a few hundred iterations) once
    for (int iteration = 0; iteration < kIterations; ++iteration)
    {
        Dia::Threading::JobSystem jobs;
        jobs.Initialize(4);
        {
            Dia::DeferredStep::CapConfig cap;
            cap.workerCap = 2;
            cap.rangesPerWorker = 4;
            Dia::DeferredStep::JobStepLauncher launcher(jobs, cap);
            Dia::DeferredStep::DeferredStepScheduler scheduler(launcher);
            TestQueryHost host;
            Probe module{ StringCRC("WorldStepModule") };
            module.CallConfigure(Config("weather,ecology", 7, true).c_str());
            CoW::WorldStepModule::TestHosts hosts;
            hosts.scheduler = &scheduler;
            hosts.queryHost = &host;
            module.UseTestHosts(hosts);
            ASSERT_EQ(module.CallStart(), Dia::ApplicationFlow::StartResult::kReady);

            OracleStage weather(64, 3, 11u);   // same cell count as OracleRig, or the vectors cannot match
            OracleStage ecology(64, 2, 29u, &weather);
            ASSERT_TRUE(module.RegisterStage(StringCRC("weather"), weather));
            ASSERT_TRUE(module.RegisterStage(StringCRC("ecology"), ecology));

            // Weeks 7..364 are 52 kicks; the last applies at 371, and day 371 is itself a due week (a 53rd kick, never applied).
            for (uint64_t d = 1; d <= 371; ++d)
            {
                module.OnSimulationTick(MakeTick(d));
                scheduler.Tick(module.GetCurrentKey());
                module.CallUpdate();
            }
            EXPECT_EQ(module.KickCount(), 53u);
            EXPECT_EQ(module.ApplyCount(), 52u);
            EXPECT_EQ(module.LostWeeks(), 0u);

            OracleRig serial(false);
            serial.RunUntilApplies(52);
            ASSERT_EQ(serial.module.ApplyCount(), 52u);
            EXPECT_EQ(weather.Committed(), serial.weather.Committed());
            EXPECT_EQ(ecology.Committed(), serial.ecology.Committed());

            module.UnregisterStage(StringCRC("weather"));
            module.UnregisterStage(StringCRC("ecology"));
            module.CallStop();
        }
        jobs.Shutdown();
    }
}
