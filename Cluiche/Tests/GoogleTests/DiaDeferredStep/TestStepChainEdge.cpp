// edge tests beyond the numbered ACs (plan task 15).
#include <gtest/gtest.h>
#include <DiaDeferredStep/StepChain.h>
#include <DiaDeferredStep/Testing/FakeStepStage.h>

#include <DiaParallelExecutor/SerialExecutor.h>

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <memory>
#include <vector>

using namespace Dia::DeferredStep;
using namespace Dia::DeferredStep::Testing;
using Dia::Core::StringCRC;
using Dia::ParallelExecutor::SerialExecutor;

namespace
{
    constexpr uint32_t kMaxStages = kDefaultStageCapacity;
}

namespace
{
    StepChainConfig<> MakeConfig(std::initializer_list<const char*> ids)
    {
        StepChainConfig<> cfg;
        for (const char* id : ids)
        {
            StageId s;
            s.crc  = StringCRC(id);
            s.name = Dia::Core::Containers::String32(id);
            cfg.order.Add(s);
        }
        return cfg;
    }
}

TEST(StepChainEdge, MaxStagesAllRegisteredAndEnabledRunInDeclaredOrder)
{
    static_assert(kMaxStages == 8, "test config below lists 8 ids");
    CallLog log;
    StepChain<> chain;
    Dia::ParallelExecutor::SerialExecutor exec;
    ASSERT_TRUE(chain.Configure(MakeConfig({"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7"})));

    std::vector<std::unique_ptr<FakeStepStage>> fakes;
    // Register in reverse so declared order must win.
    for (int i = static_cast<int>(kMaxStages) - 1; i >= 0; --i)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "s%d", i);
        fakes.push_back(std::make_unique<FakeStepStage>(StringCRC(name), log));
        ASSERT_TRUE(chain.RegisterStage(StringCRC(name), *fakes.back())) << name;
    }
    EXPECT_EQ(chain.RegisteredCount(), kMaxStages);
    EXPECT_EQ(chain.EnabledCount(), kMaxStages);
    EXPECT_TRUE(chain.AllReady());

    StageIdList<> list;
    EXPECT_FALSE(chain.MissingStages(list));
    EXPECT_FALSE(chain.UnknownStages(list));

    ASSERT_TRUE(chain.Capture(5));
    ASSERT_TRUE(chain.Run(exec));
    ASSERT_TRUE(chain.Commit());
    EXPECT_EQ(chain.Phase(), ChainPhase::Idle);

    ASSERT_EQ(log.Size(), kMaxStages * 3u);
    for (uint32_t phase = 0; phase < 3; ++phase)
    {
        for (uint32_t i = 0; i < kMaxStages; ++i)
        {
            char name[16];
            std::snprintf(name, sizeof(name), "s%u", i);
            const CallRecord& r = log[phase * kMaxStages + i];
            EXPECT_TRUE(r.stageId == StringCRC(name)) << "phase " << phase << " index " << i;
        }
    }
}

TEST(StepChainEdge, ConfigureShrinkingOrderExcludesRegisteredStage)
{
    CallLog log;
    FakeStepStage a(StringCRC("a"), log);
    FakeStepStage b(StringCRC("b"), log);
    StepChain<> chain;
    Dia::ParallelExecutor::SerialExecutor exec;
    ASSERT_TRUE(chain.Configure(MakeConfig({"a", "b"})));
    ASSERT_TRUE(chain.RegisterStage(StringCRC("a"), a));
    ASSERT_TRUE(chain.RegisterStage(StringCRC("b"), b));
    ASSERT_EQ(chain.EnabledCount(), 2u);

    // Shrink: a is still registered but no longer in the order.
    ASSERT_TRUE(chain.Configure(MakeConfig({"b"})));
    EXPECT_EQ(chain.RegisteredCount(), 2u);
    ASSERT_EQ(chain.EnabledCount(), 1u);
    EXPECT_TRUE(chain.StageAt(0) == &b);
    EXPECT_TRUE(chain.StageIdAt(0) == StringCRC("b"));
    EXPECT_TRUE(chain.StageAt(1) == nullptr);

    StageIdList<> unknown;
    ASSERT_TRUE(chain.UnknownStages(unknown));
    ASSERT_EQ(unknown.Size(), 1u);
    EXPECT_TRUE(unknown[0].crc == StringCRC("a"));
    StageIdList<> missing;
    EXPECT_FALSE(chain.MissingStages(missing));

    // a being not ready no longer matters.
    a.SetReady(false);
    EXPECT_TRUE(chain.AllReady());

    ASSERT_TRUE(chain.Capture(1));
    ASSERT_TRUE(chain.Run(exec));
    ASSERT_TRUE(chain.Commit());
    ASSERT_EQ(log.Size(), 3u);
    for (uint32_t i = 0; i < log.Size(); ++i)
    {
        EXPECT_TRUE(log[i].stageId == StringCRC("b"));
    }

    // Growing back re-enables the still-registered stage.
    a.SetReady(true);
    ASSERT_TRUE(chain.Configure(MakeConfig({"a", "b"})));
    EXPECT_EQ(chain.EnabledCount(), 2u);
    EXPECT_TRUE(chain.StageAt(0) == &a);
}

TEST(StepChainEdge, UnregisterThenReregisterAtIdle)
{
    CallLog log;
    FakeStepStage a(StringCRC("a"), log);
    FakeStepStage b(StringCRC("b"), log);
    StepChain<> chain;
    Dia::ParallelExecutor::SerialExecutor exec;
    ASSERT_TRUE(chain.Configure(MakeConfig({"a", "b"})));
    ASSERT_TRUE(chain.RegisterStage(StringCRC("a"), a));
    ASSERT_TRUE(chain.RegisterStage(StringCRC("b"), b));

    chain.UnregisterStage(StringCRC("a"));
    EXPECT_EQ(chain.RegisteredCount(), 1u);
    EXPECT_EQ(chain.PendingRemovalCount(), 0u);
    EXPECT_EQ(chain.EnabledCount(), 1u);
    EXPECT_FALSE(chain.AllReady());   // a is configured but missing
    StageIdList<> missing;
    ASSERT_TRUE(chain.MissingStages(missing));
    ASSERT_EQ(missing.Size(), 1u);
    EXPECT_TRUE(missing[0].crc == StringCRC("a"));

    // Re-register at Idle succeeds at once, in declared position.
    ASSERT_TRUE(chain.RegisterStage(StringCRC("a"), a));
    EXPECT_EQ(chain.RegisteredCount(), 2u);
    ASSERT_EQ(chain.EnabledCount(), 2u);
    EXPECT_TRUE(chain.StageAt(0) == &a);
    EXPECT_TRUE(chain.StageAt(1) == &b);
    EXPECT_TRUE(chain.AllReady());

    ASSERT_TRUE(chain.Capture(2));
    ASSERT_TRUE(chain.Run(exec));
    ASSERT_TRUE(chain.Commit());
    ASSERT_EQ(log.Size(), 6u);
    EXPECT_TRUE(log[0].stageId == StringCRC("a"));
    EXPECT_TRUE(log[1].stageId == StringCRC("b"));
}

TEST(StepChainEdge, CaptureWithEnabledNotReadyStageIsNoOpThenWorksOnceReadyInRelease)
{
#ifdef DEBUG
    GTEST_SKIP() << "Debug build asserts on Capture while not AllReady";
#else
    CallLog log;
    FakeStepStage a(StringCRC("a"), log);
    FakeStepStage b(StringCRC("b"), log);
    StepChain<> chain;
    ASSERT_TRUE(chain.Configure(MakeConfig({"a", "b"})));
    ASSERT_TRUE(chain.RegisterStage(StringCRC("a"), a));
    ASSERT_TRUE(chain.RegisterStage(StringCRC("b"), b));
    b.SetReady(false);   // registered, enabled, not ready; the ready stage precedes it

    EXPECT_FALSE(chain.Capture(9));
    EXPECT_EQ(chain.Phase(), ChainPhase::Idle);
    EXPECT_EQ(chain.CapturedKey(), 0u);
    EXPECT_EQ(log.Size(), 0u);   // not even the ready stage was captured

    b.SetReady(true);
    EXPECT_TRUE(chain.Capture(9));
    EXPECT_EQ(chain.Phase(), ChainPhase::Captured);
    EXPECT_EQ(chain.CapturedKey(), 9u);
#endif
}

TEST(StepChainEdge, PhaseAndKeyUnchangedAfterAnyFalseReturn)
{
    CallLog log;
    FakeStepStage a(StringCRC("a"), log);
    StepChain<> chain;
    Dia::ParallelExecutor::SerialExecutor exec;
    ASSERT_TRUE(chain.Configure(MakeConfig({"a"})));
    ASSERT_TRUE(chain.RegisterStage(StringCRC("a"), a));

    // Idle: Run, Commit false.
    EXPECT_FALSE(chain.Run(exec));
    EXPECT_FALSE(chain.Commit());
    EXPECT_EQ(chain.Phase(), ChainPhase::Idle);
    // Duplicate register and unknown unregister must not move the phase.
    EXPECT_FALSE(chain.RegisterStage(StringCRC("a"), a));
    chain.UnregisterStage(StringCRC("nope"));
    EXPECT_EQ(chain.Phase(), ChainPhase::Idle);
    EXPECT_EQ(log.Size(), 0u);

    // Captured: Capture again, Commit, RestoreRan, Configure all false.
    ASSERT_TRUE(chain.Capture(11));
    const uint32_t afterCapture = log.Size();
    EXPECT_FALSE(chain.Capture(12));
    EXPECT_FALSE(chain.Commit());
    EXPECT_FALSE(chain.RestoreRan(13));
    EXPECT_FALSE(chain.Configure(MakeConfig({"a"})));
    EXPECT_FALSE(chain.RegisterStage(StringCRC("a"), a));
    EXPECT_EQ(chain.Phase(), ChainPhase::Captured);
    EXPECT_EQ(chain.CapturedKey(), 11u);
    EXPECT_EQ(log.Size(), afterCapture);

    // Ran: Capture, Run again, RestoreRan, Configure all false.
    ASSERT_TRUE(chain.Run(exec));
    const uint32_t afterRun = log.Size();
    EXPECT_FALSE(chain.Capture(14));
    EXPECT_FALSE(chain.Run(exec));
    EXPECT_FALSE(chain.RestoreRan(15));
    EXPECT_FALSE(chain.Configure(MakeConfig({"a"})));
    EXPECT_EQ(chain.Phase(), ChainPhase::Ran);
    EXPECT_EQ(chain.CapturedKey(), 11u);
    EXPECT_EQ(log.Size(), afterRun);

    ASSERT_TRUE(chain.Commit());
    EXPECT_EQ(chain.Phase(), ChainPhase::Idle);
    EXPECT_EQ(chain.EnabledCount(), 1u);
    EXPECT_EQ(chain.RegisteredCount(), 1u);
}

TEST(StepChainEdge, RegisterWhenFullWithPendingRemovalStaysFalseUntilIdle)
{
    CallLog log;
    StepChain<> chain;
    Dia::ParallelExecutor::SerialExecutor exec;
    ASSERT_TRUE(chain.Configure(MakeConfig({"s0"})));
    std::vector<std::unique_ptr<FakeStepStage>> fakes;
    for (uint32_t i = 0; i < kMaxStages; ++i)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "s%u", i);
        fakes.push_back(std::make_unique<FakeStepStage>(StringCRC(name), log));
        ASSERT_TRUE(chain.RegisterStage(StringCRC(name), *fakes.back()));
    }
    ASSERT_TRUE(chain.Capture(1));
    chain.UnregisterStage(StringCRC("s5"));   // queued mid-step; table still full
    EXPECT_EQ(chain.PendingRemovalCount(), 1u);
    FakeStepStage extra(StringCRC("extra"), log);
    EXPECT_FALSE(chain.RegisterStage(StringCRC("extra"), extra));
    EXPECT_EQ(chain.RegisteredCount(), kMaxStages);

    ASSERT_TRUE(chain.Run(exec));
    ASSERT_TRUE(chain.Commit());
    EXPECT_EQ(chain.PendingRemovalCount(), 0u);
    EXPECT_EQ(chain.RegisteredCount(), kMaxStages - 1);
    EXPECT_TRUE(chain.RegisterStage(StringCRC("extra"), extra));
    EXPECT_EQ(chain.RegisteredCount(), kMaxStages);
}
