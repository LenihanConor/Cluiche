#include <gtest/gtest.h>
#include <Modules/TestResultChangeTracker.h>
#include <Modules/TestStages/TestResultsRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstring>

using namespace CluicheTest;
using Cluiche::AppFlow::StageResultEvent;
using Cluiche::AppFlow::StageResultSeverity;

namespace {

class TestResultChangeTrackerTest : public ::testing::Test
{
protected:
    void SetUp() override { TestResultsRegistry::Create(); }
    void TearDown() override { TestResultsRegistry::Destroy(); }

    TestResultsRegistry& Registry() { return TestResultsRegistry::GetInstance(); }

    unsigned int Poll(StageResultEvent* out, unsigned int max, uint64_t now = 1000)
    {
        return mTracker.Poll(Registry(), now, out, max);
    }

    TestResultChangeTracker mTracker;
};

} // namespace

TEST_F(TestResultChangeTrackerTest, RunningMapsToNoneRunning)
{
    const Dia::Core::StringCRC stage("StageA");
    Registry().SetRunning(stage, 100);

    StageResultEvent ev[4];
    ASSERT_EQ(Poll(ev, 4, 1234), 1u);
    EXPECT_EQ(ev[0].stage, stage);
    EXPECT_EQ(ev[0].result.severity, StageResultSeverity::kNone);
    EXPECT_STREQ(ev[0].result.label.AsCStr(), "RUNNING");
    EXPECT_EQ(ev[0].result.utcMs, 1234u);
    EXPECT_EQ(ev[0].result.sessionId.Length(), 0u);
    EXPECT_EQ(ev[0].result.logPath.Length(), 0u);
}

TEST_F(TestResultChangeTrackerTest, PassedMapsToOkPass)
{
    const Dia::Core::StringCRC stage("StageA");
    Registry().SetRunning(stage, 100);
    Registry().SetPassed(stage, 5);

    StageResultEvent ev[4];
    ASSERT_EQ(Poll(ev, 4), 1u);
    EXPECT_EQ(ev[0].result.severity, StageResultSeverity::kOk);
    EXPECT_STREQ(ev[0].result.label.AsCStr(), "PASS");
}

TEST_F(TestResultChangeTrackerTest, FailedMapsToFailFail)
{
    const Dia::Core::StringCRC stage("StageA");
    Registry().SetRunning(stage, 100);
    Registry().SetFailed(stage, 5);

    StageResultEvent ev[4];
    ASSERT_EQ(Poll(ev, 4), 1u);
    EXPECT_EQ(ev[0].result.severity, StageResultSeverity::kFail);
    EXPECT_STREQ(ev[0].result.label.AsCStr(), "FAIL");
}

TEST_F(TestResultChangeTrackerTest, TimeoutMapsToFailTimeout)
{
    const Dia::Core::StringCRC stage("StageA");
    Registry().SetRunning(stage, 100);
    Registry().SetTimeout(stage);

    StageResultEvent ev[4];
    ASSERT_EQ(Poll(ev, 4), 1u);
    EXPECT_EQ(ev[0].result.severity, StageResultSeverity::kFail);
    EXPECT_STREQ(ev[0].result.label.AsCStr(), "TIMEOUT");
}

TEST_F(TestResultChangeTrackerTest, NoResendWhenUnchanged)
{
    Registry().SetRunning(Dia::Core::StringCRC("StageA"), 100);

    StageResultEvent ev[4];
    EXPECT_EQ(Poll(ev, 4), 1u);
    EXPECT_EQ(Poll(ev, 4), 0u);
    EXPECT_EQ(Poll(ev, 4), 0u);
}

TEST_F(TestResultChangeTrackerTest, ResendsWhenStateChanges)
{
    const Dia::Core::StringCRC stage("StageA");
    Registry().SetRunning(stage, 100);

    StageResultEvent ev[4];
    ASSERT_EQ(Poll(ev, 4), 1u);
    EXPECT_STREQ(ev[0].result.label.AsCStr(), "RUNNING");

    Registry().SetPassed(stage, 7);
    ASSERT_EQ(Poll(ev, 4), 1u);
    EXPECT_STREQ(ev[0].result.label.AsCStr(), "PASS");
    EXPECT_EQ(Poll(ev, 4), 0u);
}

TEST_F(TestResultChangeTrackerTest, MultipleStagesTrackedIndependently)
{
    const Dia::Core::StringCRC a("StageA");
    const Dia::Core::StringCRC b("StageB");
    Registry().SetRunning(a, 100);
    Registry().SetRunning(b, 100);

    StageResultEvent ev[4];
    EXPECT_EQ(Poll(ev, 4), 2u);
    EXPECT_EQ(Poll(ev, 4), 0u);

    Registry().SetFailed(b, 3);
    ASSERT_EQ(Poll(ev, 4), 1u);
    EXPECT_EQ(ev[0].stage, b);
    EXPECT_STREQ(ev[0].result.label.AsCStr(), "FAIL");
    EXPECT_EQ(Poll(ev, 4), 0u);
}

TEST_F(TestResultChangeTrackerTest, NotRunStageSendsNothing)
{
    StageResultEvent ev[4];
    EXPECT_EQ(Poll(ev, 4), 0u);
}

TEST_F(TestResultChangeTrackerTest, StoppedAtCapacityResumesNextPoll)
{
    Registry().SetRunning(Dia::Core::StringCRC("StageA"), 100);
    Registry().SetRunning(Dia::Core::StringCRC("StageB"), 100);

    StageResultEvent ev[4];
    EXPECT_EQ(Poll(ev, 1), 1u);
    EXPECT_EQ(Poll(ev, 4), 1u);
    EXPECT_EQ(Poll(ev, 4), 0u);
}
