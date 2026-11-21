// Perf Inspector A4c: PerfExitTimer pure logic.
#include <gtest/gtest.h>
#include <DiaObservation/Profile/PerfExitTimer.h>

using Dia::Observation::Profile::PerfExitTimer;

namespace {
    constexpr int64_t kSec = 1000000000LL;
}

TEST(PerfExitTimer, DefaultIsDisarmedAndNeverFires)
{
    PerfExitTimer t;
    EXPECT_FALSE(t.IsArmed());
    EXPECT_FALSE(t.ShouldExit(1000 * kSec));
}

TEST(PerfExitTimer, ZeroSecondsStaysDisarmed)
{
    PerfExitTimer t;
    t.Arm(0, 5 * kSec);
    EXPECT_FALSE(t.IsArmed());
    EXPECT_FALSE(t.ShouldExit(100 * kSec));
}

TEST(PerfExitTimer, FiresAtDeadlineNotBefore)
{
    PerfExitTimer t;
    t.Arm(1, 10 * kSec);
    EXPECT_TRUE(t.IsArmed());
    EXPECT_FALSE(t.ShouldExit(10 * kSec));
    EXPECT_FALSE(t.ShouldExit(11 * kSec - 1));
    EXPECT_TRUE(t.ShouldExit(11 * kSec));
}

TEST(PerfExitTimer, FiresOnlyOnce)
{
    PerfExitTimer t;
    t.Arm(2, 0);
    EXPECT_TRUE(t.ShouldExit(5 * kSec));
    EXPECT_FALSE(t.ShouldExit(6 * kSec));
}

TEST(PerfExitTimer, RearmResetsLatch)
{
    PerfExitTimer t;
    t.Arm(1, 0);
    EXPECT_TRUE(t.ShouldExit(2 * kSec));
    t.Arm(1, 10 * kSec);
    EXPECT_FALSE(t.ShouldExit(10 * kSec));
    EXPECT_TRUE(t.ShouldExit(11 * kSec));
}
