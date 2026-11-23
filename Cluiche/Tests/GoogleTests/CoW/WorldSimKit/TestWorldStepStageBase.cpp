#include <gtest/gtest.h>

#include <string.h>

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

}  // namespace Testing
}  // namespace WorldStep
}  // namespace CoW
