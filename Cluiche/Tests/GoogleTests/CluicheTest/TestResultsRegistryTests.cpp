#include <gtest/gtest.h>
#include <Modules/TestStages/TestResultsRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace CluicheTest;

class TestResultsRegistryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        TestResultsRegistry::Create();
    }

    void TearDown() override
    {
        TestResultsRegistry::Destroy();
    }

    TestResultsRegistry& Registry() { return TestResultsRegistry::GetInstance(); }
};

TEST_F(TestResultsRegistryTest, InitiallyEmpty)
{
    EXPECT_EQ(Registry().GetResultCount(), 0u);
}

TEST_F(TestResultsRegistryTest, SetRunningCreatesEntry)
{
    Dia::Core::StringCRC stage("TestStage");
    Registry().SetRunning(stage, 300);

    EXPECT_EQ(Registry().GetResultCount(), 1u);
    const StageResult* r = Registry().GetResult(stage);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->state, StageResult::State::kRunning);
    EXPECT_EQ(r->budgetFrames, 300u);
}

TEST_F(TestResultsRegistryTest, SetPassedUpdatesState)
{
    Dia::Core::StringCRC stage("TestStage");
    Registry().SetRunning(stage, 300);
    Registry().SetPassed(stage, 42);

    const StageResult* r = Registry().GetResult(stage);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->state, StageResult::State::kPassed);
    EXPECT_EQ(r->settleFrame, 42u);
}

TEST_F(TestResultsRegistryTest, SetFailedUpdatesState)
{
    Dia::Core::StringCRC stage("TestStage");
    Registry().SetRunning(stage, 300);
    Registry().SetFailed(stage, 15);

    const StageResult* r = Registry().GetResult(stage);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->state, StageResult::State::kFailed);
    EXPECT_EQ(r->settleFrame, 15u);
}

TEST_F(TestResultsRegistryTest, SetTimeoutUpdatesState)
{
    Dia::Core::StringCRC stage("TestStage");
    Registry().SetRunning(stage, 100);
    Registry().SetTimeout(stage);

    const StageResult* r = Registry().GetResult(stage);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->state, StageResult::State::kTimeout);
}

TEST_F(TestResultsRegistryTest, GetResultReturnsNullForUnknownStage)
{
    Dia::Core::StringCRC stage("Unknown");
    EXPECT_EQ(Registry().GetResult(stage), nullptr);
}

TEST_F(TestResultsRegistryTest, MultipleStagesTrackedIndependently)
{
    Dia::Core::StringCRC stageA("StageA");
    Dia::Core::StringCRC stageB("StageB");

    Registry().SetRunning(stageA, 100);
    Registry().SetRunning(stageB, 200);
    Registry().SetPassed(stageA, 50);

    const StageResult* rA = Registry().GetResult(stageA);
    const StageResult* rB = Registry().GetResult(stageB);
    ASSERT_NE(rA, nullptr);
    ASSERT_NE(rB, nullptr);
    EXPECT_EQ(rA->state, StageResult::State::kPassed);
    EXPECT_EQ(rB->state, StageResult::State::kRunning);
    EXPECT_EQ(Registry().GetResultCount(), 2u);
}

TEST_F(TestResultsRegistryTest, ActiveStageTracksLatestRunning)
{
    Dia::Core::StringCRC stage("ActiveOne");
    Registry().SetRunning(stage, 500);

    EXPECT_EQ(Registry().GetActiveStage(), stage);
}

TEST_F(TestResultsRegistryTest, ActiveFrameCountSetAndGet)
{
    Registry().SetActiveFrameCount(123);
    EXPECT_EQ(Registry().GetActiveFrameCount(), 123u);
}

TEST_F(TestResultsRegistryTest, GetResultAtReturnsCorrectEntry)
{
    Dia::Core::StringCRC stageA("First");
    Dia::Core::StringCRC stageB("Second");

    Registry().SetRunning(stageA, 100);
    Registry().SetRunning(stageB, 200);

    const StageResult& r0 = Registry().GetResultAt(0);
    const StageResult& r1 = Registry().GetResultAt(1);
    EXPECT_EQ(r0.name, stageA);
    EXPECT_EQ(r1.name, stageB);
}

TEST_F(TestResultsRegistryTest, SetPassedOnUnknownStageIsNoOp)
{
    Dia::Core::StringCRC stage("Ghost");
    Registry().SetPassed(stage, 10);
    EXPECT_EQ(Registry().GetResultCount(), 0u);
}

TEST_F(TestResultsRegistryTest, SetRunningTwiceDoesNotDuplicate)
{
    Dia::Core::StringCRC stage("Same");
    Registry().SetRunning(stage, 100);
    Registry().SetRunning(stage, 200);

    EXPECT_EQ(Registry().GetResultCount(), 1u);
    const StageResult* r = Registry().GetResult(stage);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->budgetFrames, 200u);
}
