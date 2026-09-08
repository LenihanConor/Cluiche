// TestSimTimeContext.cpp - Google Test unit tests for SimTimeContext
//
// Tests SimTimeContext / RenderTimeContext / MainTimeContext from DiaCore SimTime subsystem

#include <gtest/gtest.h>
#include <DiaCore/SimTime/SimTimeContext.h>

using namespace Dia::Core;
using namespace Dia::SimTime;

TEST(SimTimeContext, BraceInitialized_FieldsAreZero)
{
    // TimeAbsolute/TimeRelative expose only a private default constructor
    // (factory-only construction — see TimeAbsolute::Zero()/CreateFrom*()),
    // so SimTimeContext ctx{} cannot value-initialize gameTime/gameDt from
    // outside the Time classes' friend circle (MSVC C2512). Supply those two
    // class-typed members explicitly via Zero(); the remaining POD members
    // (tick/timeScale/isPaused) are left to the aggregate's implicit
    // value-initialization to prove they zero out correctly.
    SimTimeContext ctx{ TimeAbsolute::Zero(), TimeRelative::Zero() };
    EXPECT_EQ(ctx.gameTime, TimeAbsolute::Zero());
    EXPECT_EQ(ctx.gameDt, TimeRelative::Zero());
    EXPECT_EQ(ctx.tick, 0u);
    EXPECT_FLOAT_EQ(ctx.timeScale, 0.0f);
    EXPECT_FALSE(ctx.isPaused);
}

TEST(SimTimeContext, FieldAssignment_RoundTrips)
{
    SimTimeContext ctx{ TimeAbsolute::Zero(), TimeRelative::Zero() };
    ctx.gameTime  = TimeAbsolute::CreateFromSeconds(5.0f);
    ctx.gameDt    = TimeRelative::CreateFromSeconds(1.0f / 60.0f);
    ctx.tick      = 42u;
    ctx.timeScale = 0.5f;
    ctx.isPaused  = true;

    EXPECT_EQ(ctx.gameTime, TimeAbsolute::CreateFromSeconds(5.0f));
    EXPECT_EQ(ctx.gameDt, TimeRelative::CreateFromSeconds(1.0f / 60.0f));
    EXPECT_EQ(ctx.tick, 42u);
    EXPECT_FLOAT_EQ(ctx.timeScale, 0.5f);
    EXPECT_TRUE(ctx.isPaused);
}

TEST(RenderTimeContext, BraceInitialized_FieldsAreZero)
{
    // Same private-default-constructor constraint as SimTimeContext above:
    // simTime (TimeAbsolute) must be supplied explicitly; renderFrame (uint64_t)
    // is left to implicit value-initialization to prove it zeros out.
    RenderTimeContext ctx{ 0.0f, TimeAbsolute::Zero() };
    EXPECT_FLOAT_EQ(ctx.frameDt, 0.0f);
    EXPECT_EQ(ctx.simTime, TimeAbsolute::Zero());
    EXPECT_EQ(ctx.renderFrame, 0u);
}

TEST(MainTimeContext, BraceInitialized_FieldIsZero)
{
    MainTimeContext ctx{};
    EXPECT_FLOAT_EQ(ctx.wallClockDt, 0.0f);
}
