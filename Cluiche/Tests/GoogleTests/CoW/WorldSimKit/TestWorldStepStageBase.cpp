#include <gtest/gtest.h>

#include <string.h>

#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/Logger.h>

#include <DiaDeferredStep/SerialExecutor.h>
#include <Shared/WorldStep/Testing/FakeWorldStage.h>

namespace CoW {
namespace WorldStep {
namespace Testing {

TEST(FakeWorldStageTest, RecordsCaptureRunCommitInOrder)
{
    FakeWorldStage stage;
    Dia::DeferredStep::SerialExecutor exec;
    stage.Capture(7);
    stage.Run(exec);
    stage.Commit();
    ASSERT_EQ(3u, stage.log.Count());
    EXPECT_STREQ("Capture", stage.log.At(0));
    EXPECT_STREQ("Run", stage.log.At(1));
    EXPECT_STREQ("Commit", stage.log.At(2));
}

TEST(FakeWorldStageTest, IsReadyHonoursSwitch)
{
    FakeWorldStage stage;
    EXPECT_TRUE(stage.IsReady());
    stage.readyValue = false;
    EXPECT_FALSE(stage.IsReady());
    stage.readyValue = true;
    stage.extraReady = false;
    EXPECT_FALSE(stage.IsReady());
}

TEST(FakeWorldStageTest, PlainStructsDefaultToZero)
{
    FakeSim sim;
    FakeEntry entry;
    FakeLatch latch;
    EXPECT_EQ(0, sim.value);
    EXPECT_EQ(0, entry.value);
    EXPECT_EQ(0, latch.value);
}

// ---------------------------------------------------------------------------------------------
// WorldStepStageBase
// ---------------------------------------------------------------------------------------------

namespace {

class CountingSink : public Dia::Observation::Log::ISink
{
public:
    void OnLogEntry(const Dia::Observation::Log::LogEntry& e) override
    {
        if (strstr(e.message, "FakeWorld") == nullptr) { return; }   // only this stage's channel text
        if (e.level == Dia::Observation::Log::LogLevel::kWarning) { ++warnings; }
        if (e.level == Dia::Observation::Log::LogLevel::kInfo) { ++infos; }
        if (e.level == Dia::Observation::Log::LogLevel::kError) { ++errors; }
        if (strstr(e.message, "discard") != nullptr) { ++discardLogs; }
    }
    const char* GetName() const override { return "WorldStepStageBaseSink"; }
    unsigned warnings = 0, infos = 0, errors = 0, discardLogs = 0;
};

class WorldStepStageBaseTest : public ::testing::Test
{
protected:
    WorldStepStageBaseTest() : stage(hooks, &hooks.log) {}
    void SetUp() override
    {
        auto& l = Dia::Observation::Log::Logger::Instance();
        l.RegisterThreadBuffer();
        l.RegisterSink(&sink);
        stage.Start();
    }
    void TearDown() override
    {
        stage.Stop();
        auto& l = Dia::Observation::Log::Logger::Instance();
        l.FlushSync();
        l.UnregisterSink(&sink);
        l.UnregisterThreadBuffer();
    }
    uint64_t Discarded()
    {
        return Dia::Observation::Metric::MetricRegistry::Instance()
            .RegisterCounter(Dia::Core::StringCRC(World::MetricName("succession", "discarded_steps").c_str()))->Value();
    }
    double StepMs()
    {
        return Dia::Observation::Metric::MetricRegistry::Instance()
            .RegisterGauge(Dia::Core::StringCRC(World::MetricName("succession", "step_ms").c_str()))->Value();
    }
    void FullStep(uint64_t key)
    {
        stage.Capture(key);
        stage.Run(exec);
        stage.Commit();
    }
    CountingSink& Sink() { Dia::Observation::Log::Logger::Instance().FlushSync(); return sink; }

    FakeHooks hooks;
    FakeStage stage;
    CountingSink sink;
    Dia::DeferredStep::SerialExecutor exec;
};

}  // namespace

TEST_F(WorldStepStageBaseTest, NotReadyBeforeAnyWorld)
{
    EXPECT_FALSE(stage.IsReady());
}

TEST_F(WorldStepStageBaseTest, NotReadyWhileSpinUpRunsAndReadyOncePendingAtTarget)
{
    WorldView w; w.revision = 1; w.present = true;
    stage.OnWorldGenerationComplete(w);
    EXPECT_FALSE(stage.IsReady());          // job launched, nothing finished
    stage.RunJobNow(1, stage.VisitGeneration(), w);
    EXPECT_FALSE(stage.IsReady());          // finished but not yet polled
    stage.Update();
    EXPECT_TRUE(stage.HasPending());
    EXPECT_TRUE(stage.IsReady());
}

TEST_F(WorldStepStageBaseTest, ReadyViaActiveAtTargetAfterPromotion)
{
    stage.SpinUpNow(1);
    stage.Capture(5);
    EXPECT_FALSE(stage.HasPending());
    EXPECT_TRUE(stage.HasActive());
    EXPECT_TRUE(stage.IsReady());
}

TEST_F(WorldStepStageBaseTest, NotReadyWhenOnlyAnOlderRevisionIsHeld)
{
    stage.SpinUpNow(1);
    stage.Capture(5);
    stage.Commit();
    WorldView w; w.revision = 2; w.present = true;
    stage.OnWorldGenerationComplete(w);     // target 2, active still rev 1
    EXPECT_FALSE(stage.IsReady());
}

TEST_F(WorldStepStageBaseTest, PromotionHappensOnlyInCapture)
{
    stage.SpinUpNow(1);
    ASSERT_TRUE(stage.HasPending());
    stage.Run(exec);
    stage.Commit();
    stage.Update();
    EXPECT_TRUE(stage.HasPending());
    EXPECT_FALSE(stage.HasActive());
    stage.Capture(5);
    EXPECT_TRUE(stage.HasActive());
    EXPECT_FALSE(stage.HasPending());
}

TEST_F(WorldStepStageBaseTest, QueueDrainsInArrivalOrderBeforeLatchInputs)
{
    stage.SpinUpNow(1);
    hooks.log.Clear();
    for (int i = 1; i <= 3; ++i) { FakeEntry e; e.value = i * 10; EXPECT_TRUE(stage.Enqueue(e)); }
    stage.Capture(5);
    ASSERT_EQ(4u, hooks.log.Count());
    EXPECT_STREQ("ApplyEntry", hooks.log.At(0));
    EXPECT_STREQ("ApplyEntry", hooks.log.At(1));
    EXPECT_STREQ("ApplyEntry", hooks.log.At(2));
    EXPECT_STREQ("LatchInputs", hooks.log.At(3));
    ASSERT_EQ(3u, hooks.appliedCount);
    EXPECT_EQ(10, hooks.applied[0]);
    EXPECT_EQ(20, hooks.applied[1]);
    EXPECT_EQ(30, hooks.applied[2]);
    EXPECT_EQ(0u, stage.QueueCount());
}

TEST_F(WorldStepStageBaseTest, The129thEnqueueIsRefusedWithOneWarningAndDegraded)
{
    FakeEntry e;
    for (int i = 0; i < 128; ++i) { EXPECT_TRUE(stage.Enqueue(e)); }
    EXPECT_EQ(0u, Sink().warnings);
    EXPECT_FALSE(stage.Enqueue(e));
    EXPECT_EQ(1u, Sink().warnings);
    EXPECT_EQ(128u, stage.QueueCount());
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kDegraded, stage.GetHealth().status);
}

TEST_F(WorldStepStageBaseTest, StageIdIsTheSystemToken)
{
    EXPECT_STREQ("succession", stage.GetStageId().c_str());
    EXPECT_STREQ("weather", World::StageId(World::kWeather).c_str());
    EXPECT_STREQ("hydro", World::StageId(World::kHydro).c_str());
    EXPECT_STREQ("ecology", World::StageId(World::kEcology).c_str());
}

TEST(WorldStepOrderTest, ConfiguredStageOrderIsWeatherHydroEcology)
{
    ASSERT_EQ(3u, World::kStageOrderCount);
    EXPECT_STREQ("weather", World::kStageOrder[0]);
    EXPECT_STREQ("hydro", World::kStageOrder[1]);
    EXPECT_STREQ("ecology", World::kStageOrder[2]);
}

TEST_F(WorldStepStageBaseTest, ValidatePendingPassesThroughTheCodecSeam)
{
    Json::Value root(Json::objectValue);
    Dia::SaveGame::LoadContext ctx(root);
    EXPECT_TRUE(stage.ValidatePending(ctx));
}

TEST_F(WorldStepStageBaseTest, NormalStepPublishesOnceWithTheStepDay)
{
    stage.SpinUpNow(1);
    const uint64_t before = Discarded();
    FullStep(42);
    EXPECT_EQ(1u, hooks.publishCount);
    EXPECT_EQ(42u, hooks.publishedDay);
    EXPECT_EQ(before, Discarded());
    EXPECT_EQ(0u, Sink().discardLogs);
}

TEST_F(WorldStepStageBaseTest, DiscardsWhenTheRevisionChangesDuringTheStep)
{
    stage.SpinUpNow(1);
    const uint64_t before = Discarded();
    stage.Capture(5);
    stage.Run(exec);
    WorldView w; w.revision = 2; w.present = true;
    stage.OnWorldGenerationComplete(w);
    stage.Commit();
    EXPECT_EQ(0u, hooks.publishCount);
    EXPECT_EQ(before + 1, Discarded());
    EXPECT_EQ(1u, Sink().discardLogs);
}

TEST_F(WorldStepStageBaseTest, DiscardsAnInvalidStepAndRunIsANoOp)
{
    stage.SpinUpNow(1);
    hooks.latchOk = false;
    const uint64_t before = Discarded();
    hooks.log.Clear();
    FullStep(5);
    for (uint32_t i = 0; i < hooks.log.Count(); ++i) { EXPECT_STRNE("RunStep", hooks.log.At(i)); }
    EXPECT_EQ(0u, hooks.publishCount);
    EXPECT_EQ(before + 1, Discarded());
    EXPECT_EQ(1u, Sink().discardLogs);
}

TEST_F(WorldStepStageBaseTest, DiscardedStepsCounterReadsThreeAfterThreeDiscards)
{
    stage.SpinUpNow(1);
    hooks.latchOk = false;
    const uint64_t before = Discarded();
    FullStep(1);
    FullStep(2);
    FullStep(3);
    EXPECT_EQ(before + 3, Discarded());
    EXPECT_EQ(0u, hooks.publishCount);
}

TEST_F(WorldStepStageBaseTest, StepMsIsRecordedOnANormalStep)
{
    stage.SpinUpNow(1);
    hooks.runSleepMs = 4;
    FullStep(1);
    EXPECT_GE(StepMs(), 3.0);
}

TEST_F(WorldStepStageBaseTest, FailedInitialiseSimReportsFailingWithTheCause)
{
    hooks.initFails = true;
    stage.SpinUpNow(1);
    const Dia::Observation::Health::Health h = stage.GetHealth();
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kFailing, h.status);
    EXPECT_TRUE(h.reason == Dia::Core::StringCRC("fake_init_failed"));
    EXPECT_EQ(1u, stage.GetCauseCode());
    EXPECT_FALSE(stage.IsReady());
    EXPECT_FALSE(stage.HasPending());
    EXPECT_EQ(1u, Sink().errors);
}

// ----- kit task 10, partial (no hold): spin-up, failure and stale-result behaviours ---------------

TEST_F(WorldStepStageBaseTest, NewRevisionCancelsTheRunningSpinUpAndTheOldCompletionIsDiscarded)
{
    const int live = FakeSim::sLive;
    WorldView w1; w1.revision = 1; w1.present = true;
    WorldView w2; w2.revision = 2; w2.present = true;
    stage.OnWorldGenerationComplete(w1);
    stage.OnWorldGenerationComplete(w2);
    stage.RunJobNow(1, stage.VisitGeneration(), w1);   // late, superseded
    stage.Update();
    EXPECT_FALSE(stage.HasPending());
    EXPECT_EQ(live, FakeSim::sLive);                   // the discarded sim was deleted
    stage.RunJobNow(2, stage.VisitGeneration(), w2);
    stage.Update();
    EXPECT_TRUE(stage.HasPending());
    EXPECT_TRUE(stage.IsReady());
}

TEST_F(WorldStepStageBaseTest, ResultFromAnOlderVisitGenerationIsDiscarded)
{
    const int live = FakeSim::sLive;
    WorldView w; w.revision = 1; w.present = true;
    stage.OnWorldGenerationComplete(w);
    stage.RunJobNow(1, stage.VisitGeneration() - 1, w);
    stage.Update();
    EXPECT_FALSE(stage.HasPending());
    EXPECT_FALSE(stage.IsReady());
    EXPECT_EQ(live, FakeSim::sLive);
}

TEST_F(WorldStepStageBaseTest, WorldGenerationFailedDropsBothSimsAndFails)
{
    stage.SpinUpNow(1);
    stage.Capture(1);
    stage.Commit();                                    // active held
    stage.SpinUpNow(1);                                // and a pending one
    ASSERT_TRUE(stage.HasActive());
    ASSERT_TRUE(stage.HasPending());
    stage.OnWorldGenerationFailed();
    EXPECT_FALSE(stage.HasActive());
    EXPECT_FALSE(stage.HasPending());
    EXPECT_FALSE(stage.IsReady());
    EXPECT_EQ(Dia::Observation::Health::HealthStatus::kFailing, stage.GetHealth().status);
}

TEST_F(WorldStepStageBaseTest, WorldGenerationFailedCancelsTheRunningJob)
{
    const int live = FakeSim::sLive;
    WorldView w; w.revision = 1; w.present = true;
    stage.OnWorldGenerationComplete(w);
    stage.OnWorldGenerationFailed();
    stage.RunJobNow(1, stage.VisitGeneration(), w);    // cancelled: the job's sim is deleted
    stage.Update();
    EXPECT_FALSE(stage.HasPending());
    EXPECT_EQ(live, FakeSim::sLive);
}

TEST_F(WorldStepStageBaseTest, WorldGenerationFailedMidStepDefersTheDropAndDiscardsTheStep)
{
    stage.SpinUpNow(1);
    stage.Capture(5);
    stage.Run(exec);
    stage.OnWorldGenerationFailed();
    EXPECT_TRUE(stage.HasActive());                    // the worker may still read it
    stage.Commit();
    EXPECT_FALSE(stage.HasActive());
    EXPECT_EQ(0u, hooks.publishCount);
}

}  // namespace Testing
}  // namespace WorldStep
}  // namespace CoW
