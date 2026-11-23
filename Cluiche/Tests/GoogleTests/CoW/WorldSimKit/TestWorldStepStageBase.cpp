#include <gtest/gtest.h>

#include <string.h>

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

}  // namespace Testing
}  // namespace WorldStep
}  // namespace CoW
