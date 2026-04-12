#include <gtest/gtest.h>
#include <DiaCore/Timer/TimerExpiry.h>
#include <DiaCore/Time/TimeServer.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>

using namespace Dia::Core;

TEST(TimerExpiry, DefaultConstruction_IsNotRunning)
{
    TimerExpiry timer;

    EXPECT_FALSE(timer.IsRunning(TimeAbsolute::Zero()));
}

TEST(TimerExpiry, Start_ThenReset_StopsTimer)
{
    TimerExpiry timer;

    timer.Start(TimeAbsolute::Zero(), TimeRelative::CreateFromSeconds(3.0f));

    EXPECT_TRUE(timer.IsRunning(TimeAbsolute::Zero()));

    timer.Reset();

    EXPECT_FALSE(timer.IsRunning(TimeAbsolute::Zero()));
}

TEST(TimerExpiry, StartWithExpiryTime_RunsUntilExpiry)
{
    TimerExpiry timer;

    timer.Start(TimeAbsolute::Zero() + TimeRelative::CreateFromSeconds(0.1f));

    EXPECT_TRUE(timer.IsRunning(TimeAbsolute::Zero()));

    timer.Reset();

    EXPECT_FALSE(timer.IsRunning(TimeAbsolute::Zero()));
}

TEST(TimerExpiry, TickUntilExpiry_TimerStops)
{
    TimerExpiry timer;
    TimeServer time(100.0f, TimeAbsolute::Zero());

    TimeAbsolute expireAt = time.GetTime() + TimeRelative::CreateFromSeconds(0.1f);
    timer.Start(expireAt);

    time.Tick();

    EXPECT_TRUE(timer.IsRunning(time.GetTime()));
    EXPECT_EQ(timer.TimeUntilExpiry(time.GetTime()), expireAt - time.GetTime());
}

TEST(TimerExpiry, TimeSinceExpiry_BeforeExpiry_Asserts)
{
    TimerExpiry timer;
    TimeServer time(100.0f, TimeAbsolute::Zero());

    TimeAbsolute expireAt = time.GetTime() + TimeRelative::CreateFromSeconds(0.1f);
    timer.Start(expireAt);

    time.Tick();

    EXPECT_DEATH({ timer.TimeSinceExpiry(time.GetTime()); }, "");
}

TEST(TimerExpiry, TickPastExpiry_TimerStopsRunning)
{
    TimerExpiry timer;
    TimeServer time(100.0f, TimeAbsolute::Zero());

    TimeAbsolute expireAt = time.GetTime() + TimeRelative::CreateFromSeconds(0.1f);
    timer.Start(expireAt);

    time.Tick();

    for (int i = 0; i < 1000; i++)
    {
        time.Tick();
    }

    EXPECT_FALSE(timer.IsRunning(time.GetTime()));
}
