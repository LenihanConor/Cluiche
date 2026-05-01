// TestTimer.cpp - Google Test unit tests for Timer
//
// Tests Timer from DiaCore Time subsystem

#include <gtest/gtest.h>
#include <DiaCore/Timer/Timer.h>
#include <DiaCore/Time/TimeServer.h>

using namespace Dia::Core;

// ==============================================================================
// Timer Basic Tests
// ==============================================================================

TEST(Timer, DefaultConstruction_NotRunning)
{
    Timer timer;

    EXPECT_FALSE(timer.IsRunning());
    EXPECT_EQ(timer.IsRunningFor(TimeAbsolute::Zero()), TimeRelative::Zero());
}

TEST(Timer, StopWithoutStart_Asserts)
{
    Timer timer;

    // Stopping a timer that hasn't been started should assert
    EXPECT_DEATH(timer.Stop(TimeAbsolute::Zero()), ".*");
}

TEST(Timer, StartStop_TimerNotRunningAfterStop)
{
    Timer timer;

    timer.Start(TimeAbsolute::Zero());
    timer.Stop(TimeAbsolute::Zero());

    EXPECT_FALSE(timer.IsRunning());
    EXPECT_EQ(timer.IsRunningFor(TimeAbsolute::Zero()), TimeRelative::Zero());
}

TEST(Timer, Reset_SetsStartTimeToZero)
{
    Timer timer1;
    Timer timer2;

    timer2.Reset();

    EXPECT_EQ(timer1.StartTime(), timer2.StartTime());
}

// ==============================================================================
// Timer with TimeServer Tests
// ==============================================================================

TEST(Timer, StartWithTimeServer_IsRunning)
{
    Timer timer;
    TimeServer time(60.0f, TimeAbsolute::Zero());

    timer.Start(time.GetTime());
    time.Tick();

    EXPECT_TRUE(timer.IsRunning());
    EXPECT_EQ(timer.IsRunningFor(time.GetTime()), time.GetStep());
}

TEST(Timer, StopAfterTwoTicks_RecordsElapsedTime)
{
    Timer timer;
    TimeServer time(60.0f, TimeAbsolute::Zero());

    timer.Start(time.GetTime());
    time.Tick();
    time.Tick();
    timer.Stop(time.GetTime());

    EXPECT_FALSE(timer.IsRunning());
    EXPECT_EQ(timer.IsRunningFor(time.GetTime()), time.GetStep() * 2.0f);
}

TEST(Timer, ResetAfterStop_ClearsElapsedTime)
{
    Timer timer;
    TimeServer time(60.0f, TimeAbsolute::Zero());

    timer.Start(time.GetTime());
    time.Tick();
    timer.Stop(time.GetTime());
    timer.Reset();

    EXPECT_FALSE(timer.IsRunning());
    EXPECT_EQ(timer.IsRunningFor(time.GetTime()), TimeRelative::Zero());
}

// ==============================================================================
// Timer State Tests
// ==============================================================================

TEST(Timer, StopAndQuery_PreservesElapsedTime)
{
    Timer timer;
    TimeServer time(60.0f, TimeAbsolute::Zero());

    timer.Start(time.GetTime());
    time.Tick();
    timer.Stop(time.GetTime());

    TimeRelative stoppedDuration = timer.IsRunningFor(time.GetTime());

    // Querying again should return same value
    time.Tick();
    TimeRelative queriedAgain = timer.IsRunningFor(time.GetTime());

    EXPECT_EQ(stoppedDuration, queriedAgain);
}

TEST(Timer, IsRunningFor_WhileRunning_UpdatesDynamically)
{
    Timer timer;
    TimeServer time(60.0f, TimeAbsolute::Zero());

    timer.Start(time.GetTime());

    time.Tick();
    TimeRelative afterOneTick = timer.IsRunningFor(time.GetTime());

    time.Tick();
    TimeRelative afterTwoTicks = timer.IsRunningFor(time.GetTime());

    EXPECT_TRUE(timer.IsRunning());
    EXPECT_EQ(afterOneTick, time.GetStep());
    EXPECT_EQ(afterTwoTicks, time.GetStep() * 2.0f);
}

TEST(Timer, StartTime_ReflectsInitialStartTime)
{
    Timer timer;
    TimeServer time(60.0f, TimeAbsolute::Zero());

    time.Tick();
    time.Tick();

    timer.Start(time.GetTime());

    EXPECT_EQ(timer.StartTime(), time.GetTime());
}

// ==============================================================================
// Timer Edge Cases
// ==============================================================================

TEST(Timer, StartStopImmediate_ZeroElapsedTime)
{
    Timer timer;
    TimeAbsolute now = TimeAbsolute::Zero();

    timer.Start(now);
    timer.Stop(now);

    EXPECT_FALSE(timer.IsRunning());
    EXPECT_EQ(timer.IsRunningFor(now), TimeRelative::Zero());
}

TEST(Timer, ResetWhileRunning_StopsAndClears)
{
    Timer timer;
    TimeServer time(60.0f, TimeAbsolute::Zero());

    timer.Start(time.GetTime());
    time.Tick();

    EXPECT_TRUE(timer.IsRunning());

    timer.Reset();

    EXPECT_FALSE(timer.IsRunning());
    EXPECT_EQ(timer.IsRunningFor(time.GetTime()), TimeRelative::Zero());
}

TEST(Timer, MultipleResets_IdempotentBehavior)
{
    Timer timer;
    TimeServer time(60.0f, TimeAbsolute::Zero());

    timer.Start(time.GetTime());
    time.Tick();
    timer.Stop(time.GetTime());

    timer.Reset();
    timer.Reset();
    timer.Reset();

    EXPECT_FALSE(timer.IsRunning());
    EXPECT_EQ(timer.IsRunningFor(time.GetTime()), TimeRelative::Zero());
}
