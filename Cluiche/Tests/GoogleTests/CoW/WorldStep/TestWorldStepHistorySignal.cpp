// World Step Feature 3: the history signal (save-and-history.md AC 14-20a; plan tasks 2-6, 10).
#include "WorldStepTestRig.h"

using namespace WsTest;
using CoW::WorldStep::IWorldStepListener;
using CoW::WorldStep::WorldStepApplied;
using CoW::WorldStep::Testing::FakeWorldStepListener;
using Dia::DeferredStep::StepState;
using Dia::DeferredStep::Testing::LogCapture;
using Level = Dia::Observation::Log::LogLevel;

namespace
{
    const StringCRC kWorld("world");

    // A listener that calls back into the module from inside the signal and records what came back.
    struct ReentryProbe
    {
        CoW::WorldStepModule* module = nullptr;
        FakeWorldStepListener* other = nullptr;
        FakeWorldStepStage* stage = nullptr;
        FakeWorldStepListener* self = nullptr;
        int calls = 0;
        bool registerListener = true;
        bool registerStage = true;
        bool kick = true;
        bool unregisterListener = false;
        bool unregisterStage = false;

        static void OnSignal(void* user, const WorldStepApplied&)
        {
            ReentryProbe& p = *static_cast<ReentryProbe*>(user);
            ++p.calls;
            p.registerListener = p.module->RegisterListener(*p.other);
            p.registerStage = p.module->RegisterStage(StringCRC("hydro"), *p.stage);
            const Json::Value kicked = p.module->RunStepCommand(Json::Value(Json::objectValue));
            p.kick = kicked["success"].asBool();
            p.module->UnregisterListener(*p.self);
            p.unregisterListener = p.module->ListenerCount() == 0;
            p.module->UnregisterStage(StringCRC("weather"));
            p.unregisterStage = p.module->GetChain().EnabledCount() < 2;
        }
    };
}

// ---- AC 19: registration ------------------------------------------------------------------------------------------------

TEST(WorldStepHistorySignal, FourListenersGetTheSignalInRegistrationOrder)
{
    Rig rig;
    Dia::Core::Containers::DynamicArrayC<int, 16> order;
    FakeWorldStepListener l[5];
    for (int i = 0; i < 5; ++i) { l[i].sequence = &order; l[i].tag = i + 1; }

    for (int i = 0; i < 4; ++i)
        EXPECT_TRUE(rig.module.RegisterListener(l[i]));
    EXPECT_FALSE(rig.module.RegisterListener(l[4]));   // a fifth is refused
    EXPECT_EQ(rig.module.ListenerCount(), 4u);

    rig.Steps(1, 14);
    ASSERT_EQ(order.Size(), 4u);
    for (unsigned i = 0; i < 4; ++i)
        EXPECT_EQ(order[i], static_cast<int>(i) + 1);
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(l[i].received.Size(), 1u);
    EXPECT_EQ(l[4].received.Size(), 0u);

    for (int i = 0; i < 4; ++i)
        rig.module.UnregisterListener(l[i]);
}

TEST(WorldStepHistorySignal, DuplicateRegistrationReturnsFalseAndTheListenerIsRaisedOnce)
{
    Rig rig;
    FakeWorldStepListener l;
    EXPECT_TRUE(rig.module.RegisterListener(l));
    EXPECT_FALSE(rig.module.RegisterListener(l));
    EXPECT_EQ(rig.module.ListenerCount(), 1u);
    rig.Steps(1, 14);
    EXPECT_EQ(l.received.Size(), 1u);
    rig.module.UnregisterListener(l);
}

TEST(WorldStepHistorySignal, UnregisteredListenerGetsNothingAndTheOthersKeepTheirOrder)
{
    Rig rig;
    Dia::Core::Containers::DynamicArrayC<int, 16> order;
    FakeWorldStepListener a, b, c;
    a.sequence = b.sequence = c.sequence = &order;
    a.tag = 1; b.tag = 2; c.tag = 3;
    ASSERT_TRUE(rig.module.RegisterListener(a));
    ASSERT_TRUE(rig.module.RegisterListener(b));
    ASSERT_TRUE(rig.module.RegisterListener(c));
    rig.module.UnregisterListener(b);
    rig.module.UnregisterListener(b);   // unknown now: a no-op
    EXPECT_EQ(rig.module.ListenerCount(), 2u);
    rig.Steps(1, 14);
    ASSERT_EQ(order.Size(), 2u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 3);
    EXPECT_EQ(b.received.Size(), 0u);
    rig.module.UnregisterListener(a);
    rig.module.UnregisterListener(c);
}

TEST(WorldStepHistorySignal, NoListenerIsANoOpAndChangesNothingElse)
{
    uint64_t applies = 0, kicks = 0, seam = 0;
    int committed = 0;
    {
        Rig rig;
        rig.weather.source = 3;
        rig.Steps(1, 49);
        EXPECT_EQ(rig.module.ListenerCount(), 0u);
        applies = rig.module.ApplyCount();
        kicks = rig.module.KickCount();
        seam = rig.module.HistorySeamCalls();
        committed = rig.weather.committed;
    }
    EXPECT_GT(applies, 0u);
    EXPECT_EQ(seam, applies);   // the seam is reached once per apply, nobody is called

    Rig rig;
    FakeWorldStepListener l;
    ASSERT_TRUE(rig.module.RegisterListener(l));
    rig.weather.source = 3;
    rig.Steps(1, 49);
    EXPECT_EQ(rig.module.ApplyCount(), applies);
    EXPECT_EQ(rig.module.KickCount(), kicks);
    EXPECT_EQ(rig.weather.committed, committed);
    EXPECT_EQ(l.received.Size(), applies);
    rig.module.UnregisterListener(l);
}

// ---- AC 14, 15: payload and ordering --------------------------------------------------------------------------------------

TEST(WorldStepHistorySignal, EachAppliedStepRaisesOneSignalWithStepDayApplyKeyAndStageCount)
{
    Rig rig;
    FakeWorldStepListener l;
    ASSERT_TRUE(rig.module.RegisterListener(l));
    rig.Steps(1, 13);
    EXPECT_EQ(l.received.Size(), 0u);   // nothing applies before kick day + k
    rig.Step(14);
    ASSERT_EQ(l.received.Size(), 1u);
    EXPECT_EQ(l.received[0].stepDay, 7u);
    EXPECT_EQ(l.received[0].applyKey, 14u);
    EXPECT_EQ(l.received[0].stageCount, 2u);
    rig.Steps(15, 21);
    ASSERT_EQ(l.received.Size(), 2u);
    EXPECT_EQ(l.received[1].stepDay, 14u);
    EXPECT_EQ(l.received[1].applyKey, 21u);
    EXPECT_EQ(rig.module.ApplyCount(), 2u);
    rig.module.UnregisterListener(l);
}

TEST(WorldStepHistorySignal, TheSignalIsRaisedOnlyAfterEveryStageCommitted)
{
    Rig rig;
    rig.weather.source = 4;
    rig.ecology.source = 5;
    FakeWorldStepListener l;
    l.Observe(&rig.weather);
    l.Observe(&rig.ecology);
    ASSERT_TRUE(rig.module.RegisterListener(l));
    rig.Steps(1, 14);
    ASSERT_EQ(l.received.Size(), 1u);
    EXPECT_EQ(l.observedCommitted[0], 12);   // fake commits working * 3
    EXPECT_EQ(l.observedCommitted[1], 15);
    rig.module.UnregisterListener(l);
}

TEST(WorldStepHistorySignal, StageCountIsTheEnabledStagesNotTheRegisteredOnes)
{
    Rig rig(Config("weather", 7, true));   // ecology is registered but not in the order
    FakeWorldStepListener l;
    ASSERT_TRUE(rig.module.RegisterListener(l));
    rig.Steps(1, 14);
    ASSERT_EQ(l.received.Size(), 1u);
    EXPECT_EQ(l.received[0].stageCount, 1u);
    rig.module.UnregisterListener(l);
}

// ---- AC 17: retried week ----------------------------------------------------------------------------------------------------

TEST(WorldStepHistorySignal, ARetriedWeekCarriesTheMissedStepDayAndTheLaterApplyKey)
{
    Rig rig(Config("weather,ecology", 10, true));
    FakeWorldStepListener l;
    ASSERT_TRUE(rig.module.RegisterListener(l));
    rig.Steps(1, 17);   // week 7 applies at 17; week 14 found the channel busy and kicks at 17
    ASSERT_EQ(l.received.Size(), 1u);
    EXPECT_EQ(l.received[0].stepDay, 7u);
    EXPECT_EQ(l.received[0].applyKey, 17u);
    rig.Steps(18, 27);
    ASSERT_EQ(l.received.Size(), 2u);
    EXPECT_EQ(l.received[1].stepDay, 14u);   // the day the step stands for, not the retry day
    EXPECT_EQ(l.received[1].applyKey, 27u);
    rig.module.UnregisterListener(l);
}

// ---- AC 18: synchronous mode --------------------------------------------------------------------------------------------------

TEST(WorldStepHistorySignal, SynchronousModeRaisesInlineWithApplyKeyEqualToStepDay)
{
    Rig rig(Config("weather,ecology", 7, false));
    FakeWorldStepListener l;
    l.Observe(&rig.weather);
    rig.weather.source = 2;
    ASSERT_TRUE(rig.module.RegisterListener(l));
    rig.Steps(1, 14);
    ASSERT_EQ(l.received.Size(), 2u);
    EXPECT_EQ(l.received[0].stepDay, 7u);
    EXPECT_EQ(l.received[0].applyKey, 7u);
    EXPECT_EQ(l.received[1].stepDay, 14u);
    EXPECT_EQ(l.received[1].applyKey, 14u);
    EXPECT_EQ(l.received[1].stageCount, 2u);
    EXPECT_EQ(l.observedCommitted[0], 6);
    rig.module.UnregisterListener(l);
}

// ---- AC 16: suppression ---------------------------------------------------------------------------------------------------------

TEST(WorldStepHistorySignal, NotRaisedWhenAStageUnregistersMidStep)
{
    Rig rig;
    rig.autoComplete = false;
    FakeWorldStepListener l;
    ASSERT_TRUE(rig.module.RegisterListener(l));
    rig.Steps(1, 7);
    ASSERT_EQ(rig.scheduler.GetState(kWorld), StepState::kRunning);
    rig.module.UnregisterStage(StringCRC("ecology"));   // drains and discards
    rig.autoComplete = true;
    rig.Steps(8, 13);
    EXPECT_EQ(l.received.Size(), 0u);
    rig.module.UnregisterListener(l);
}

TEST(WorldStepHistorySignal, NotRaisedByASaveDrain)
{
    Rig rig;
    rig.autoComplete = false;
    FakeWorldStepListener l;
    ASSERT_TRUE(rig.module.RegisterListener(l));
    rig.Steps(1, 7);
    rig.scheduler.DrainAll();   // what a save does: waits for the step, applies nothing
    EXPECT_EQ(rig.scheduler.GetState(kWorld), StepState::kComplete);
    EXPECT_EQ(l.received.Size(), 0u);
    EXPECT_EQ(rig.module.ApplyCount(), 0u);
    rig.module.UnregisterListener(l);
}

TEST(WorldStepHistorySignal, NotRaisedByADiscardOrByStop)
{
    Rig rig;
    rig.autoComplete = false;
    FakeWorldStepListener l;
    ASSERT_TRUE(rig.module.RegisterListener(l));
    rig.Steps(1, 7);
    rig.scheduler.DrainAndDiscardAll();   // what a load does first
    EXPECT_EQ(l.received.Size(), 0u);
    rig.Steps(8, 14);                     // the scheduler abandoned the step: the chain resets on the next tick
    rig.autoComplete = true;
    rig.Steps(15, 15);
    rig.Stop();
    EXPECT_EQ(l.received.Size(), 0u);
    EXPECT_EQ(rig.module.HistorySeamCalls(), 0u);
}

// ---- AC 20a: apply tolerance ------------------------------------------------------------------------------------------------------

TEST(WorldStepHistorySignal, ApplyOfARestoredChannelWithAnIdleChainLogsOnceRaisesNothingAndTheNextKickIsAccepted)
{
    LogCapture logs("WST");
    Rig rig;
    FakeWorldStepListener l;
    ASSERT_TRUE(rig.module.RegisterListener(l));
    rig.Steps(1, 6);
    // A restored channel whose stages could not restore: the scheduler holds a pending result, the chain stayed Idle.
    ASSERT_TRUE(rig.scheduler.RestorePending(kWorld, 5, 10));
    rig.Steps(9, 10);

    EXPECT_EQ(logs.Warnings("nothing to commit"), 1u);
    EXPECT_EQ(l.received.Size(), 0u);
    EXPECT_EQ(rig.module.ApplyCount(), 0u);
    EXPECT_EQ(rig.module.HistorySeamCalls(), 0u);
    EXPECT_EQ(rig.module.GetChain().Phase(), CoW::WorldStep::ChainPhase::Idle);
    EXPECT_EQ(rig.scheduler.GetState(kWorld), StepState::kIdle);

    rig.Steps(11, 14);   // week 14 kicks: the channel and the chain are usable
    EXPECT_EQ(rig.module.KickCount(), 1u);
    rig.Steps(15, 21);
    EXPECT_EQ(rig.module.ApplyCount(), 1u);
    ASSERT_EQ(l.received.Size(), 1u);
    EXPECT_EQ(l.received[0].stepDay, 14u);
    rig.module.UnregisterListener(l);
}

// ---- AC 20: re-entrancy -----------------------------------------------------------------------------------------------------------

TEST(WorldStepHistorySignal, AListenerCallingBackIntoTheModuleIsRefusedAndTripsTheDebugAssert)
{
    AssertCapture asserts;
    Rig rig;
    FakeWorldStepStage extra(StringCRC("hydro"), rig.log);
    FakeWorldStepListener other;
    FakeWorldStepListener self;
    ReentryProbe probe;
    probe.module = &rig.module;
    probe.other = &other;
    probe.stage = &extra;
    probe.self = &self;
    self.hook = &ReentryProbe::OnSignal;
    self.hookUser = &probe;
    ASSERT_TRUE(rig.module.RegisterListener(self));

    rig.Steps(1, 14);

    ASSERT_EQ(probe.calls, 1);
    EXPECT_FALSE(probe.registerListener);
    EXPECT_FALSE(probe.registerStage);
    EXPECT_FALSE(probe.kick);
    EXPECT_FALSE(probe.unregisterListener);   // still registered: the call was a no-op
    EXPECT_FALSE(probe.unregisterStage);      // weather is still enabled
    EXPECT_EQ(rig.module.ListenerCount(), 1u);
    EXPECT_EQ(rig.module.GetChain().EnabledCount(), 2u);
    EXPECT_EQ(rig.module.GetChain().RegisteredCount(), 2u);
#ifdef DEBUG
    EXPECT_EQ(asserts.Count(), 5);   // one per refused call
#endif
    // The guard is released: the same calls work outside the signal.
    self.hook = nullptr;
    EXPECT_TRUE(rig.module.RegisterListener(other));
    EXPECT_TRUE(rig.module.RegisterStage(StringCRC("hydro"), extra));
    rig.module.UnregisterStage(StringCRC("hydro"));
    rig.module.UnregisterListener(other);
    rig.module.UnregisterListener(self);
}
