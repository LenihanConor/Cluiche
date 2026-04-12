// TestTimeServer.cpp - Google Test unit tests for TimeServer
//
// Tests TimeServer from DiaCore Time subsystem

#include <gtest/gtest.h>
#include <DiaCore/Time/TimeServer.h>

using namespace Dia::Core;

// ==============================================================================
// TimeServer Construction Tests
// ==============================================================================

TEST(TimeServer, ConstructWith60FPS_InitializedCorrectly)
{
    TimeServer timeServer(60.0f, TimeAbsolute::Zero());

    EXPECT_EQ(timeServer.GetTime(), TimeAbsolute::Zero());
    EXPECT_EQ(timeServer.GetTime(), timeServer.GetLastTime());
    EXPECT_EQ(timeServer.GetStep(), TimeRelative::CreateFromSeconds(1.0f / 60.0f));
    EXPECT_EQ(timeServer.GetLastStep(), TimeRelative::Zero());
    EXPECT_EQ(timeServer.GetNextStep(), timeServer.GetStep());
    EXPECT_EQ(timeServer.GetTick(), 0u);
    EXPECT_FLOAT_EQ(timeServer.GetTimeScale(), 1.0f);
}

TEST(TimeServer, DefaultConstruction_TickBeforeCreate_Asserts)
{
    TimeServer timeServer;

    // Ticking before Create() should assert
    EXPECT_DEATH(timeServer.Tick(), ".*");
}

// ==============================================================================
// TimeServer Create Tests
// ==============================================================================

TEST(TimeServer, Create_MatchesConstructor)
{
    TimeServer timeServer1;
    TimeServer timeServer2(60.0f, TimeAbsolute::Zero());

    timeServer1.Create(60.0f, TimeAbsolute::Zero());

    EXPECT_EQ(timeServer1.GetTime(), timeServer2.GetTime());
    EXPECT_EQ(timeServer1.GetLastTime(), timeServer2.GetLastTime());
    EXPECT_EQ(timeServer1.GetStep(), timeServer2.GetStep());
    EXPECT_EQ(timeServer1.GetLastStep(), timeServer2.GetLastStep());
    EXPECT_EQ(timeServer1.GetNextStep(), timeServer2.GetNextStep());
    EXPECT_EQ(timeServer1.GetTick(), timeServer2.GetTick());
    EXPECT_FLOAT_EQ(timeServer1.GetTimeScale(), timeServer2.GetTimeScale());
}

// ==============================================================================
// TimeServer Assignment Tests
// ==============================================================================

TEST(TimeServer, AssignmentOperator_CopiesState)
{
    TimeServer timeServer1;
    TimeServer timeServer2(60.0f, TimeAbsolute::Zero());

    timeServer1 = timeServer2;

    EXPECT_EQ(timeServer1.GetTime(), timeServer2.GetTime());
    EXPECT_EQ(timeServer1.GetLastTime(), timeServer2.GetLastTime());
    EXPECT_EQ(timeServer1.GetStep(), timeServer2.GetStep());
    EXPECT_EQ(timeServer1.GetLastStep(), timeServer2.GetLastStep());
    EXPECT_EQ(timeServer1.GetNextStep(), timeServer2.GetNextStep());
    EXPECT_EQ(timeServer1.GetTick(), timeServer2.GetTick());
    EXPECT_FLOAT_EQ(timeServer1.GetTimeScale(), timeServer2.GetTimeScale());
}

TEST(TimeServer, CopyConstructor_CopiesState)
{
    TimeServer timeServer2(60.0f, TimeAbsolute::Zero());
    TimeServer timeServer1(timeServer2);

    EXPECT_EQ(timeServer1.GetTime(), timeServer2.GetTime());
    EXPECT_EQ(timeServer1.GetLastTime(), timeServer2.GetLastTime());
    EXPECT_EQ(timeServer1.GetStep(), timeServer2.GetStep());
    EXPECT_EQ(timeServer1.GetLastStep(), timeServer2.GetLastStep());
    EXPECT_EQ(timeServer1.GetNextStep(), timeServer2.GetNextStep());
    EXPECT_EQ(timeServer1.GetTick(), timeServer2.GetTick());
    EXPECT_FLOAT_EQ(timeServer1.GetTimeScale(), timeServer2.GetTimeScale());
}

// ==============================================================================
// TimeServer Tick Tests
// ==============================================================================

TEST(TimeServer, Tick_AdvancesTimeByOneStep)
{
    TimeServer timeServer(60.0f, TimeAbsolute::Zero());

    timeServer.Tick();

    EXPECT_EQ(timeServer.GetTime(), TimeAbsolute::Zero() + timeServer.GetStep());
    EXPECT_EQ(timeServer.GetLastTime(), TimeAbsolute::Zero());
    EXPECT_EQ(timeServer.GetTick(), 1u);
}

TEST(TimeServer, Tick_MultipleTicks_AdvancesTime)
{
    TimeServer timeServer(60.0f, TimeAbsolute::Zero());

    timeServer.Tick();
    timeServer.Tick();
    timeServer.Tick();

    TimeRelative expectedTime = timeServer.GetStep() * 3.0f;
    EXPECT_EQ(timeServer.GetTime(), TimeAbsolute::Zero() + expectedTime);
    EXPECT_EQ(timeServer.GetTick(), 3u);
}

// ==============================================================================
// TimeServer Reset Tests
// ==============================================================================

TEST(TimeServer, Reset_ResetsToInitialState)
{
    TimeServer timeServer(60.0f, TimeAbsolute::Zero());

    timeServer.Tick();
    timeServer.Reset();

    EXPECT_EQ(timeServer.GetTime(), TimeAbsolute::Zero());
    EXPECT_EQ(timeServer.GetLastTime(), TimeAbsolute::Zero());
    EXPECT_EQ(timeServer.GetTick(), 0u);
}

TEST(TimeServer, Reset_AfterMultipleTicks_ResetsCorrectly)
{
    TimeServer timeServer(60.0f, TimeAbsolute::Zero());

    timeServer.Tick();
    timeServer.Tick();
    timeServer.Tick();
    timeServer.Reset();

    EXPECT_EQ(timeServer.GetTime(), TimeAbsolute::Zero());
    EXPECT_EQ(timeServer.GetLastTime(), TimeAbsolute::Zero());
    EXPECT_EQ(timeServer.GetTick(), 0u);
}

// ==============================================================================
// TimeServer TimeScale Tests - SetTimeScale
// ==============================================================================

TEST(TimeServer, SetTimeScale_HalfSpeed_SlowsTime)
{
    TimeServer timeServer(60.0f, TimeAbsolute::Zero());

    timeServer.SetTimeScale(0.5f);
    timeServer.Tick();
    timeServer.Tick();

    // First tick: full speed (scale applied after first tick)
    // Second tick: half speed
    TimeAbsolute expectedTime = TimeAbsolute::Zero() + timeServer.GetStep() + (timeServer.GetStep() * 0.5f);
    EXPECT_EQ(timeServer.GetTime(), expectedTime);
    EXPECT_EQ(timeServer.GetLastTime(), TimeAbsolute::Zero() + timeServer.GetStep());
    EXPECT_EQ(timeServer.GetTick(), 2u);
}

TEST(TimeServer, SetTimeScale_DoubleSpeed_SpeedsUpTime)
{
    TimeServer timeServer(60.0f, TimeAbsolute::Zero());

    timeServer.SetTimeScale(2.0f);
    timeServer.Tick();
    timeServer.Tick();

    TimeAbsolute expectedTime = TimeAbsolute::Zero() + timeServer.GetStep() + (timeServer.GetStep() * 2.0f);
    EXPECT_EQ(timeServer.GetTime(), expectedTime);
    EXPECT_EQ(timeServer.GetTick(), 2u);
}

TEST(TimeServer, SetTimeScale_ZeroSpeed_PausesTime)
{
    TimeServer timeServer(60.0f, TimeAbsolute::Zero());

    timeServer.Tick();
    TimeAbsolute timeAfterFirstTick = timeServer.GetTime();

    timeServer.SetTimeScale(0.0f);
    timeServer.Tick();  // First tick after SetTimeScale still advances (queued behavior)
    timeServer.Tick();  // Second tick is paused
    timeServer.Tick();  // Third tick is paused

    // Scale change takes effect after one tick (queued behavior)
    // So time advances by one more step, then pauses
    TimeAbsolute expectedTime = timeAfterFirstTick + timeServer.GetStep();
    EXPECT_EQ(timeServer.GetTime(), expectedTime);
    EXPECT_EQ(timeServer.GetTick(), 4u);  // Tick count still increases
}

// ==============================================================================
// TimeServer TimeScale Tests - AdjustTimeScale
// ==============================================================================

TEST(TimeServer, AdjustTimeScale_AddsToCurrentScale)
{
    TimeServer timeServer(60.0f, TimeAbsolute::Zero());

    // Initial scale is 1.0, adjust by +0.5 = 1.5
    timeServer.AdjustTimeScale(0.5f);
    timeServer.Tick();
    timeServer.Tick();

    // First tick: full speed
    // Second tick: 1.5x speed
    TimeAbsolute expectedTime = TimeAbsolute::Zero() + timeServer.GetStep() + (timeServer.GetStep() * 1.5f);
    EXPECT_EQ(timeServer.GetTime(), expectedTime);
    EXPECT_EQ(timeServer.GetLastTime(), TimeAbsolute::Zero() + timeServer.GetStep());
    EXPECT_EQ(timeServer.GetTick(), 2u);
}

TEST(TimeServer, AdjustTimeScale_NegativeAdjustment_ReducesScale)
{
    TimeServer timeServer(60.0f, TimeAbsolute::Zero());

    // Initial scale is 1.0, adjust by -0.5 = 0.5
    timeServer.AdjustTimeScale(-0.5f);
    timeServer.Tick();
    timeServer.Tick();

    TimeAbsolute expectedTime = TimeAbsolute::Zero() + timeServer.GetStep() + (timeServer.GetStep() * 0.5f);
    EXPECT_EQ(timeServer.GetTime(), expectedTime);
    EXPECT_EQ(timeServer.GetTick(), 2u);
}

// ==============================================================================
// TimeServer GetTimeScale Tests
// ==============================================================================

TEST(TimeServer, GetTimeScale_InitiallyOne)
{
    TimeServer timeServer(60.0f, TimeAbsolute::Zero());

    EXPECT_FLOAT_EQ(timeServer.GetTimeScale(), 1.0f);
}

TEST(TimeServer, GetTimeScale_AfterSetTimeScale_ReturnsNewScale)
{
    TimeServer timeServer(60.0f, TimeAbsolute::Zero());

    timeServer.SetTimeScale(2.5f);

    EXPECT_FLOAT_EQ(timeServer.GetTimeScale(), 2.5f);
}

TEST(TimeServer, GetTimeScale_AfterAdjustTimeScale_ReturnsAdjustedScale)
{
    TimeServer timeServer(60.0f, TimeAbsolute::Zero());

    timeServer.AdjustTimeScale(0.5f);

    EXPECT_FLOAT_EQ(timeServer.GetTimeScale(), 1.5f);
}

// ==============================================================================
// TimeServer Different Framerates Tests
// ==============================================================================

TEST(TimeServer, Construct30FPS_CorrectStep)
{
    TimeServer timeServer(30.0f, TimeAbsolute::Zero());

    EXPECT_EQ(timeServer.GetStep(), TimeRelative::CreateFromSeconds(1.0f / 30.0f));
}

TEST(TimeServer, Construct120FPS_CorrectStep)
{
    TimeServer timeServer(120.0f, TimeAbsolute::Zero());

    EXPECT_EQ(timeServer.GetStep(), TimeRelative::CreateFromSeconds(1.0f / 120.0f));
}

// ==============================================================================
// TimeServer Edge Cases
// ==============================================================================

TEST(TimeServer, ConstructWithNonZeroStartTime_StartsAtSpecifiedTime)
{
    TimeAbsolute startTime = TimeAbsolute::CreateFromSeconds(100.0f);
    TimeServer timeServer(60.0f, startTime);

    EXPECT_EQ(timeServer.GetTime(), startTime);
    EXPECT_EQ(timeServer.GetLastTime(), startTime);
}

TEST(TimeServer, Tick_AfterNonZeroStartTime_AdvancesCorrectly)
{
    TimeAbsolute startTime = TimeAbsolute::CreateFromSeconds(100.0f);
    TimeServer timeServer(60.0f, startTime);

    timeServer.Tick();

    EXPECT_EQ(timeServer.GetTime(), startTime + timeServer.GetStep());
    EXPECT_EQ(timeServer.GetLastTime(), startTime);
}
