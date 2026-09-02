// TestSimTimeDomain.cpp - Google Test unit tests for SimTimeDomain
//
// Tests SimTimeDomain from DiaCore SimTime subsystem. SimTimeDomain is a thin
// wrapper that composes Dia::Core::TimeServer, so behaviour (including the
// one-tick-latency of SetScale, inherited from TimeServer::SetTimeScale) mirrors
// TestTimeServer.cpp.

#include <gtest/gtest.h>
#include <DiaCore/SimTime/SimTimeDomain.h>

using namespace Dia::Core;
using namespace Dia::SimTime;

// ==============================================================================
// Construction
// ==============================================================================

TEST(SimTimeDomain, Construct_InitialState)
{
    StringCRC id("world");
    SimTimeDomain domain(id, 60.0f, TimeAbsolute::Zero());

    EXPECT_EQ(domain.Now(), TimeAbsolute::Zero());
    EXPECT_EQ(domain.GetId(), id);
    EXPECT_FLOAT_EQ(domain.GetScale(), 1.0f);
    EXPECT_FALSE(domain.IsPaused());
    EXPECT_EQ(domain.GetTick(), 0u);
    EXPECT_EQ(domain.Step(), TimeRelative::CreateFromSeconds(1.0f / 60.0f));
}

TEST(SimTimeDomain, Construct_NonZeroStartTime)
{
    TimeAbsolute startTime = TimeAbsolute::CreateFromSeconds(100.0f);
    SimTimeDomain domain(StringCRC("world"), 60.0f, startTime);

    EXPECT_EQ(domain.Now(), startTime);
}

// ==============================================================================
// Tick
// ==============================================================================

TEST(SimTimeDomain, Tick_AdvancesByStepAndIncrementsTick)
{
    SimTimeDomain domain(StringCRC("world"), 60.0f, TimeAbsolute::Zero());

    domain.Tick();

    EXPECT_EQ(domain.Now(), TimeAbsolute::Zero() + domain.Step());
    EXPECT_EQ(domain.GetTick(), 1u);

    domain.Tick();
    domain.Tick();

    EXPECT_EQ(domain.Now(), TimeAbsolute::Zero() + (domain.Step() * 3.0f));
    EXPECT_EQ(domain.GetTick(), 3u);
}

// ==============================================================================
// Pause / Resume
// ==============================================================================

TEST(SimTimeDomain, Pause_FreezesTick)
{
    SimTimeDomain domain(StringCRC("world"), 60.0f, TimeAbsolute::Zero());

    domain.Pause();
    EXPECT_TRUE(domain.IsPaused());

    domain.Tick();
    domain.Tick();

    EXPECT_EQ(domain.Now(), TimeAbsolute::Zero());
    EXPECT_EQ(domain.GetTick(), 0u);
}

TEST(SimTimeDomain, Resume_RestoresTick)
{
    SimTimeDomain domain(StringCRC("world"), 60.0f, TimeAbsolute::Zero());

    domain.Pause();
    domain.Tick();          // no-op while paused
    domain.Resume();
    EXPECT_FALSE(domain.IsPaused());

    domain.Tick();

    EXPECT_EQ(domain.Now(), TimeAbsolute::Zero() + domain.Step());
    EXPECT_EQ(domain.GetTick(), 1u);
}

// ==============================================================================
// Step (manual single-step advance, bypasses pause)
// ==============================================================================

TEST(SimTimeDomain, Step_WhilePaused_AdvancesExactlyOnce)
{
    SimTimeDomain domain(StringCRC("world"), 60.0f, TimeAbsolute::Zero());

    domain.Pause();
    TimeRelative step = TimeRelative::CreateFromSeconds(0.25f);
    domain.Step(step);

    EXPECT_EQ(domain.Now(), TimeAbsolute::Zero() + step);
    EXPECT_EQ(domain.GetTick(), 1u);

    // Still paused - a subsequent Tick() must not advance further
    domain.Tick();
    EXPECT_EQ(domain.Now(), TimeAbsolute::Zero() + step);
    EXPECT_EQ(domain.GetTick(), 1u);
}

// ==============================================================================
// AdvanceTo
// ==============================================================================

TEST(SimTimeDomain, AdvanceTo_LaterTarget_JumpsForward)
{
    SimTimeDomain domain(StringCRC("world"), 60.0f, TimeAbsolute::Zero());

    TimeAbsolute target = TimeAbsolute::CreateFromSeconds(5.0f);
    domain.AdvanceTo(target);

    EXPECT_EQ(domain.Now(), target);
    EXPECT_EQ(domain.GetTick(), 1u);
}

TEST(SimTimeDomain, AdvanceTo_EqualTarget_IsNoOp)
{
    SimTimeDomain domain(StringCRC("world"), 60.0f, TimeAbsolute::Zero());

    domain.AdvanceTo(TimeAbsolute::Zero());

    EXPECT_EQ(domain.Now(), TimeAbsolute::Zero());
    EXPECT_EQ(domain.GetTick(), 0u);
}

TEST(SimTimeDomain, AdvanceTo_EarlierTarget_IsNoOp)
{
    TimeAbsolute startTime = TimeAbsolute::CreateFromSeconds(10.0f);
    SimTimeDomain domain(StringCRC("world"), 60.0f, startTime);

    domain.AdvanceTo(TimeAbsolute::CreateFromSeconds(5.0f));

    EXPECT_EQ(domain.Now(), startTime);
    EXPECT_EQ(domain.GetTick(), 0u);
}

// ==============================================================================
// SetScale (one-tick-latency inherited from TimeServer::SetTimeScale)
// ==============================================================================

TEST(SimTimeDomain, SetScale_UpdatesReportedScaleImmediately)
{
    SimTimeDomain domain(StringCRC("world"), 60.0f, TimeAbsolute::Zero());

    domain.SetScale(0.5f);

    EXPECT_FLOAT_EQ(domain.GetScale(), 0.5f);
}

TEST(SimTimeDomain, SetScale_HalfSpeed_AffectsStepAfterOneTick)
{
    SimTimeDomain domain(StringCRC("world"), 60.0f, TimeAbsolute::Zero());

    domain.SetScale(0.5f);
    domain.Tick();  // scale change takes effect after one tick (queued behavior)
    domain.Tick();

    // First tick: full speed; second tick: half speed
    TimeAbsolute expected = TimeAbsolute::Zero() + domain.Step() + (domain.Step() * 0.5f);
    EXPECT_EQ(domain.Now(), expected);
    EXPECT_EQ(domain.GetTick(), 2u);
}
