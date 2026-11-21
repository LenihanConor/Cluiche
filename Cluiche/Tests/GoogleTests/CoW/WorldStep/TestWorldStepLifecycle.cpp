// World Step Feature 2: missing and retired stages, mid-step unregister, scheduler-abandoned steps, DoStop, re-entry and
// the live mode switch (module-and-cadence.md AC 16, 18-22, 24a; plan tasks 12-15).
#include "WorldStepTestRig.h"

using namespace WsTest;
using Dia::Observation::Health::HealthStatus;
using Dia::DeferredStep::StepState;
using Dia::DeferredStep::Testing::LogCapture;
using Level = Dia::Observation::Log::LogLevel;

namespace
{
    const StringCRC kWorld("world");
    // k = 3 leaves the channel idle between a step's apply (7n + 3) and the next week (7n + 7).
    const std::string kIdleGapConfig = Config("weather,ecology", 3, true);
}

// ---- AC 16: a configured stage that never registers ----------------------------------------------------------------

TEST(WorldStepLifecycle, MissingConfiguredStageSetsFailingLogsAnErrorNamingItAndBlocksKicks)
{
    LogCapture logs("WST");
    Rig rig(kDefaultConfig, false);
    ASSERT_TRUE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
    rig.Steps(1, 20);   // weeks 7 and 14 pass with ecology missing
    EXPECT_EQ(rig.module.GetHealthReporter().Report().status, HealthStatus::kFailing);
    EXPECT_EQ(rig.module.GetHealthReporter().Report().reason, StringCRC("stage_missing"));
    EXPECT_EQ(logs.Count(Level::kError, "ecology"), 1u);   // once, not once per tick
    EXPECT_EQ(rig.module.KickCount(), 0u);
    EXPECT_EQ(rig.module.MissingStageCount(), 1u);

    ASSERT_TRUE(rig.module.RegisterStage(StringCRC("ecology"), rig.ecology));
    rig.Step(21);   // registering later clears Failing and kicks begin
    EXPECT_EQ(rig.module.GetHealthReporter().Report().status, HealthStatus::kOK);
    EXPECT_EQ(rig.module.MissingStageCount(), 0u);
    EXPECT_EQ(rig.module.KickCount(), 1u);
}

TEST(WorldStepLifecycle, TwoMissingStagesAreBothNamedInOneError)
{
    LogCapture logs("WST");
    Rig rig(kDefaultConfig, false);
    rig.Steps(1, 10);
    EXPECT_EQ(rig.module.MissingStageCount(), 2u);
    EXPECT_EQ(logs.Count(Level::kError, "weather"), 1u);
    EXPECT_EQ(logs.Count(Level::kError, "ecology"), 1u);
    EXPECT_EQ(logs.Count(Level::kError, "weather,ecology"), 1u);
}

// ---- AC 18: a stage that leaves while idle ---------------------------------------------------------------------------

TEST(WorldStepLifecycle, StageUnregisteredWhileIdleStopsKicksWithNoErrorAndTheInfoNamesIt)
{
    LogCapture logs("WST");
    Rig rig(kIdleGapConfig);
    rig.Steps(1, 11);   // week 7 kicked and applied at 10: idle now
    ASSERT_EQ(rig.scheduler.GetState(kWorld), StepState::kIdle);
    ASSERT_EQ(rig.module.KickCount(), 1u);

    rig.module.UnregisterStage(StringCRC("weather"));
    EXPECT_EQ(rig.module.RetiredStageCount(), 1u);
    EXPECT_EQ(rig.module.MissingStageCount(), 0u);
    EXPECT_GE(logs.Infos("'weather' retired"), 1u);

    rig.Steps(12, 45);
    EXPECT_EQ(rig.module.KickCount(), 1u);                      // kicks stopped
    EXPECT_EQ(logs.Count(Level::kError), 0u);                   // and nothing was an error
    EXPECT_EQ(rig.module.GetHealthReporter().Report().status, HealthStatus::kDegraded);
    EXPECT_EQ(rig.module.GetHealthReporter().Report().reason, StringCRC("stage_retired"));

    ASSERT_TRUE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));   // it comes back
    rig.Steps(46, 49);
    EXPECT_EQ(rig.module.KickCount(), 2u);
    EXPECT_EQ(rig.module.RetiredStageCount(), 0u);
    EXPECT_EQ(rig.module.GetHealthReporter().Report().status, HealthStatus::kOK);
}

TEST(WorldStepLifecycle, UnregisteringAnUnknownIdIsANoOp)
{
    LogCapture logs("WST");
    Rig rig;
    rig.module.UnregisterStage(StringCRC("nobody"));
    EXPECT_EQ(rig.module.GetChain().RegisteredCount(), 2u);
    EXPECT_EQ(logs.Count(Level::kError), 0u);
    EXPECT_EQ(logs.Warnings(), 0u);
}

// ---- AC 19: a stage unregistered while a step is in flight -----------------------------------------------------------

TEST(WorldStepLifecycle, StageUnregisteredMidStepDrainsAndDiscardsTheStepWithNoCommitAndNoLeak)
{
    LogCapture logs("WST");
    Rig rig;
    rig.autoComplete = false;
    rig.Steps(1, 7);   // week 7 is captured and in flight, not run
    ASSERT_EQ(rig.scheduler.GetState(kWorld), StepState::kRunning);
    ASSERT_EQ(rig.module.GetChain().Phase(), Dia::DeferredStep::ChainPhase::Captured);
    const uint32_t channelsBefore = rig.scheduler.GetChannelCount();

    rig.module.UnregisterStage(StringCRC("ecology"));
    rig.Harvest();

    EXPECT_EQ(logs.Warnings("mid-step"), 1u);
    EXPECT_EQ(logs.Count(Level::kError), 0u);
    EXPECT_EQ(rig.scheduler.GetState(kWorld), StepState::kIdle);                       // drained
    EXPECT_EQ(rig.module.GetChain().Phase(), Dia::DeferredStep::ChainPhase::Idle);        // abandoned
    EXPECT_EQ(rig.Count(CallPhase::Commit), 0u);                                       // never committed
    EXPECT_EQ(rig.module.HistorySeamCalls(), 0u);                                      // no history signal
    EXPECT_EQ(rig.module.ApplyCount(), 0u);
    EXPECT_EQ(rig.scheduler.GetStats().applies, 0u);
    EXPECT_EQ(rig.scheduler.GetChannelCount(), channelsBefore);                        // channel re-registered, none leaked
    EXPECT_TRUE(rig.module.IsChannelRegistered());
    EXPECT_EQ(rig.module.GetChain().EnabledCount(), 1u);                               // the removal took effect
    EXPECT_EQ(rig.module.GetChain().PendingRemovalCount(), 0u);

    // The channel is usable afterwards: the stage comes back and the next week kicks and applies normally.
    rig.autoComplete = true;
    ASSERT_TRUE(rig.module.RegisterStage(StringCRC("ecology"), rig.ecology));
    rig.Steps(8, 21);
    EXPECT_EQ(rig.module.KickCount(), 3u);   // 7 (discarded), 14, 21
    EXPECT_EQ(rig.module.ApplyCount(), 1u);  // only week 14, at 21
    EXPECT_EQ(rig.scheduler.GetChannelCount(), channelsBefore);
}

TEST(WorldStepLifecycle, MidStepUnregisterOfEveryStageLeavesAnIdleUsableModule)
{
    Rig rig;
    rig.autoComplete = false;
    rig.Steps(1, 7);
    rig.UnregisterBoth();
    EXPECT_EQ(rig.module.GetChain().RegisteredCount(), 0u);
    EXPECT_EQ(rig.scheduler.GetState(kWorld), StepState::kIdle);
    EXPECT_EQ(rig.module.GetChain().Phase(), Dia::DeferredStep::ChainPhase::Idle);
    EXPECT_EQ(rig.Count(CallPhase::Commit), 0u);
    rig.Steps(8, 30);   // retired stages, no kicks, no crash
    EXPECT_EQ(rig.module.KickCount(), 1u);
}

// ---- AC 20: the scheduler abandons a step (a load) -------------------------------------------------------------------

TEST(WorldStepLifecycle, SchedulerAbandonedStepResetsTheChainOnTheNextTick)
{
    LogCapture logs("WST");
    Rig rig;
    rig.autoComplete = false;
    rig.Steps(1, 7);
    ASSERT_EQ(rig.module.KickCount(), 1u);
    rig.scheduler.DrainAndDiscardAll();   // what a load does: the channel goes idle, the module is not told
    ASSERT_EQ(rig.scheduler.GetState(kWorld), StepState::kIdle);
    ASSERT_NE(rig.module.GetChain().Phase(), Dia::DeferredStep::ChainPhase::Idle);

    rig.Step(8);
    EXPECT_EQ(rig.module.GetChain().Phase(), Dia::DeferredStep::ChainPhase::Idle);
    EXPECT_EQ(logs.Infos("abandoned"), 1u);
    EXPECT_EQ(rig.Count(CallPhase::Commit), 0u);

    rig.autoComplete = true;
    rig.Steps(9, 14);   // a later kick is accepted
    EXPECT_EQ(rig.module.KickCount(), 2u);
    EXPECT_EQ(rig.scheduler.GetStats().kicksRejected, 0u);
}

// ---- AC 21: enter, exit, enter again ---------------------------------------------------------------------------------

TEST(WorldStepLifecycle, ReEntryRebuildsFromConfigWithNoLeakedChannelPendingWeekOrStaleKey)
{
    Rig rig(Config("weather,ecology", 10, true));
    rig.Steps(1, 14);   // week 7 in flight, week 14 pending
    ASSERT_TRUE(rig.module.HasPendingWeek());
    ASSERT_EQ(rig.module.GetCurrentKey(), 14u);
    rig.UnregisterBoth();   // stage modules stop before the host
    rig.Stop();

    // Everything the spec lists as a leak:
    EXPECT_EQ(rig.scheduler.GetChannelCount(), 0u);                       // channel registration
    EXPECT_FALSE(rig.module.IsChannelRegistered());
    EXPECT_FALSE(rig.module.HasPendingWeek());                            // pending week
    EXPECT_FALSE(rig.host.GetHost().Has(StringCRC("cow.worldstep.status")));   // names
    EXPECT_FALSE(rig.host.GetHost().Has(StringCRC("cow.worldstep.step")));
    EXPECT_EQ(rig.module.GetChain().RegisteredCount(), 0u);
    EXPECT_EQ(rig.module.RetiredStageCount(), 0u);
    EXPECT_FALSE(rig.module.IsRunning());

    ASSERT_TRUE(rig.Start());
    EXPECT_EQ(rig.module.GetCurrentKey(), 0u);                            // stale key
    EXPECT_FALSE(rig.module.HasPendingWeek());
    EXPECT_EQ(rig.module.KickCount(), 0u);
    EXPECT_EQ(rig.module.RetiredStageCount(), 0u);
    EXPECT_EQ(rig.module.MissingStageCount(), 0u);
    EXPECT_EQ(rig.scheduler.GetChannelCount(), 1u);                       // exactly one registration, not two
    EXPECT_TRUE(rig.module.NamesRegistered());
    EXPECT_EQ(rig.module.GetConfig().applyAfterSteps, 10u);                // rebuilt from config
    EXPECT_EQ(rig.module.GetChain().EnabledCount(), 0u);                  // stages must register again

    rig.RegisterBoth();
    rig.Steps(15, 21);                                                    // week 21 kicks on the second visit
    EXPECT_EQ(rig.module.KickCount(), 1u);
    EXPECT_EQ(rig.kickDays.back(), 21u);
    EXPECT_FALSE(rig.module.HasPendingWeek());
}

TEST(WorldStepLifecycle, SecondVisitKicksAgainOnItsFirstWeek)
{
    Rig rig;
    rig.Steps(1, 7);
    rig.UnregisterBoth();
    rig.Stop();
    ASSERT_TRUE(rig.Start());
    rig.RegisterBoth();
    rig.Steps(8, 14);   // days continue (the scheduler's own key never moves backwards)
    EXPECT_EQ(rig.module.KickCount(), 1u);
    EXPECT_EQ(rig.kickDays.back(), 14u);
}

TEST(WorldStepLifecycle, DoubleStopIsHarmless)
{
    Rig rig;
    rig.UnregisterBoth();
    rig.module.CallStop();
    rig.module.CallStop();
    EXPECT_EQ(rig.scheduler.GetChannelCount(), 0u);
    EXPECT_FALSE(rig.module.IsRunning());
    EXPECT_FALSE(rig.host.GetHost().Has(StringCRC("cow.worldstep.status")));
}

TEST(WorldStepLifecycle, StopBeforeAnyStartIsHarmless)
{
    Harness h(kDefaultConfig, false);
    h.module.CallStop();
    EXPECT_EQ(h.scheduler.GetChannelCount(), 0u);
}

// ---- AC 22: DoStop with a step in flight ------------------------------------------------------------------------------

TEST(WorldStepLifecycle, DoStopWithAStepInFlightAbandonsItWithNoCommitAndNoSeamCall)
{
    Rig rig;
    rig.autoComplete = false;
    rig.Steps(1, 7);
    ASSERT_EQ(rig.scheduler.GetState(kWorld), StepState::kRunning);
    ASSERT_EQ(rig.module.HistorySeamCalls(), 0u);

    rig.Stop();   // stages are still registered: the host stops without them having left
    rig.Harvest();
    EXPECT_EQ(rig.Count(CallPhase::Commit), 0u);
    EXPECT_EQ(rig.scheduler.GetStats().applies, 0u);
    EXPECT_EQ(rig.scheduler.GetChannelCount(), 0u);
    EXPECT_EQ(rig.module.GetChain().Phase(), Dia::DeferredStep::ChainPhase::Idle);
    EXPECT_EQ(rig.module.GetChain().RegisteredCount(), 0u);   // the chain was rebuilt empty
}

// ---- AC 24a: SetDeferred ------------------------------------------------------------------------------------------------

TEST(WorldStepLifecycle, SetDeferredSucceedsWhenIdleAndRegistersAndUnregistersTheChannelWithNoLeak)
{
    Rig rig(Config("weather,ecology", 7, false));
    ASSERT_EQ(rig.scheduler.GetChannelCount(), 0u);
    const int committedBefore = rig.weather.committed;
    const char* reason = nullptr;

    for (int round = 0; round < 5; ++round)
    {
        ASSERT_TRUE(rig.module.SetDeferred(true, &reason)) << (reason ? reason : "");
        EXPECT_TRUE(rig.module.IsDeferred());
        EXPECT_EQ(rig.scheduler.GetChannelCount(), 1u);
        EXPECT_TRUE(rig.module.IsChannelRegistered());
        ASSERT_TRUE(rig.module.SetDeferred(false, &reason)) << (reason ? reason : "");
        EXPECT_FALSE(rig.module.IsDeferred());
        EXPECT_EQ(rig.scheduler.GetChannelCount(), 0u);
        EXPECT_FALSE(rig.module.IsChannelRegistered());
    }
    EXPECT_EQ(rig.weather.committed, committedBefore);   // a switch changes no committed state
    EXPECT_EQ(rig.module.ApplyCount(), 0u);
}

TEST(WorldStepLifecycle, SwitchingToTheModeAlreadyInEffectIsANoOpSuccess)
{
    Rig rig;
    EXPECT_TRUE(rig.module.SetDeferred(true));
    EXPECT_EQ(rig.scheduler.GetChannelCount(), 1u);
}

TEST(WorldStepLifecycle, SetDeferredIsRefusedWithAReasonWhileAStepIsInFlight)
{
    LogCapture logs("WST");
    Rig rig;
    rig.Steps(1, 7);
    ASSERT_EQ(rig.scheduler.GetState(kWorld), StepState::kRunning);
    const char* reason = nullptr;
    EXPECT_FALSE(rig.module.SetDeferred(false, &reason));
    ASSERT_NE(reason, nullptr);
    EXPECT_NE(std::string(reason).find("in flight"), std::string::npos);
    EXPECT_TRUE(rig.module.IsDeferred());                    // unchanged
    EXPECT_EQ(rig.scheduler.GetChannelCount(), 1u);          // channel untouched
    EXPECT_EQ(rig.scheduler.GetState(kWorld), StepState::kRunning);
    EXPECT_EQ(logs.Warnings("refused"), 1u);

    rig.Steps(8, 14);   // the step still applies normally
    EXPECT_EQ(rig.module.ApplyCount(), 1u);
}

TEST(WorldStepLifecycle, SetDeferredWithARetiredStageWhileIdleSucceedsAndKicksResumeWhenItReturns)
{
    Rig rig(Config("weather,ecology", 3, false));
    rig.Steps(1, 8);
    ASSERT_EQ(rig.module.SyncStepCount(), 1u);
    rig.module.UnregisterStage(StringCRC("weather"));
    ASSERT_EQ(rig.module.RetiredStageCount(), 1u);

    ASSERT_TRUE(rig.module.SetDeferred(true));
    EXPECT_EQ(rig.scheduler.GetChannelCount(), 1u);
    rig.Steps(9, 20);
    EXPECT_EQ(rig.module.KickCount(), 0u);   // not ready while a configured stage is gone
    ASSERT_TRUE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));
    rig.Steps(21, 21);
    EXPECT_EQ(rig.module.KickCount(), 1u);
}

TEST(WorldStepLifecycle, SetDeferredBeforeStartOnlyChangesTheModeThatStartWillUse)
{
    Harness h(kDefaultConfig, false);
    EXPECT_TRUE(h.module.SetDeferred(false));
    ASSERT_TRUE(h.Start());
    EXPECT_FALSE(h.module.IsDeferred());
    EXPECT_EQ(h.scheduler.GetChannelCount(), 0u);
}

// ---- Misc lifecycle edges ----------------------------------------------------------------------------------------------

TEST(WorldStepLifecycle, RegisteringTheSameIdAfterAMidStepRemovalSucceeds)
{
    Rig rig;
    rig.autoComplete = false;
    rig.Steps(1, 7);
    rig.module.UnregisterStage(StringCRC("weather"));
    EXPECT_TRUE(rig.module.RegisterStage(StringCRC("weather"), rig.weather));   // the removal was applied at once after the drain
    EXPECT_EQ(rig.module.GetChain().EnabledCount(), 2u);
    EXPECT_EQ(rig.module.RetiredStageCount(), 0u);
}

TEST(WorldStepLifecycle, TheModuleCanRunWithStagesRegisteredBeforeStart)
{
    Harness h(kDefaultConfig, false);
    CallLog log;
    FakeStepStage weather(StringCRC("weather"), log);
    FakeStepStage ecology(StringCRC("ecology"), log);
    ASSERT_TRUE(h.module.RegisterStage(StringCRC("weather"), weather));
    ASSERT_TRUE(h.module.RegisterStage(StringCRC("ecology"), ecology));
    ASSERT_TRUE(h.Start());
    EXPECT_EQ(h.module.GetChain().EnabledCount(), 2u);   // picked up by Configure at start
    h.StepDays(1, 7);
    EXPECT_EQ(h.module.KickCount(), 1u);
    h.module.UnregisterStage(StringCRC("weather"));
    h.module.UnregisterStage(StringCRC("ecology"));
}
