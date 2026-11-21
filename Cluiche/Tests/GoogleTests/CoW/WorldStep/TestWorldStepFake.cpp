// World Step Feature 1: FakeWorldStepStage call-log shape and executor identity (AC 13 shape, AC 19 identity).
#include <gtest/gtest.h>
#include <Shared/WorldStep/Testing/FakeWorldStepStage.h>

#include <DiaDeferredStep/SerialExecutor.h>

using namespace CoW::WorldStep;
using namespace CoW::WorldStep::Testing;
using Dia::Core::StringCRC;
using Dia::DeferredStep::SerialExecutor;

namespace
{
    // Counts ParallelFor calls so the fake's configurable count can be checked.
    class CountingExecutor final : public Dia::DeferredStep::IParallelExecutor
    {
    public:
        void ParallelFor(uint32_t, uint32_t, const Dia::DeferredStep::RangeFn&) override { ++calls; }
        uint32_t GetLastRangeCount() const override { return 1; }
        uint32_t calls = 0;
    };
}

TEST(WorldStepFake, RecordsCapturedRunCommitInOrderWithKey)
{
    CallLog log;
    FakeWorldStepStage fake(StringCRC("weather"), log);
    SerialExecutor exec;

    fake.Capture(42);
    fake.Run(exec);
    fake.Commit();

    ASSERT_EQ(log.Size(), 3u);
    EXPECT_TRUE(log[0].stageId == StringCRC("weather"));
    EXPECT_EQ(log[0].phase, CallPhase::Capture);
    EXPECT_EQ(log[0].key, 42u);
    EXPECT_EQ(log[1].phase, CallPhase::Run);
    EXPECT_EQ(log[2].phase, CallPhase::Commit);
    EXPECT_EQ(fake.CapturedKey(), 42u);
}

TEST(WorldStepFake, SharedLogInterleavesStagesInCallOrder)
{
    CallLog log;
    FakeWorldStepStage a(StringCRC("weather"), log);
    FakeWorldStepStage b(StringCRC("ecology"), log);

    a.Capture(1);
    b.Capture(1);

    ASSERT_EQ(log.Size(), 2u);
    EXPECT_TRUE(log[0].stageId == StringCRC("weather"));
    EXPECT_TRUE(log[1].stageId == StringCRC("ecology"));
}

TEST(WorldStepFake, IsReadyIsNotLoggedAndSetReadyFlips)
{
    CallLog log;
    FakeWorldStepStage fake(StringCRC("weather"), log);
    EXPECT_TRUE(fake.IsReady());
    fake.SetReady(false);
    EXPECT_FALSE(fake.IsReady());
    EXPECT_EQ(log.Size(), 0u);
}

TEST(WorldStepFake, RecordsExecutorIdentity)   // AC 19
{
    CallLog log;
    FakeWorldStepStage fake(StringCRC("weather"), log);
    SerialExecutor exec;
    EXPECT_EQ(fake.LastExecutor(), nullptr);
    fake.Capture(1);
    fake.Run(exec);
    EXPECT_EQ(fake.LastExecutor(), &exec);
}

TEST(WorldStepFake, ParallelForCallCountIsConfigurable)   // AC 19
{
    CallLog log;
    FakeWorldStepStage fake(StringCRC("weather"), log);
    CountingExecutor exec;
    fake.SetParallelForCalls(3);
    fake.Capture(1);
    fake.Run(exec);
    EXPECT_EQ(exec.calls, 3u);
}

TEST(WorldStepFake, PoisonedSourceDoesNotAffectRun)
{
    CallLog log;
    FakeWorldStepStage fake(StringCRC("weather"), log);
    SerialExecutor exec;
    fake.source = 5;
    fake.Capture(1);
    fake.source = 999;   // poison after capture
    fake.Run(exec);
    fake.Commit();
    EXPECT_EQ(fake.committed, 15);
}

TEST(WorldStepFake, CommitOrderProbeSeesOtherCommittedValueAtOwnCommit)
{
    CallLog log;
    FakeWorldStepStage first(StringCRC("weather"), log);
    FakeWorldStepStage second(StringCRC("ecology"), log);
    CommitOrderProbe probe;
    probe.other = &first;
    second.SetCommitProbe(&probe);
    SerialExecutor exec;
    first.source = 2;
    second.source = 1;

    first.Capture(1);  second.Capture(1);
    first.Run(exec);   second.Run(exec);
    first.Commit();    second.Commit();

    EXPECT_TRUE(probe.fired);
    EXPECT_EQ(probe.observed, 6);
}

TEST(WorldStepFake, ResetOnCaptureClearsPriorWorkingState)
{
    CallLog log;
    FakeWorldStepStage fake(StringCRC("weather"), log);
    SerialExecutor exec;
    fake.resetOnCapture = true;
    fake.source = 4;
    fake.Capture(1);
    fake.Run(exec);
    EXPECT_EQ(fake.result, 12);
    fake.source = 0;
    fake.Capture(2);
    EXPECT_EQ(fake.working, 0);
    EXPECT_EQ(fake.result, 0);
}
