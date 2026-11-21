// World Step Feature 1: WorldStepChain (plan tasks 5-13). AC 5-27, Gap-pass amendments 2-4.
#include <gtest/gtest.h>
#include <Shared/WorldStep/WorldStepChain.h>
#include <Shared/WorldStep/Testing/FakeWorldStepStage.h>

#include <DiaDeferredStep/SerialExecutor.h>

#include <cstdio>
#include <initializer_list>
#include <memory>
#include <vector>

using namespace CoW::WorldStep;
using namespace CoW::WorldStep::Testing;
using Dia::Core::StringCRC;
using Dia::DeferredStep::SerialExecutor;

namespace
{
    WorldStepConfig MakeConfig(std::initializer_list<const char*> ids)
    {
        WorldStepConfig cfg;
        for (const char* id : ids)
        {
            StageId s;
            s.crc  = StringCRC(id);
            s.name = Dia::Core::Containers::String32(id);
            cfg.order.Add(s);
        }
        return cfg;
    }

    // Order {weather, ecology}; both fakes registered (ecology first, to prove declared order wins) and ready.
    struct Rig
    {
        CallLog log;
        FakeWorldStepStage weather{StringCRC("weather"), log};
        FakeWorldStepStage ecology{StringCRC("ecology"), log};
        WorldStepChain chain;
        SerialExecutor exec;

        Rig()
        {
            chain.Configure(MakeConfig({"weather", "ecology"}));
            chain.RegisterStage(StringCRC("ecology"), ecology);
            chain.RegisterStage(StringCRC("weather"), weather);
        }

        void FullCycle(StepKey key = 1)
        {
            ASSERT_TRUE(chain.Capture(key));
            ASSERT_TRUE(chain.Run(exec));
            ASSERT_TRUE(chain.Commit());
        }
    };

    // Expects log[offset], log[offset+1] to be weather then ecology for `phase`.
    void ExpectWeatherThenEcology(const CallLog& log, uint32_t offset, CallPhase phase)
    {
        ASSERT_GE(log.Size(), offset + 2);
        EXPECT_TRUE(log[offset].stageId == StringCRC("weather"));
        EXPECT_EQ(log[offset].phase, phase);
        EXPECT_TRUE(log[offset + 1].stageId == StringCRC("ecology"));
        EXPECT_EQ(log[offset + 1].phase, phase);
    }

    class CountingExecutor final : public Dia::DeferredStep::IParallelExecutor
    {
    public:
        void ParallelFor(uint32_t, uint32_t, const Dia::DeferredStep::RangeFn&) override { ++calls; }
        uint32_t GetLastRangeCount() const override { return 1; }
        uint32_t calls = 0;
    };

    struct ObserverEvent
    {
        bool     begin;
        uint32_t index;
        StagePhase phase;
        bool operator==(const ObserverEvent& o) const { return begin == o.begin && index == o.index && phase == o.phase; }
    };

    class RecordingObserver final : public IChainObserver
    {
    public:
        void OnStageBegin(uint32_t index, StagePhase phase) override { events.push_back({true, index, phase}); }
        void OnStageEnd(uint32_t index, StagePhase phase) override { events.push_back({false, index, phase}); }
        std::vector<ObserverEvent> events;
    };

    // A stage that calls back into the chain from inside each of its methods.
    class ReentrantStage final : public IWorldStepStage
    {
    public:
        explicit ReentrantStage(WorldStepChain& chain) : mChain(chain) {}

        bool IsReady() const override { return true; }
        void Capture(StepKey) override
        {
            captureNested = mChain.Capture(9);
            configureNested = mChain.Configure(WorldStepConfig());
        }
        void Run(Dia::DeferredStep::IParallelExecutor& ex) override { runNested = mChain.Run(ex); }
        void Commit() override { commitNested = mChain.Commit(); }
        void SerializePending(Dia::SaveGame::SaveContext&) const override {}
        void DeserializePending(Dia::SaveGame::LoadContext&) override {}
        bool ValidatePending(Dia::SaveGame::LoadContext&) const override { return true; }

        bool captureNested = true, configureNested = true, runNested = true, commitNested = true;

    private:
        WorldStepChain& mChain;
    };
}

// ----------------------------------------------------------------------------------------------------------
// Registration and resolution (AC 5-10)
// ----------------------------------------------------------------------------------------------------------

TEST(WorldStepChain, EnabledFollowsDeclaredOrderNotRegistrationOrder)   // AC 5
{
    Rig rig;   // registers ecology first
    ASSERT_EQ(rig.chain.EnabledCount(), 2u);
    EXPECT_EQ(rig.chain.StageAt(0), &rig.weather);
    EXPECT_EQ(rig.chain.StageAt(1), &rig.ecology);
    EXPECT_TRUE(rig.chain.StageIdAt(0) == StringCRC("weather"));
    EXPECT_TRUE(rig.chain.StageIdAt(1) == StringCRC("ecology"));
    EXPECT_EQ(rig.chain.StageAt(2), nullptr);
}

TEST(WorldStepChain, RegisterFalseOnDuplicateIdAndChangesNothing)   // AC 8
{
    Rig rig;
    FakeWorldStepStage other(StringCRC("weather"), rig.log);
    EXPECT_FALSE(rig.chain.RegisterStage(StringCRC("weather"), other));
    EXPECT_EQ(rig.chain.RegisteredCount(), 2u);
    EXPECT_EQ(rig.chain.EnabledCount(), 2u);
    EXPECT_EQ(rig.chain.StageAt(0), &rig.weather);
}

TEST(WorldStepChain, RegisterFalseWhenTableFullAndChangesNothing)   // AC 8
{
    CallLog log;
    WorldStepChain chain;
    chain.Configure(MakeConfig({"s0"}));
    std::vector<std::unique_ptr<FakeWorldStepStage>> fakes;
    for (uint32_t i = 0; i < kMaxStages; ++i)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "s%u", i);
        fakes.push_back(std::make_unique<FakeWorldStepStage>(StringCRC(name), log));
        EXPECT_TRUE(chain.RegisterStage(StringCRC(name), *fakes.back())) << name;
    }
    EXPECT_EQ(chain.RegisteredCount(), kMaxStages);

    FakeWorldStepStage extra(StringCRC("overflow"), log);
    EXPECT_FALSE(chain.RegisterStage(StringCRC("overflow"), extra));
    EXPECT_EQ(chain.RegisteredCount(), kMaxStages);
    EXPECT_EQ(chain.EnabledCount(), 1u);
}

TEST(WorldStepChain, UnregisterAtIdleRemovesAtOnce)   // AC 9
{
    Rig rig;
    rig.chain.UnregisterStage(StringCRC("weather"));
    EXPECT_EQ(rig.chain.EnabledCount(), 1u);
    EXPECT_EQ(rig.chain.StageAt(0), &rig.ecology);
    EXPECT_EQ(rig.chain.RegisteredCount(), 1u);
    EXPECT_EQ(rig.chain.PendingRemovalCount(), 0u);
}

TEST(WorldStepChain, UnregisterUnknownIdIsNoOp)   // AC 9
{
    Rig rig;
    rig.chain.UnregisterStage(StringCRC("nonesuch"));
    EXPECT_EQ(rig.chain.EnabledCount(), 2u);
    EXPECT_EQ(rig.chain.RegisteredCount(), 2u);
    EXPECT_EQ(rig.chain.PendingRemovalCount(), 0u);
}

TEST(WorldStepChain, ConfigureAgainReplacesOrderAndReresolvesRegisteredStages)   // AC 10
{
    Rig rig;
    ASSERT_TRUE(rig.chain.Configure(MakeConfig({"ecology", "weather"})));
    ASSERT_EQ(rig.chain.EnabledCount(), 2u);
    EXPECT_EQ(rig.chain.StageAt(0), &rig.ecology);
    EXPECT_EQ(rig.chain.StageAt(1), &rig.weather);

    ASSERT_TRUE(rig.chain.Configure(MakeConfig({"weather"})));
    EXPECT_EQ(rig.chain.EnabledCount(), 1u);
    EXPECT_EQ(rig.chain.StageAt(0), &rig.weather);
    StageIdList unknown;
    ASSERT_TRUE(rig.chain.UnknownStages(unknown));
    EXPECT_TRUE(unknown[0].crc == StringCRC("ecology"));
}

TEST(WorldStepChain, ConfigureMidStepReturnsFalseAndChangesNothing)   // AC 10
{
    Rig rig;
    ASSERT_TRUE(rig.chain.Capture(1));
    EXPECT_FALSE(rig.chain.Configure(MakeConfig({"ecology"})));
    EXPECT_EQ(rig.chain.EnabledCount(), 2u);
    EXPECT_EQ(rig.chain.StageAt(0), &rig.weather);
    ASSERT_TRUE(rig.chain.Run(rig.exec));
    EXPECT_FALSE(rig.chain.Configure(MakeConfig({"ecology"})));
    EXPECT_EQ(rig.chain.EnabledCount(), 2u);
    ASSERT_TRUE(rig.chain.Commit());
    EXPECT_TRUE(rig.chain.Configure(MakeConfig({"ecology"})));   // Idle again
}

// ----------------------------------------------------------------------------------------------------------
// Unknown and missing stages (AC 6, 7)
// ----------------------------------------------------------------------------------------------------------

TEST(WorldStepChain, UnknownStageIsNotEnabledNeverCalledAndReported)   // AC 6
{
    Rig rig;
    FakeWorldStepStage extra(StringCRC("extra"), rig.log);
    ASSERT_TRUE(rig.chain.RegisterStage(StringCRC("extra"), extra));
    EXPECT_EQ(rig.chain.EnabledCount(), 2u);

    StageIdList unknown;
    ASSERT_TRUE(rig.chain.UnknownStages(unknown));
    ASSERT_EQ(unknown.Size(), 1u);
    EXPECT_TRUE(unknown[0].crc == StringCRC("extra"));

    rig.FullCycle();
    ASSERT_EQ(rig.log.Size(), 6u);
    for (uint32_t i = 0; i < rig.log.Size(); ++i)
    {
        EXPECT_FALSE(rig.log[i].stageId == StringCRC("extra"));
    }
}

TEST(WorldStepChain, UnknownStagesFalseWhenEverythingRegisteredIsConfigured)   // AC 6
{
    Rig rig;
    StageIdList unknown;
    EXPECT_FALSE(rig.chain.UnknownStages(unknown));
    EXPECT_EQ(unknown.Size(), 0u);
}

TEST(WorldStepChain, MissingStagesListedInDeclaredOrderAndAllReadyFalse)   // AC 7
{
    CallLog log;
    FakeWorldStepStage ecology(StringCRC("ecology"), log);
    WorldStepChain chain;
    chain.Configure(MakeConfig({"weather", "hydro", "ecology"}));
    chain.RegisterStage(StringCRC("ecology"), ecology);

    StageIdList missing;
    ASSERT_TRUE(chain.MissingStages(missing));
    ASSERT_EQ(missing.Size(), 2u);
    EXPECT_TRUE(missing[0].crc == StringCRC("weather"));
    EXPECT_TRUE(missing[1].crc == StringCRC("hydro"));
    EXPECT_FALSE(chain.AllReady());
}

TEST(WorldStepChain, MissingStagesFalseWhenAllRegistered)   // AC 7
{
    Rig rig;
    StageIdList missing;
    EXPECT_FALSE(rig.chain.MissingStages(missing));
    EXPECT_EQ(missing.Size(), 0u);
}

// ----------------------------------------------------------------------------------------------------------
// Readiness (AC 11, 12, 26)
// ----------------------------------------------------------------------------------------------------------

TEST(WorldStepChain, AllReadyFalseUntilEveryEnabledStageReadyAndNoneMissing)   // AC 11
{
    CallLog log;
    FakeWorldStepStage weather(StringCRC("weather"), log);
    FakeWorldStepStage ecology(StringCRC("ecology"), log);
    weather.SetReady(false);
    ecology.SetReady(false);
    WorldStepChain chain;
    chain.Configure(MakeConfig({"weather", "ecology"}));
    EXPECT_FALSE(chain.AllReady());                              // none registered

    chain.RegisterStage(StringCRC("weather"), weather);
    EXPECT_FALSE(chain.AllReady());                              // ecology missing, weather not ready
    weather.SetReady(true);
    EXPECT_FALSE(chain.AllReady());                              // ecology still missing

    chain.RegisterStage(StringCRC("ecology"), ecology);
    EXPECT_FALSE(chain.AllReady());                              // ecology not ready
    ecology.SetReady(true);
    EXPECT_TRUE(chain.AllReady());

    weather.SetReady(false);
    EXPECT_FALSE(chain.AllReady());                              // flipping one flips it back
}

TEST(WorldStepChain, AllReadyFalseWhenNeverConfigured)   // AC 11
{
    WorldStepChain chain;
    EXPECT_FALSE(chain.AllReady());
    EXPECT_EQ(chain.EnabledCount(), 0u);
}

TEST(WorldStepChain, UnknownStageReadinessDoesNotAffectAllReady)   // AC 12
{
    Rig rig;
    FakeWorldStepStage extra(StringCRC("extra"), rig.log);
    extra.SetReady(false);
    rig.chain.RegisterStage(StringCRC("extra"), extra);
    EXPECT_TRUE(rig.chain.AllReady());
}

TEST(WorldStepChain, StageGoingNotReadyAfterCommitMakesNextAllReadyFalse)   // AC 26
{
    Rig rig;
    rig.weather.source = 4;
    rig.FullCycle();
    EXPECT_EQ(rig.weather.committed, 12);
    rig.weather.SetReady(false);
    EXPECT_FALSE(rig.chain.AllReady());
    EXPECT_EQ(rig.weather.committed, 12);                        // the committed step is unaffected
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Idle);
}

// ----------------------------------------------------------------------------------------------------------
// Sequencing (AC 13-20)
// ----------------------------------------------------------------------------------------------------------

TEST(WorldStepChain, EachPhaseVisitsEnabledStagesInDeclaredOrder)   // AC 13
{
    Rig rig;
    rig.FullCycle();
    ASSERT_EQ(rig.log.Size(), 6u);
    ExpectWeatherThenEcology(rig.log, 0, CallPhase::Capture);
    ExpectWeatherThenEcology(rig.log, 2, CallPhase::Run);
    ExpectWeatherThenEcology(rig.log, 4, CallPhase::Commit);
}

TEST(WorldStepChain, OutOfSequenceCallsReturnFalseAndCallNoStage)   // AC 14
{
    Rig rig;
    // Idle: Run and Commit are wrong.
    EXPECT_FALSE(rig.chain.Run(rig.exec));
    EXPECT_FALSE(rig.chain.Commit());
    EXPECT_EQ(rig.log.Size(), 0u);
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Idle);

    // Captured: Capture twice and Commit are wrong.
    ASSERT_TRUE(rig.chain.Capture(1));
    const uint32_t afterCapture = rig.log.Size();
    EXPECT_EQ(afterCapture, 2u);
    EXPECT_FALSE(rig.chain.Capture(2));
    EXPECT_FALSE(rig.chain.Commit());
    EXPECT_EQ(rig.log.Size(), afterCapture);
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Captured);
    EXPECT_EQ(rig.chain.CapturedKey(), 1u);

    // Ran: Run twice and Capture are wrong.
    ASSERT_TRUE(rig.chain.Run(rig.exec));
    const uint32_t afterRun = rig.log.Size();
    EXPECT_EQ(afterRun, 4u);
    EXPECT_FALSE(rig.chain.Run(rig.exec));
    EXPECT_FALSE(rig.chain.Capture(3));
    EXPECT_EQ(rig.log.Size(), afterRun);
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Ran);

    // Commit twice.
    ASSERT_TRUE(rig.chain.Commit());
    const uint32_t afterCommit = rig.log.Size();
    EXPECT_FALSE(rig.chain.Commit());
    EXPECT_EQ(rig.log.Size(), afterCommit);
}

TEST(WorldStepChain, CommitReturnsToIdleAndAnotherStepRunsOnTheSameChain)   // AC 15
{
    Rig rig;
    rig.FullCycle(1);
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Idle);
    rig.FullCycle(2);
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Idle);
    EXPECT_EQ(rig.log.Size(), 12u);
}

TEST(WorldStepChain, CaptureGivesEveryStageTheSameKeyAndChainReportsIt)   // AC 16
{
    Rig rig;
    ASSERT_TRUE(rig.chain.Capture(0xABCDEF0123ull));
    EXPECT_EQ(rig.chain.CapturedKey(), 0xABCDEF0123ull);
    EXPECT_EQ(rig.weather.CapturedKey(), 0xABCDEF0123ull);
    EXPECT_EQ(rig.ecology.CapturedKey(), 0xABCDEF0123ull);
    EXPECT_EQ(rig.log[0].key, 0xABCDEF0123ull);
    EXPECT_EQ(rig.log[1].key, 0xABCDEF0123ull);
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Captured);
}

TEST(WorldStepChain, RunUsesCapturedWorkingCopyNotPoisonedSource)   // AC 17
{
    Rig rig;
    rig.weather.source = 5;
    ASSERT_TRUE(rig.chain.Capture(1));
    rig.weather.source = 999;   // poison
    ASSERT_TRUE(rig.chain.Run(rig.exec));
    ASSERT_TRUE(rig.chain.Commit());
    EXPECT_EQ(rig.weather.committed, 15);
}

TEST(WorldStepChain, LaterStageSeesEarlierStagesCommitAlreadyVisible)   // AC 18
{
    Rig rig;
    CommitOrderProbe probe;
    probe.other = &rig.weather;
    rig.ecology.SetCommitProbe(&probe);
    rig.weather.source = 4;
    rig.FullCycle();
    EXPECT_TRUE(probe.fired);
    EXPECT_EQ(probe.observed, 12);
}

TEST(WorldStepChain, SameExecutorPassedToEveryStageAndParallelForMayRepeat)   // AC 19
{
    Rig rig;
    rig.weather.SetParallelForCalls(3);
    rig.ecology.SetParallelForCalls(2);
    CountingExecutor counting;
    ASSERT_TRUE(rig.chain.Capture(1));
    ASSERT_TRUE(rig.chain.Run(counting));
    EXPECT_EQ(rig.weather.LastExecutor(), &counting);
    EXPECT_EQ(rig.ecology.LastExecutor(), &counting);
    EXPECT_EQ(counting.calls, 5u);
    ASSERT_TRUE(rig.chain.Commit());
}

TEST(WorldStepChain, CaptureRunCommitDoNotReallocateTables)   // AC 20 (amendment 3)
{
    Rig rig;
    const void*    table    = rig.chain.EnabledTableData();
    const uint32_t capacity = WorldStepChain::EnabledTableCapacity();
    const uint32_t regCap   = WorldStepChain::RegisteredTableCapacity();
    const uint32_t enabled  = rig.chain.EnabledCount();
    const uint32_t reg      = rig.chain.RegisteredCount();

    rig.FullCycle();

    EXPECT_EQ(rig.chain.EnabledTableData(), table);
    EXPECT_EQ(WorldStepChain::EnabledTableCapacity(), capacity);
    EXPECT_EQ(WorldStepChain::RegisteredTableCapacity(), regCap);
    EXPECT_EQ(rig.chain.EnabledCount(), enabled);
    EXPECT_EQ(rig.chain.RegisteredCount(), reg);
    EXPECT_EQ(rig.chain.PendingRemovalCount(), 0u);
}

// ----------------------------------------------------------------------------------------------------------
// Mid-step changes (AC 21-25)
// ----------------------------------------------------------------------------------------------------------

TEST(WorldStepChain, StageUnregisteredAfterCaptureStillRunsCommitsAndIsGoneNextStep)   // AC 21
{
    CallLog log;
    auto weather = std::make_unique<FakeWorldStepStage>(StringCRC("weather"), log);
    auto ecology = std::make_unique<FakeWorldStepStage>(StringCRC("ecology"), log);
    SerialExecutor exec;
    WorldStepChain chain;
    chain.Configure(MakeConfig({"weather", "ecology"}));
    chain.RegisterStage(StringCRC("weather"), *weather);
    chain.RegisterStage(StringCRC("ecology"), *ecology);

    ASSERT_TRUE(chain.Capture(1));
    chain.UnregisterStage(StringCRC("ecology"));
    EXPECT_EQ(chain.EnabledCount(), 2u);   // table untouched until the step ends
    ASSERT_TRUE(chain.Run(exec));
    ASSERT_TRUE(chain.Commit());

    ASSERT_EQ(log.Size(), 6u);
    ExpectWeatherThenEcology(log, 4, CallPhase::Commit);
    EXPECT_EQ(chain.EnabledCount(), 1u);
    EXPECT_EQ(chain.PendingRemovalCount(), 0u);

    // The removed stage may now be destroyed; the chain must not touch it again.
    ecology.reset();
    log.RemoveAll();
    ASSERT_TRUE(chain.Capture(2));
    ASSERT_TRUE(chain.Run(exec));
    ASSERT_TRUE(chain.Commit());
    EXPECT_EQ(log.Size(), 3u);
    EXPECT_EQ(chain.EnabledCount(), 1u);
}

TEST(WorldStepChain, AbandonAfterCaptureCallsNoStageAndReturnsToIdle)   // AC 22
{
    Rig rig;
    ASSERT_TRUE(rig.chain.Capture(1));
    rig.chain.UnregisterStage(StringCRC("ecology"));
    const uint32_t before = rig.log.Size();
    rig.chain.Abandon();
    EXPECT_EQ(rig.log.Size(), before);
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Idle);
    EXPECT_EQ(rig.chain.PendingRemovalCount(), 0u);
    EXPECT_EQ(rig.chain.EnabledCount(), 1u);   // removal applied
    EXPECT_EQ(rig.chain.RegisteredCount(), 1u);
}

TEST(WorldStepChain, AbandonAfterRunCallsNoStageAndReturnsToIdle)   // AC 22
{
    Rig rig;
    ASSERT_TRUE(rig.chain.Capture(1));
    ASSERT_TRUE(rig.chain.Run(rig.exec));
    rig.chain.UnregisterStage(StringCRC("weather"));
    const uint32_t before = rig.log.Size();
    rig.chain.Abandon();
    EXPECT_EQ(rig.log.Size(), before);
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Idle);
    EXPECT_EQ(rig.chain.EnabledCount(), 1u);
    EXPECT_EQ(rig.chain.StageAt(0), &rig.ecology);
    EXPECT_EQ(rig.weather.committed, 0);   // no Commit happened
}

TEST(WorldStepChain, AbandonAtIdleIsDefinedNoOp)   // AC 22 (edge)
{
    Rig rig;
    rig.chain.Abandon();
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Idle);
    EXPECT_EQ(rig.chain.EnabledCount(), 2u);
    EXPECT_EQ(rig.log.Size(), 0u);
}

TEST(WorldStepChain, CaptureAfterAbandonWorksAndDoesNotSeeAbandonedWorkingState)   // AC 23
{
    Rig rig;
    rig.weather.source = 2;
    ASSERT_TRUE(rig.chain.Capture(1));
    ASSERT_TRUE(rig.chain.Run(rig.exec));
    EXPECT_EQ(rig.weather.result, 6);
    rig.chain.Abandon();
    EXPECT_EQ(rig.weather.committed, 0);

    rig.weather.resetOnCapture = true;
    ASSERT_TRUE(rig.chain.Capture(2));
    EXPECT_EQ(rig.chain.CapturedKey(), 2u);
    EXPECT_EQ(rig.weather.result, 0);   // abandoned result not visible
    ASSERT_TRUE(rig.chain.Run(rig.exec));
    ASSERT_TRUE(rig.chain.Commit());
    EXPECT_EQ(rig.weather.committed, 6);
}

TEST(WorldStepChain, ReregisterWhileRemovalPendingFalseUntilIdleThenSucceeds)   // AC 24
{
    Rig rig;
    ASSERT_TRUE(rig.chain.Capture(1));
    rig.chain.UnregisterStage(StringCRC("weather"));
    EXPECT_FALSE(rig.chain.RegisterStage(StringCRC("weather"), rig.weather));
    ASSERT_TRUE(rig.chain.Run(rig.exec));
    EXPECT_FALSE(rig.chain.RegisterStage(StringCRC("weather"), rig.weather));
    ASSERT_TRUE(rig.chain.Commit());

    EXPECT_EQ(rig.chain.EnabledCount(), 1u);
    EXPECT_TRUE(rig.chain.RegisterStage(StringCRC("weather"), rig.weather));
    ASSERT_EQ(rig.chain.EnabledCount(), 2u);
    EXPECT_EQ(rig.chain.StageAt(0), &rig.weather);
}

TEST(WorldStepChain, MidStepUnregisterLeavesTheTableRunWalksUntouched)   // AC 25 (structural)
{
    Rig rig;
    ASSERT_TRUE(rig.chain.Capture(1));
    const void*    table   = rig.chain.EnabledTableData();
    IWorldStepStage* s0    = rig.chain.StageAt(0);
    IWorldStepStage* s1    = rig.chain.StageAt(1);

    rig.chain.UnregisterStage(StringCRC("weather"));
    rig.chain.UnregisterStage(StringCRC("weather"));   // idempotent: queued once

    EXPECT_EQ(rig.chain.EnabledTableData(), table);
    EXPECT_EQ(rig.chain.EnabledCount(), 2u);
    EXPECT_EQ(rig.chain.StageAt(0), s0);
    EXPECT_EQ(rig.chain.StageAt(1), s1);
    EXPECT_TRUE(rig.chain.StageIdAt(0) == StringCRC("weather"));
    EXPECT_EQ(rig.chain.RegisteredCount(), 2u);
    EXPECT_EQ(rig.chain.PendingRemovalCount(), 1u);   // the removal lives in its own list
    ASSERT_TRUE(rig.chain.Run(rig.exec));
    ASSERT_TRUE(rig.chain.Commit());
}

// ----------------------------------------------------------------------------------------------------------
// Restore (AC 27)
// ----------------------------------------------------------------------------------------------------------

TEST(WorldStepChain, RestoreRanMovesIdleToRanWithoutCallingStagesThenCommitCommitsInOrder)   // AC 27
{
    Rig rig;
    ASSERT_TRUE(rig.chain.RestoreRan(77));
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Ran);
    EXPECT_EQ(rig.chain.CapturedKey(), 77u);
    EXPECT_EQ(rig.log.Size(), 0u);

    ASSERT_TRUE(rig.chain.Commit());
    ASSERT_EQ(rig.log.Size(), 2u);
    ExpectWeatherThenEcology(rig.log, 0, CallPhase::Commit);
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Idle);
}

TEST(WorldStepChain, RestoreRanFalseWhenNotAllReady)   // AC 27
{
    Rig rig;
    rig.ecology.SetReady(false);
    EXPECT_FALSE(rig.chain.RestoreRan(5));
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Idle);
    EXPECT_EQ(rig.chain.CapturedKey(), 0u);
}

TEST(WorldStepChain, RestoreRanFalseWhenAStageIsMissing)   // AC 27
{
    CallLog log;
    FakeWorldStepStage weather(StringCRC("weather"), log);
    WorldStepChain chain;
    chain.Configure(MakeConfig({"weather", "ecology"}));
    chain.RegisterStage(StringCRC("weather"), weather);
    EXPECT_FALSE(chain.RestoreRan(5));
    EXPECT_EQ(chain.Phase(), ChainPhase::Idle);
}

TEST(WorldStepChain, RestoreRanFalseWhenAlreadyCapturedOrRan)   // AC 27
{
    Rig rig;
    ASSERT_TRUE(rig.chain.Capture(1));
    EXPECT_FALSE(rig.chain.RestoreRan(2));
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Captured);
    EXPECT_EQ(rig.chain.CapturedKey(), 1u);
    ASSERT_TRUE(rig.chain.Run(rig.exec));
    EXPECT_FALSE(rig.chain.RestoreRan(2));
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Ran);
    EXPECT_EQ(rig.chain.CapturedKey(), 1u);
}

// ----------------------------------------------------------------------------------------------------------
// Observer (amendment 2)
// ----------------------------------------------------------------------------------------------------------

TEST(WorldStepChain, ObserverSeesBeginEndPairsPerStagePerPhaseInDeclaredOrder)   // amendment 2
{
    Rig rig;
    RecordingObserver obs;
    rig.chain.SetObserver(&obs);
    rig.FullCycle();

    const StagePhase phases[3] = {StagePhase::Capture, StagePhase::Run, StagePhase::Commit};
    std::vector<ObserverEvent> expected;
    for (StagePhase p : phases)
    {
        for (uint32_t i = 0; i < 2; ++i)
        {
            expected.push_back({true, i, p});
            expected.push_back({false, i, p});
        }
    }
    EXPECT_TRUE(obs.events == expected);
}

TEST(WorldStepChain, ObserverBracketsEachStageCallNotTheWholePhase)   // amendment 2
{
    // Begin and end of stage 0 land before the begin of stage 1: interleave check via the call log size.
    struct Probe final : IChainObserver
    {
        CallLog* log = nullptr;
        std::vector<uint32_t> sizesAtEvent;
        void OnStageBegin(uint32_t, StagePhase) override { sizesAtEvent.push_back(log->Size()); }
        void OnStageEnd(uint32_t, StagePhase) override { sizesAtEvent.push_back(log->Size()); }
    } probe;
    Rig rig;
    probe.log = &rig.log;
    rig.chain.SetObserver(&probe);
    ASSERT_TRUE(rig.chain.Capture(1));
    ASSERT_EQ(probe.sizesAtEvent.size(), 4u);
    EXPECT_EQ(probe.sizesAtEvent[0], 0u);   // begin 0: nothing called yet
    EXPECT_EQ(probe.sizesAtEvent[1], 1u);   // end 0: stage 0 called
    EXPECT_EQ(probe.sizesAtEvent[2], 1u);   // begin 1
    EXPECT_EQ(probe.sizesAtEvent[3], 2u);   // end 1
}

TEST(WorldStepChain, NullObserverIsANoOp)   // amendment 2
{
    Rig rig;
    rig.chain.SetObserver(nullptr);
    rig.FullCycle();
    EXPECT_EQ(rig.log.Size(), 6u);

    RecordingObserver obs;
    rig.chain.SetObserver(&obs);
    rig.chain.SetObserver(nullptr);
    rig.FullCycle();
    EXPECT_TRUE(obs.events.empty());
}

TEST(WorldStepChain, ObserverNotCalledByAbandonOrRestoreRan)   // amendment 2
{
    Rig rig;
    RecordingObserver obs;
    rig.chain.SetObserver(&obs);
    ASSERT_TRUE(rig.chain.RestoreRan(3));
    EXPECT_TRUE(obs.events.empty());
    rig.chain.Abandon();
    EXPECT_TRUE(obs.events.empty());
}

// ----------------------------------------------------------------------------------------------------------
// Release-only behaviours (amendment 4): the Debug build asserts instead, so these are skipped there.
// ----------------------------------------------------------------------------------------------------------

TEST(WorldStepChain, CaptureWhenNotAllReadyIsNoOpReturningFalseInRelease)   // spec Sequencing
{
#ifdef DEBUG
    GTEST_SKIP() << "Debug build asserts on Capture while not AllReady";
#else
    Rig rig;
    rig.ecology.SetReady(false);
    EXPECT_FALSE(rig.chain.Capture(1));
    EXPECT_EQ(rig.chain.Phase(), ChainPhase::Idle);
    EXPECT_EQ(rig.log.Size(), 0u);
#endif
}

TEST(WorldStepChain, ChainCallFromInsideStageMethodIsNoOpReturningFalseInRelease)   // amendment 4
{
#ifdef DEBUG
    GTEST_SKIP() << "Debug build asserts on re-entrant chain calls";
#else
    WorldStepChain chain;
    SerialExecutor exec;
    ReentrantStage stage(chain);
    chain.Configure(MakeConfig({"weather"}));
    chain.RegisterStage(StringCRC("weather"), stage);

    ASSERT_TRUE(chain.Capture(1));
    EXPECT_FALSE(stage.captureNested);
    EXPECT_FALSE(stage.configureNested);
    EXPECT_EQ(chain.Phase(), ChainPhase::Captured);
    ASSERT_TRUE(chain.Run(exec));
    EXPECT_FALSE(stage.runNested);
    EXPECT_EQ(chain.Phase(), ChainPhase::Ran);
    ASSERT_TRUE(chain.Commit());
    EXPECT_FALSE(stage.commitNested);
    EXPECT_EQ(chain.Phase(), ChainPhase::Idle);
    EXPECT_EQ(chain.EnabledCount(), 1u);
#endif
}
