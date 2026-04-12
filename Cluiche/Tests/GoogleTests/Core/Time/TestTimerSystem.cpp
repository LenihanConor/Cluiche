#include <gtest/gtest.h>
#include <DiaCore/Timer/TimerSystem.h>
#include <DiaCore/Time/TimeRelative.h>

using namespace Dia::Core;

TEST(TimerSystem, DefaultConstruction_IsNotRunning)
{
    TimerSystem timer;

    EXPECT_FALSE(timer.IsRunning());
    EXPECT_EQ(timer.IsRunningFor(), TimeRelative::Zero());
}

TEST(TimerSystem, Stop_WithoutStart_Asserts)
{
    TimerSystem timer;

    EXPECT_DEATH({ timer.Stop(); }, "");
}

TEST(TimerSystem, Start_ThenStop_IsNotRunning)
{
    TimerSystem timer;

    timer.Start();
    timer.Stop();

    EXPECT_FALSE(timer.IsRunning());
    EXPECT_EQ(timer.IsRunningFor(), TimeRelative::Zero());
}

TEST(TimerSystem, Reset_SetsStartTime)
{
    TimerSystem timer1;
    TimerSystem timer2;

    timer2.Reset();

    EXPECT_EQ(timer1.StartTime(), timer2.StartTime());
}

TEST(TimerSystem, Start_MarksAsRunning)
{
    TimerSystem timer;

    timer.Start();

    EXPECT_TRUE(timer.IsRunning());
}

TEST(TimerSystem, Stop_AfterStart_MarksAsNotRunning)
{
    TimerSystem timer;

    timer.Start();

    // Simulate some frames passing
    for (int i = 0; i < 8; i++)
    {
        EXPECT_TRUE(timer.IsRunning());
    }

    timer.Stop();

    EXPECT_FALSE(timer.IsRunning());
}
